# 01_blueprint.md - Atomic Decomposition (libtorrent Fusion V2)

## 1. The Modulo Coordinate Fix
To prevent bit-shifting corruption, all offset-to-index translations must follow the strict modulo coordinate system.
- `piece_idx = file_offset / PIECE_SIZE`
- `block_idx = (file_offset % PIECE_SIZE) / BLOCK_SIZE`

> [!IMPORTANT]
> This ensures that even with variable piece sizes or sub-block writes, the mapping to the `piece_picker` internal bitfield remains stable.

## 2. Segment Clipping
Every `aria2::Segment` allocated or managed by the system must be physically clipped to `PIECE_SIZE` boundaries. 
- A segment cannot span across two pieces.
- If a network request spans a piece boundary, it must be decomposed into two distinct `PieceBlock` requests within the picker.

## 3. The 4-State Machine & Math Corrections
We are moving from a binary used/free state to a robust 4-state lifecycle for every block:
1. **OPEN**: Block is available for picking.
2. **REQUESTED**: Block is assigned to a peer/cuid but not yet received.
3. **WRITING**: Data has arrived and is being committed to disk (O_DIRECT).
4. **FINISHED**: Disk confirms write success AND SHA-256 hash passes.

### Last-Block Ceiling Logic (Math Correction)
To prevent "phantom block" stalls and ensure all data is requested, we must use ceiling division for the block count:
- `last_bpp = (last_piece_bytes + BLOCK_SIZE - 1) / BLOCK_SIZE`
- **Constraint**: For single-piece files, `regular_bpp = last_bpp` must be forced.

> [!WARNING]
> `mark_as_finished()` must be gated by both the disk return and the integrity check. `we_have()` is the final terminal state.

## 4. The Thread-Safety Perimeter
To prevent race conditions during high-concurrency "End Game" or peer churn, a `std::recursive_mutex` will wrap the entire `piece_picker` instance and related data structures.
- **Coverage**:
    - `HAVE` message processing.
    - `BITFIELD` arrivals (Rarity Bootstrap).
    - Peer disconnection/cleanup.
    - Block picking and state transitions.

### Deque Safety (The Silent Crash)
- **Problem**: `aria2::usedSegmentEntries_` is a `std::deque` and is not thread-safe.
- **Fix**: The `picker_mutex_` MUST wrap every access (iterator, push, erase) to `usedSegmentEntries_` to prevent invalidation crashes during concurrent network and disk-completion events.

## 5. Hidden Dependencies & Rarity Bootstrap
- **`aria2_peer_stub`**: A lightweight bridge to replace `libtorrent::torrent_peer*`. It will map `aria2` peer metadata to the picker's requirement for tracking who has which piece.
- **Rarity Bootstrap**: Upon receipt of a bitfield, the system must call `libtorrent::piece_picker::inc_refcount` for every piece the peer has. This establishes the "rarest-first" baseline before the first block is picked.

## 6. Dependency Tree
- [ ] Implement `LTPiecePicker` wrapper with `std::recursive_mutex`.
- [ ] Implement `aria2_peer_stub`.
- [ ] Refactor `SegmentMan` to use `LTPiecePicker` instead of `BitfieldMan` for picking.
- [ ] Hook `DefaultDiskWriter` to trigger `WRITING -> FINISHED` transitions.
