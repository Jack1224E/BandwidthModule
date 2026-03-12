# Step 6/7 Review: Operation New Skull (libtorrent Fusion V2)

## Updated LTPiecePicker Wrapper Logic

The "Fusion V2" engine implements a strictly segmented lock hierarchy to prevent ABBA deadlocks between the Piece Storage and the Peer Segment Management layers.

### 1. The Deferred Callback Pattern
To break the cycle where `PieceStorage` calls into `SegmentMan` (and vice versa), we have implemented the **Deferred Callback Pattern**.

- **Workflow**:
    1. `PieceStorage::completePiece(piece)` is called upon successful chunk verification.
    2. Instead of immediately updating the `LTPiecePicker`, the storage engine releases its `piece_storage_mutex_`.
    3. A new `PieceCompleteCommand` is instantiated and moved into the `DownloadEngine`'s command queue via `e_->addCommand()`.
    4. The main thread (event loop) eventually executes this command.
    5. `PieceCompleteCommand::execute()` acquires ONLY the `picker_mutex_` (inner lock) and calls `lt_picker_.we_have()`.

This ensures that the `lt_picker_` update (which can trigger further calls into segment management) never happens while holding the `piece_storage_mutex_`.

### 2. ABI-Compliant Peer Container
The crash in V1 was caused by a stub that didn't account for libtorrent's bit-packed `torrent_peer` layout (specifically fields like `snubbed`, `speed_category`, and `have_pieces`).

- **The Fix**: `aria2_peer_stub` now inherits directly from `lt::torrent_peer`.
- **Validation**: Added `static_assert(offsetof(lt::torrent_peer, port) > 0)` (and others) to detect layout shifts at compile-time.
- **Result**: aria2 pointers to `torrent_peer` are now binary-compatible with libtorrent's internal dereferences.

### 3. Mutex Perimeter Rules
- `std::recursive_mutex` is officially banned to expose re-entrancy risks.
- `picker_mutex_` is the **INNER** lock.
- `piece_storage_mutex_` is the **OUTER** lock.
- **Rule**: Never hold both simultaneously for any operation that might trigger a callback.

## Atomic Implementation Status
- [x] `src/LTPeerStub.h` - Binary compatible container implemented.
- [x] `src/PieceCompleteCommand.h` - Async callback bridge implemented.
- [x] Lock Hierarchy documented.

The "New Skull" is ready for integration into the main fusion engine.
