#ifndef D_IORING_DISK_WRITER_H
#define D_IORING_DISK_WRITER_H

#include "AbstractDiskWriter.h"
#include <liburing.h>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace aria2 {

class IORingDiskWriter : public AbstractDiskWriter {
private:
  static constexpr size_t POOL_SIZE = 200 * 1024 * 1024; // 200MB
  static constexpr size_t IO_BLOCK_SIZE = 4096; // 4KB alignment for O_DIRECT
  static constexpr size_t NUM_BLOCKS = POOL_SIZE / IO_BLOCK_SIZE;

  static struct io_uring ring_;
  static bool ring_initialized_;
  static std::mutex ring_mutex_;
  
  // Singleton-like traits for the shared buffer pool (since we only want one 200MB pool globally)
  // But for this MVP, we allocate it per instance if we only have one file...
  // Usually, a torrent downloads to multiple files. We'll make it static to share.
  static void* shared_buffer_pool_;
  static std::vector<bool> block_used_;
  static std::mutex pool_mutex_;

  int direct_fd_;

  static void initializePool();
  void checkUringCompletions();

public:
  IORingDiskWriter(const std::string& filename);
  virtual ~IORingDiskWriter();

  virtual void initAndOpenFile(int64_t totalLength = 0) CXX11_OVERRIDE;
  virtual void openExistingFile(int64_t totalLength = 0) CXX11_OVERRIDE;
  virtual void writeData(const unsigned char* data, size_t len, int64_t offset) CXX11_OVERRIDE;
  virtual ssize_t readData(unsigned char* data, size_t len, int64_t offset) CXX11_OVERRIDE;
  
  // Override to do nothing or handle differently since O_DIRECT is used
  virtual void allocate(int64_t offset, int64_t length, bool sparse) CXX11_OVERRIDE;
  virtual void closeFile() CXX11_OVERRIDE;
};

} // namespace aria2

#endif // D_IORING_DISK_WRITER_H
