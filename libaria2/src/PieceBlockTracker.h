#ifndef PIECE_BLOCK_TRACKER_H
#define PIECE_BLOCK_TRACKER_H

#include <vector>
#include <atomic>
#include <cstddef>
#include <unordered_map>
#include <mutex>
#include <memory>
#include "Command.h"


namespace aria2 {

class PieceBlockTracker {
public:
  enum State : uint8_t { FREE = 0, REQUESTED = 1, WRITING = 2 };

  PieceBlockTracker() = default;

  struct PieceData {
    std::vector<std::atomic<uint8_t>> states;
    std::vector<std::atomic<cuid_t>> winners;
    PieceData(size_t blocks) : states(blocks), winners(blocks)
    {
      for (size_t i = 0; i < blocks; ++i) {
        states[i].store(FREE, std::memory_order_relaxed);
        winners[i].store(0, std::memory_order_relaxed);
      }
    }
  };


  // Initializes state for a piece if it doesn't exist
  void add_piece(size_t piece_idx, size_t blocks)
  {
    std::lock_guard<std::mutex> lock(map_mutex_);
    if (pieces_.find(piece_idx) == pieces_.end()) {
      pieces_[piece_idx] = std::make_shared<PieceData>(blocks);
    }
  }

  bool try_request(size_t piece_idx, size_t block_idx)
  {
    auto data = get_piece_data(piece_idx);
    if (!data || block_idx >= data->states.size()) {
      return false;
    }
    uint8_t expected = FREE;
    return data->states[block_idx].compare_exchange_strong(
        expected, REQUESTED, std::memory_order_acq_rel);
  }

  bool try_start_write(size_t piece_idx, size_t block_idx, cuid_t cuid)

  {
    auto data = get_piece_data(piece_idx);
    if (!data || block_idx >= data->states.size()) {
      return false;
    }

    cuid_t expected_winner = 0;

    if (data->winners[block_idx].compare_exchange_strong(
            expected_winner, cuid, std::memory_order_acq_rel)) {
      // I am the first one to write this block!
      return true;
    }

    // If I'm already the winner, I can continue writing
    return expected_winner == cuid;
  }

  void reset_block(size_t piece_idx, size_t block_idx)
  {
    auto data = get_piece_data(piece_idx);
    if (data && block_idx < data->states.size()) {
      data->states[block_idx].store(FREE, std::memory_order_relaxed);
      data->winners[block_idx].store(0, std::memory_order_relaxed);
    }
  }

  // Clear ownership for a piece if it belongs to the given cuid
  void clear_cuid(size_t piece_idx, cuid_t cuid)
  {
    auto data = get_piece_data(piece_idx);
    if (data) {
      for (size_t i = 0; i < data->winners.size(); ++i) {
        cuid_t expected = cuid;
        data->winners[i].compare_exchange_strong(expected, 0,
                                                 std::memory_order_acq_rel);
      }
    }
  }

private:
  std::shared_ptr<PieceData> get_piece_data(size_t piece_idx)
  {
    std::lock_guard<std::mutex> lock(map_mutex_);
    auto it = pieces_.find(piece_idx);
    if (it != pieces_.end()) {
      return it->second;
    }
    return nullptr;
  }

  std::unordered_map<size_t, std::shared_ptr<PieceData>> pieces_;
  std::mutex map_mutex_;

};

} // namespace aria2

#endif // PIECE_BLOCK_TRACKER_H
