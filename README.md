# Bandwidth Module: End-Game Sniping (Subpart 3.2)

This repository contains a high-performance download engine based on a modified version of `libaria2`. It features an implementation of **End-Game Sniping**, specifically the **First-Write-Wins (CAS) Gate** (Subpart 3.2).

## Key Features
- **End-Game Sniping**: Prevents multiple threads from writing the same data blocks during the "sniping" phase.
- **Atomic CAS Gate**: Uses atomic `compare_exchange_strong` to ensure high-speed, thread-safe data commitment.
- **Accurate Telemetry**: Reverted accounting logic to ensure stable progress metrics.

## Project Structure
- `libaria2/`: Modified source code of the `aria2` library.
- `main.cpp`: Benchmark/CLI application to run high-speed downloads.
- `CMakeLists.txt`: Build configuration.

## How to Build

### 1. Build the modified libaria2
```bash
cd libaria2
autoreconf -i
./configure
make
```

### 2. Build the Benchmark Application
```bash
mkdir build
cd build
cmake ..
make
```

## Usage
```bash
./benchmark <URL>
```

## Status
The current version is stable and tested against real 100MB downloads, achieving high bandwidth utilization with Subpart 3.2 logic preserved.
