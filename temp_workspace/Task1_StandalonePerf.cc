#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <memory>

using namespace std;
using namespace std::chrono;

// --- MOCK BITFIELD (To isolate Task 1 logic without aria2 deps) ---
class MockBitfield {
public:
    MockBitfield(size_t blocks) : blocks_(blocks), bits_(new uint8_t[(blocks + 7) / 8]{}) {}
    ~MockBitfield() { delete[] bits_; }
    void setUseBit(size_t i) { bits_[i/8] |= (1 << (i%8)); }
    bool isUseBitSet(size_t i) const { return bits_[i/8] & (1 << (i%8)); }
private:
    size_t blocks_;
    uint8_t* bits_;
};

// --- Piece Mock (Task 1 Logic) ---
struct Piece {
    mutable std::mutex piece_mutex_;
    mutable std::atomic<uint16_t> requested_count_{0};
    
    struct PieceBlockTracker {
        enum State : uint8_t { FREE = 0, REQUESTED = 1, WRITING = 2, FINISHED = 3 };
        std::vector<std::atomic<uint8_t>> states_;
        PieceBlockTracker(size_t blocks) : states_(blocks) {
            for(auto& s : states_) s.store(FREE);
        }
        bool try_request(size_t idx) {
            uint8_t expected = FREE;
            return states_[idx].compare_exchange_strong(expected, REQUESTED);
        }
        bool try_start_write(size_t idx) {
            uint8_t expected = REQUESTED;
            return states_[idx].compare_exchange_strong(expected, WRITING);
        }
    };
    
    std::unique_ptr<PieceBlockTracker> blockTracker_;
    std::unique_ptr<MockBitfield> bitfield_;
    size_t num_blocks_;

    Piece(size_t blocks) : num_blocks_(blocks) {
        bitfield_ = make_unique<MockBitfield>(blocks);
        blockTracker_ = make_unique<PieceBlockTracker>(blocks);
    }

    bool reserveBlock(size_t i) {
        lock_guard<mutex> lock(piece_mutex_);
        bitfield_->setUseBit(i);
        requested_count_.fetch_add(1);
        return true;
    }

    bool isBlockUsed(size_t i) const {
        lock_guard<mutex> lock(piece_mutex_);
        return bitfield_->isUseBitSet(i);
    }
};

// --- TEST 1: Urgency Benchmark ---
void runUrgencyBenchmark() {
    size_t blocks = 2048;
    Piece piece(blocks);
    for(size_t i=0; i<256; ++i) piece.reserveBlock(i);

    auto atomic_t1 = high_resolution_clock::now();
    for(int i=0; i<100000; ++i) {
        volatile auto v = piece.requested_count_.load(memory_order_relaxed);
    }
    auto atomic_t2 = high_resolution_clock::now();

    auto scan_t1 = high_resolution_clock::now();
    for(int i=0; i<10000; ++i) {
        size_t count = 0;
        for(size_t b=0; b < blocks; ++b) { 
            if(piece.isBlockUsed(b)) count++; 
        }
    }
    auto scan_t2 = high_resolution_clock::now();

    double atomic_ns = duration_cast<nanoseconds>(atomic_t2-atomic_t1).count()/100000.0;
    double scan_ns = duration_cast<nanoseconds>(scan_t2-scan_t1).count()/10000.0;

    cout << "--- TEST 1: Urgency Benchmark ---" << endl;
    cout << "Atomic Lookup: " << atomic_ns << " ns" << endl;
    cout << "Bitfield Scan (Simulated): " << scan_ns << " ns" << endl;
    cout << "Speedup: " << scan_ns / atomic_ns << "x" << endl << endl;
}

// --- TEST 2: Mutex Perimeter Stress ---
void runPerimeterStress() {
    Piece piece(2048);
    const int threads = 128;
    const int iterations = 5000;
    vector<long long> waits;
    mutex waits_mutex;

    auto worker = [&]() {
        for(int i=0; i<iterations; ++i) {
            auto t1 = high_resolution_clock::now();
            lock_guard<mutex> lock(piece.piece_mutex_);
            auto t2 = high_resolution_clock::now();
            { 
                lock_guard<mutex> g(waits_mutex); 
                waits.push_back(duration_cast<nanoseconds>(t2-t1).count()); 
            }
        }
    };

    vector<thread> pool;
    for(int i=0; i<threads; ++i) pool.emplace_back(worker);
    for(auto &t : pool) t.join();
    sort(waits.begin(), waits.end());
    
    cout << "--- TEST 2: Mutex Perimeter Stress ---" << endl;
    cout << "Median Lock Latency: " << waits[waits.size()/2] << " ns" << endl;
    cout << "P95 Lock Latency: " << waits[waits.size()*95/100] << " ns" << endl << endl;
}

// --- TEST 3: First-Write-Wins (CAS Race) ---
void runCASRace() {
    Piece piece(1);
    piece.blockTracker_->try_request(0); 
    atomic<int> winners{0}, losers{0};
    auto worker = [&]() { 
        if(piece.blockTracker_->try_start_write(0)) winners++; 
        else losers++; 
    };
    vector<thread> pool;
    for(int i=0; i<100; ++i) pool.emplace_back(worker);
    for(auto &t : pool) t.join();

    cout << "--- TEST 3: First-Write-Wins (CAS Race) ---" << endl;
    cout << "CAS Winners: " << winners << ", Losers: " << losers << endl << endl;
}

int main() {
    runUrgencyBenchmark();
    runPerimeterStress();
    runCASRace();
    return 0;
}
