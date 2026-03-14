#include "UrgencyAwarePieceSelector.h"
#include "SegmentMan.h"

namespace aria2 {

UrgencyAwarePieceSelector::UrgencyAwarePieceSelector(SegmentMan* segman,
                                                     std::unique_ptr<PieceSelector> fallback,
                                                     uint16_t max_segs)
    : segman_(segman),
      fallbackSelector_(std::move(fallback)),
      max_segs_per_piece_(max_segs)
{
}

bool UrgencyAwarePieceSelector::select(size_t& index, const unsigned char* bitfield, size_t nbits) const
{
  // Pass 1: Urgency Swarm
  std::vector<size_t> urgentCandidates = segman_->getUrgentPieceCandidates(max_segs_per_piece_);
  for (size_t idx : urgentCandidates) {
    if (idx < nbits && peerHasPiece(bitfield, idx)) {
      index = idx;
      return true;
    }
  }

  // Pass 2: Fallback to LongestSequencePieceSelector
  return fallbackSelector_->select(index, bitfield, nbits);
}

} // namespace aria2
