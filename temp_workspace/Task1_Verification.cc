#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <algorithm>
#include "deps/aria2/src/Piece.h"
#include "home/jack/Documents/Bandwidth_Module/temp_workspace/PieceBlockTracker.h"

using namespace std;
using namespace std::chrono;

// --- TEST 1: Urgency Benchmark ---
void runUrgencyBenchmark() {
    aria2::Piece piece(0, 2048 * 16384); // 2048 blocks
    piece.requested_count_.store(256); // Simulate 256 requested blocks

    auto atomic_time = high_resolution_clock::now();
    for(int i=0; i<10000; ++i) {
        volatile auto v = piece.requested_count_.load();
    }
    auto atomic_end = high_resolution_clock::now();

    // Simulating the legacy O(n) scan
    auto scan_time = high_resolution_clock::now();
    for(int i=0; i<10000; ++i) {
        size_t count = 0;
        for(size_t b=0; b < 2048; ++b) { 
            if(piece.isBlockUsed(b)) count++; 
        }
    }
    auto scan_end = high_resolution_clock::now();

    double atomic_ns = duration_cast<nanoseconds>(atomic_end-atomic_time).count()/10000.0;
    double scan_ns = duration_cast<nanoseconds>(scan_end-scan_time).count()/10000.0;

    cout << "--- TEST 1: Urgency Benchmark ---" << endl;
    cout << "Atomic Lookup: " << atomic_ns << "ns" << endl;
    cout << "Bitfield Scan: " << scan_ns << "ns" << endl;
    cout << "Speedup: " << scan_ns / atomic_ns << "x" << endl << endl;
}

// --- TEST 2: Mutex Perimeter Stress ---
void runPerimeterStress() {
    aria2::Piece piece(0, 1000 * 16384);
    const int threads = 128;
    const int iterations = 10000;
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
    cout << "Median Lock Latency: " << waits[waits.size()/2] << "ns" << endl;
    cout << "P95 Lock Latency: " << waits[waits.size()*95/100] << "ns" << endl << endl;
}

// --- TEST 3: First-Write-Wins (CAS Race) ---
void runCASRace() {
    aria2::Piece piece(0, 16384);
    piece.blockTracker_->try_request(0); // Set block 0 to REQUESTED
    atomic<int> winners{0}, losers{0};
    auto worker = [&]() { 
        if(piece.blockTracker_->try_start_write(0)) winners++; 
        else losers++; 
    };
    vector<thread> pool;
    for(int i=0; i<10; ++i) pool.emplace_back(worker);
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
