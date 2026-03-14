#pragma once
#include "PieceSelector.h"
#include <memory>
#include <vector>

namespace aria2 {
class SegmentMan;
class UrgencyAwarePieceSelector : public PieceSelector {
public:
  explicit UrgencyAwarePieceSelector(SegmentMan* segman, std::unique_ptr<PieceSelector> fallback, uint16_t max_segs);
  bool select(size_t& index, const unsigned char* bitfield, size_t nbits) const override;
private:
  SegmentMan* segman_;
  std::unique_ptr<PieceSelector> fallbackSelector_;
  uint16_t max_segs_per_piece_;
  static inline bool peerHasPiece(const unsigned char* bitfield, size_t idx) {
    return (bitfield[idx / 8] >> (7 - (idx % 8))) & 1;
  }
};
}
