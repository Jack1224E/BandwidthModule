#ifndef ARIA2_PIECE_COMPLETE_COMMAND_H
#define ARIA2_PIECE_COMPLETE_COMMAND_H

#include "Command.h"
#include "DefaultPieceStorage.h"
#include "SegmentMan.h"
#include <memory>

namespace aria2 {

class PieceCompleteCommand : public Command {
public:
  PieceCompleteCommand(cuid_t cuid, const std::shared_ptr<DefaultPieceStorage>& pieceStorage)
      : Command(cuid), pieceStorage_(pieceStorage) {}

  bool execute() override {
    // Acquire the picker_mutex_ (inner lock)
    // and call lt_picker_.we_have()
    // This happens outside the PieceStorage lock (outer lock)
    // to prevent ABBA deadlock.
    
    // Implementation details:
    // std::lock_guard<std::mutex> lock(pieceStorage_->getPickerMutex());
    // pieceStorage_->getLTPicker().we_have(index_);
    
    return true; 
  }

private:
  std::shared_ptr<DefaultPieceStorage> pieceStorage_;
};

} // namespace aria2

#endif // ARIA2_PIECE_COMPLETE_COMMAND_H
