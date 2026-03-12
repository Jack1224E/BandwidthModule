# 01_blueprint.md - Native Lock-Free Scheduler Architecture

## Executive Summary
This architecture abandons all external library dependencies (libtorrent) in favor of a **Native High-Performance Scheduler** integrated directly into aria2. It leverages lock-free synchronization for block-level tracking and implements "End-Game Sniping" to mitigate tail latency in 10Gbps environments.

## 1. The Lock-Free Block Tracker
To eliminate mutex contention at the 16KB block resolution:
- **PieceBlockTracker**: Encapsulated within each `Piece` object.
- **Atomic State Machine**: `std::array<std::atomic<uint8_t>, BLOCKS_PER_PIECE> block_states_`.
    - `0 (EMPTY)`: Default state.
    - `1 (REQUESTED)`: Reserved by a connection.
    - `2 (WRITING)`: Data received, being committed to disk.
    - `3 (FINISHED)`: Successfully written and verified.
- **CAS Transitions**: Reservations use `compare_exchange_strong` to transit from `EMPTY` to `REQUESTED`. 

## 2. Rarity-Aware (Urgency) Allocation
HTTP "Rarity" is redefined as **Piece Completion Urgency**.
- **Strategy**: `SegmentMan::getSegment()` is overridden to prioritize connections towards pieces with the highest number of `REQUESTED` or `WRITING` blocks.
- **Goal**: Minimize the "In-Flight Piece" window to reduce memory pressure and accelerate hash verification/flushing.

## 3. HTTP End-Game Sniping
Replicates BitTorrent "sniping" for the final 1% of the download.
- **Trigger**: `downloaded_bytes > total_length * 0.99`.
- **Logic**: Connections are permitted to request blocks already in the `REQUESTED` state.
- **Collision Resolution**: **First-Write-Wins**. 
    - Multiple connections compete to CAS state from `REQUESTED` to `WRITING`.
    - The winner proceeds to disk I/O.
    - Losers detect the CAS failure and immediately discard buffers, preventing redundant writes and over-allocation.

## 4. Deferred Completion & Verification
- **Segment Integrity**: Pieces are only marked as complete in `BitfieldMan` after the tracker reports all blocks as `FINISHED` and the SHA-256 hash passes.
- **Cleanup**: Lock-free state ensures no "zombie" requested blocks remain if a connection drops.

## 5. Constraint Compliance
- **Zero Networking Overhead**: No changes to `SocketCore`.
- **Memory Efficiency**: 16KB resolution ensures compatibility with aria2 standard buffer sizes.
