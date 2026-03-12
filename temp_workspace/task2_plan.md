# Task 2 Plan: Urgency-Aware Allocation

## Subpart 1: Max-Heap Tracking (Data Structure)
We need a way to track the most urgent piece instantly without scanning the entire bitfield or piece list.
- **Data Structure**: `std::set<std::pair<uint16_t, size_t>, std::greater<>> urgency_heap_`
- **Location**: To be added to `DefaultPieceStorage` or a dedicated tracking component.
- **Complexity**: 
    - **O(1)** access to the highest urgency piece (`*urgency_heap_.begin()`).
    - **O(log K)** for updates (removal/insertion) when a piece’s `requested_count_` changes.
- **Storage**: Maps `(requested_count, piece_index)` to ensure the highest `requested_count` is always at the top.

## Subpart 2: UrgencyAwarePieceSelector (The Logic)
A new selector that implements the prioritisation logic.
- **Inheritance**: Inherits from `aria2::PieceSelector`.
- **Method `select()`**:
    - Peek at `urgency_heap_.begin()`.
    - **Anti-Starvation Guard**: Check if `requested_count_ < max segments allowed per piece`. 
    - **Fallback**: If the heap is empty, all pieces are saturated (reach max segments), or the candidate piece is invalid, delegate selection to the vanilla `LongestSequencePieceSelector`.
- **Result**: This ensures connections "swarm" around the most urgent/active pieces while still allowing new pieces to be started when needed.

## Subpart 3: SegmentMan Handshake (Lifecycle Hooks)
Integrating the urgency tracking into the segment lifecycle within `SegmentMan.cc`.
- **Allocation (`getSegment`)**: 
    - Target: Around line 161 and the memo-resume path (line 170).
    - Logic: Fetch the `Piece` object, lock the `piece_mutex_`, increment `requested_count_`.
    - Update Tracker: Update the `urgency_heap_` with the new score in O(log K) time.
- **Deallocation (`cancelSegment`, `completeSegment`)**:
    - Logic: Lock the `piece_mutex_`, decrement `requested_count_`.
    - Update Tracker: Update the `urgency_heap_` to reflect the reduced urgency score.

## Subpart 4: Injection Point (The Swap)
We swap the piece selection strategy at the group initialization level.
- **File**: `RequestGroup.cc`
- **Method**: `initPieceStorage`
- **Logic**: Locate where the `LongestSequencePieceSelector` is instantiated and replace it with a new `UrgencyAwarePieceSelector`.
- **Constraint**: This allows us to inject the new logic without modifying the internals of `DefaultPieceStorage`.
