# Race Condition Visualizer

A C++ project that simulates, detects, and visualizes race conditions using POSIX threads, mutex, and semaphores.

## Files

| File | Purpose |
|------|---------|
| `simulator.cpp` | Core simulation — threads, mutex, semaphore modes |
| `educational.cpp` | Guided terminal demo with colored output |
| `detector.cpp` | File-level lock conflict monitor |
| `tracer.cpp` | Parse trace.log → ASCII timeline + report |
| `visualizer.html` | **Main HTML visualizer** — open in browser |
| `Makefile` | Build all |

## Build

```bash
make all
# Produces: simulator, detector, tracer, educational
```

Requires: `g++` with C++17 and POSIX (`-lpthread`). Linux/WSL/macOS.

## Run

### Interactive HTML Visualizer (recommended)
```
Open visualizer.html in any browser. No server needed.
```
Controls:
- **Threads**: 1–8 concurrent threads
- **Speed**: 50ms–1500ms per operation
- **Iterations**: how many increments per thread
- **Mode**: Unsafe / Mutex / Semaphore

### Terminal Simulator
```bash
# Usage: ./simulator [threads] [speed_ms] [mode]
# Mode: 0=unsafe, 1=mutex, 2=semaphore

./simulator 4 300 0    # 4 threads, 300ms speed, unsafe
./simulator 4 300 1    # with mutex
./simulator 4 300 2    # with semaphore
```
Output: JSON events (one per line) + trace.log

### Parse the trace
```bash
./simulator 4 200 0 > trace.log
./tracer trace.log
```
Generates ASCII timeline and race condition report.

### Educational Demo
```bash
./educational
```
Step-by-step guided terminal walkthrough of all three modes.

### Detector (file-level)
```bash
./detector shared_resource.txt
```
Monitors a file for overlapping lock conflicts using `/proc/locks`.

## Concepts Demonstrated

**Race Condition (Mode 0)**
```
T0: reads counter=5
T1: reads counter=5    ← both read same value
T0: writes 6
T1: writes 6           ← T0's write is LOST
Expected: 7, Got: 6
```

**Mutex (Mode 1)**
```
T0: waits for lock
T0: acquires lock
T0: reads counter=5, writes 6
T0: releases lock
T1: acquires lock      ← only after T0 releases
T1: reads 6, writes 7  ← correct!
```

**Semaphore (Mode 2)**
```
sem_wait(sem)  → P() → decrement; block if 0
  [critical section]
sem_post(sem)  → V() → increment; wake a waiter
```
Binary semaphore (init=1) = mutex.
Counting semaphore (init=N) = N concurrent accesses.

## ASCII Timeline Legend
```
R = Read     W = Write    X = Race condition!
L = Lock     U = Unlock   S = Semaphore acquired
s = Semaphore released    D = Thread done
- = Waiting
```
