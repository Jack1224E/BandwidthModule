#include "IORingDiskWriter.h"
#include "File.h"
#include "util.h"
#include "message.h"
#include "DlAbortEx.h"
#include "a2io.h"
#include "fmt.h"
#include "DownloadFailureException.h"
#include "error_code.h"
#include "LogFactory.h"

#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <iostream>

namespace aria2 {

void* IORingDiskWriter::shared_buffer_pool_ = nullptr;
std::vector<bool> IORingDiskWriter::block_used_;
std::mutex IORingDiskWriter::pool_mutex_;
struct io_uring IORingDiskWriter::ring_;
bool IORingDiskWriter::ring_initialized_ = false;
std::mutex IORingDiskWriter::ring_mutex_;

void IORingDiskWriter::initializePool() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    if (!shared_buffer_pool_) {
        shared_buffer_pool_ = aligned_alloc(IO_BLOCK_SIZE, POOL_SIZE);
        if (!shared_buffer_pool_) {
            throw DL_ABORT_EX("Failed to allocate 200MB 4KB-aligned buffer pool.");
        }
        std::memset(shared_buffer_pool_, 0, POOL_SIZE);
        block_used_.assign(NUM_BLOCKS, false);
    }
    
    std::lock_guard<std::mutex> rlock(ring_mutex_);
    if (!ring_initialized_) {
        if (io_uring_queue_init(512, &ring_, 0) < 0) {
            throw DL_ABORT_EX("Failed to initialize global io_uring");
        }
        ring_initialized_ = true;
    }
}

IORingDiskWriter::IORingDiskWriter(const std::string& filename)
    : AbstractDiskWriter(filename), direct_fd_(-1) {
    initializePool();
}

IORingDiskWriter::~IORingDiskWriter() { 
    closeFile(); 
}

void IORingDiskWriter::initAndOpenFile(int64_t totalLength) {
    std::string filename = File(filename_).getPath();
    util::mkdirs(File(filename).getDirname());
    
    direct_fd_ = open(filename.c_str(), O_CREAT | O_RDWR | O_TRUNC | O_DIRECT, 0666);
    if (direct_fd_ < 0) {
        int errNum = errno;
        throw DL_ABORT_EX3(errNum, fmt("Failed to open file %s with O_DIRECT: %s", filename.c_str(), util::safeStrerror(errNum).c_str()), error_code::FILE_CREATE_ERROR);
    }
}

void IORingDiskWriter::openExistingFile(int64_t totalLength) {
    std::string filename = File(filename_).getPath();
    direct_fd_ = open(filename.c_str(), O_RDWR | O_DIRECT);
    if (direct_fd_ < 0) {
        int errNum = errno;
        throw DL_ABORT_EX3(errNum, fmt("Failed to open existing file %s with O_DIRECT: %s", filename.c_str(), util::safeStrerror(errNum).c_str()), error_code::FILE_OPEN_ERROR);
    }
}

void IORingDiskWriter::closeFile() {
    checkUringCompletions();
    if (direct_fd_ != -1) {
        close(direct_fd_);
        direct_fd_ = -1;
    }
}

void IORingDiskWriter::checkUringCompletions() {
    std::lock_guard<std::mutex> rlock(ring_mutex_);
    struct io_uring_cqe *cqe;
    unsigned head;
    unsigned count = 0;

    io_uring_for_each_cqe(&ring_, head, cqe) {
        if (cqe->res < 0) {
            std::cerr << "IORing ERROR: cqe->res=" << cqe->res << " (" << util::safeStrerror(-cqe->res) << ")" << std::endl;
        }
        
        size_t block_index = (size_t)cqe->user_data;
        {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            block_used_[block_index] = false;
        }
        count++;
    }
    
    if (count > 0) {
        io_uring_cq_advance(&ring_, count);
    }
}

void IORingDiskWriter::writeData(const unsigned char* data, size_t len, int64_t offset) {
    checkUringCompletions();

    size_t aligned_len = (len + IO_BLOCK_SIZE - 1) & ~(IO_BLOCK_SIZE - 1);
    int64_t aligned_offset = offset & ~(IO_BLOCK_SIZE - 1);
    int64_t offset_diff = offset - aligned_offset;

    size_t block_index = (size_t)-1;
    while (block_index == (size_t)-1) {
        {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            size_t blocks_needed = aligned_len / IO_BLOCK_SIZE;
            for (size_t i = 0; i <= NUM_BLOCKS - blocks_needed; ++i) {
                bool free = true;
                for (size_t j = 0; j < blocks_needed; ++j) {
                    if (block_used_[i + j]) { free = false; break; }
                }
                if (free) {
                    for (size_t j = 0; j < blocks_needed; ++j) block_used_[i + j] = true;
                    block_index = i;
                    break;
                }
            }
        }

        if (block_index == (size_t)-1) {
            // Wait for at least one completion globally
            {
                std::lock_guard<std::mutex> rlock(ring_mutex_);
                io_uring_submit_and_wait(&ring_, 1);
            }
            checkUringCompletions();
        }
    }

    unsigned char* dest = static_cast<unsigned char*>(shared_buffer_pool_) + (block_index * IO_BLOCK_SIZE);
    
    // Clear padding for data integrity
    if (offset_diff > 0) std::memset(dest, 0, offset_diff);
    std::memcpy(dest + offset_diff, data, len);
    if (aligned_len > offset_diff + len) {
        std::memset(dest + offset_diff + len, 0, aligned_len - (offset_diff + len));
    }
    
    {
        std::lock_guard<std::mutex> rlock(ring_mutex_);
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring_);
        if (!sqe) {
            io_uring_submit(&ring_);
            sqe = io_uring_get_sqe(&ring_);
        }
        if (sqe) {
            io_uring_prep_write(sqe, direct_fd_, dest, aligned_len, aligned_offset);
            io_uring_sqe_set_data(sqe, (void*)block_index);
            io_uring_submit(&ring_);
        } else {
            std::cerr << "IORing CRITICAL: Failed to get SQE even after submit" << std::endl;
        }
    }
}

ssize_t IORingDiskWriter::readData(unsigned char* data, size_t len, int64_t offset) {
    size_t aligned_len = (len + IO_BLOCK_SIZE - 1) & ~(IO_BLOCK_SIZE - 1);
    int64_t aligned_offset = offset & ~(IO_BLOCK_SIZE - 1);
    unsigned char* temp_buf = static_cast<unsigned char*>(aligned_alloc(IO_BLOCK_SIZE, aligned_len));
    
    ssize_t ret = pread(direct_fd_, temp_buf, aligned_len, aligned_offset);
    if (ret > 0) {
        int64_t offset_diff = offset - aligned_offset;
        std::memcpy(data, temp_buf + offset_diff, std::min(len, (size_t)(ret - offset_diff)));
    }
    free(temp_buf);
    return ret > 0 ? len : ret;
}

void IORingDiskWriter::allocate(int64_t offset, int64_t length, bool sparse) {}

} // namespace aria2
