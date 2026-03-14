# Bandwidth Module

A high-performance C++ networking engine designed for low-latency, multithreaded downloads. This module is a surgical fork of the **aria2** core, optimized specifically for "End-Game Sniping" scenarios where multiple concurrent connections compete to finalize the last segments of a file.

---

## Core Philosophy: The First-Write-Wins Gate

Traditional downloaders often suffer from redundant I/O during the final phase of a download. This module implements a **CAS (Compare-and-Swap) Gate** within the `PieceBlockTracker` to ensure that even when 16+ connections are "sniping" the same block, only the fastest one commits to the disk. This prevents SSD write-inflation and maintains system responsiveness during heavy downloads.

### Key Features

* **Atomic State Tracking**: Implements an atomic state machine (`REQUESTED`, `WRITING`, `FINISHED`) to manage per-block life cycles.
* **Fast-Exit Optimization**: Instantly rejects redundant connections once a block is finished, skipping expensive atomic operations.
* **Thread-Safe Memory Purge**: Automatically reclaims tracker state after successful cryptographic hash verification to prevent memory bloat.

---

## Usage & Integration

This module is intended to be used as a backend "Brain" for integrated browser environments or desktop download managers. It exposes its functionality primarily through a centralized management layer in `SegmentMan`.

### Build Prerequisites

* C++11 compatible compiler
* GnuTLS or OpenSSL
* Libxml2 or Expat

---

## Licensing & Attribution

The **Bandwidth Module** is based on the open-source [aria2](https://github.com/aria2/aria2) project.

As aria2 is licensed under the **GNU General Public License v2.0**, this derivative work is also released under the **GPLv2**.

* **Original Author**: Tatsuhiro Tsujikawa and the aria2 contributors.
* **Modifications**: All logic pertaining to the PieceBlockTracker, CAS Gate implementation, and specialized telemetry accounting are unique to this fork.

> **Note**: This repository represents "Version 1" of the Bandwidth Module. Development for "Version 2" is focused on a memory-mapped Rust engine to achieve zero-copy I/O without the complexities of legacy C++ state management.

---