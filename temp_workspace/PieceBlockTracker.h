#ifndef PIECE_BLOCK_TRACKER_H
#define PIECE_BLOCK_TRACKER_H
#include <vector>
#include <atomic>
#include <cstddef>

namespace aria2 {
class PieceBlockTracker {
public:
  enum State : uint8_t { FREE = 0, REQUESTED = 1, WRITING = 2, FINISHED = 3 };
  explicit PieceBlockTracker(size_t blocks) : states_(blocks) {
    for (size_t i = 0; i < blocks; ++i) states_[i].store(FREE, std::memory_order_relaxed);
  }
  bool try_request(size_t idx) {
    uint8_t expected = FREE;
    return states_[idx].compare_exchange_strong(expected, REQUESTED, std::memory_order_acq_rel);
  }
  bool try_start_write(size_t idx) {
    uint8_t expected = REQUESTED;
    return states_[idx].compare_exchange_strong(expected, WRITING, std::memory_order_acq_rel);
  }
  void mark_finished(size_t idx) { states_[idx].store(FINISHED, std::memory_order_release); }
  void reset_block(size_t idx) { states_[idx].store(FREE, std::memory_order_relaxed); }
private:
  std::vector<std::atomic<uint8_t>> states_;
};
}
#endif
