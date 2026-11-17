# Memory Management Simulator

A menu-driven, single-file C program that simulates core operating-system memory management concepts. Written in portable C99 with no external dependencies.

## Features

### 1. Contiguous allocation (dynamic partitioning)
- **First Fit**, **Best Fit**, **Worst Fit** placement strategies (switchable at runtime)
- Allocate / free processes by ID
- Automatic **coalescing** of adjacent free holes
- **Compaction** to eliminate external fragmentation
- Memory map table plus an ASCII bar chart
- Statistics: used/free memory, hole count, largest hole, **external fragmentation %**
- Clear failure messages that distinguish "not enough memory" from "fragmented memory"

### 2. Paging
- Configurable number of frames and page size
- Process creation allocates frames (not necessarily contiguous) and builds a **page table**
- Reports **internal fragmentation** for each process
- **Logical → physical address translation** with the full calculation shown
- Out-of-range addresses are reported as a segmentation fault
- Frame table and per-process page table display

### 3. Page replacement
- **FIFO**, **LRU**, **Optimal (Belady's MIN)**
- Step-by-step trace table showing frame contents, hits and faults
- "Compare all" mode with fault counts and hit ratios

## Build and run

```bash
gcc -Wall -Wextra -O2 -o memsim memsim.c
./memsim
```

Works with GCC or Clang on Linux, macOS, and Windows (MinGW / WSL).

## Example sessions

### Contiguous allocation (100 KB, First Fit)

```
  Start    End      Size     Status
--------------------------------------------
  0        29       30       P1
  30       49       20       FREE
  50       89       40       P3
  90       99       10       FREE
--------------------------------------------
  [111111111111111111............333333333333333333333333......]

  Total: 100 KB | Used: 70 KB (70.0%) | Free: 30 KB in 2 hole(s)
  Largest hole: 20 KB | External fragmentation: 33.3%
```

Here a 25 KB request fails even though 30 KB is free, which is exactly the situation compaction solves.

### Paging (8 frames x 256 bytes, process of 600 bytes)

```
  P1 created: 600 bytes -> 3 page(s). Internal fragmentation: 168 bytes.
  Logical address  : 300
  Page number      : 300 / 256 = 1, offset = 44
  Physical address : 1 * 256 + 44 = 300
```

### Page replacement

Reference string `7 0 1 2 0 3 0 4 2 3 0 3 2`, 3 frames:

| Algorithm | Faults | Hit ratio |
|-----------|--------|-----------|
| FIFO      | 10     | 23.1%     |
| LRU       | 9      | 30.8%     |
| Optimal   | 7      | 46.2%     |

## How it works

| Topic | Implementation |
|-------|----------------|
| Memory map | Array of `{start, size, pid}` blocks; `pid == 0` means a free hole |
| Allocation | Scan holes by chosen strategy, split the chosen hole if larger than the request |
| Free | Mark block free, then merge neighbouring free holes |
| Compaction | Slide all allocated blocks to address 0 and merge free space into one hole |
| Paging | `frame_pid[]` / `frame_page[]` frame table + `page_table[]` per process |
| Translation | `page = addr / page_size`, `offset = addr % page_size`, `phys = frame * page_size + offset` |
| FIFO | Evict the frame with the oldest load time |
| LRU | Evict the frame with the oldest last-use time |
| Optimal | Evict the page whose next use is farthest in the future (or never) |

## Limits

| Item | Limit |
|------|-------|
| Memory blocks | 128 |
| Process IDs | 1-99 |
| Physical frames (paging) | 256 |
| Pages per process | 128 |
| Reference string length | 128 |
| Frames for replacement | 16 |

## Project layout

```
.
├── memsim.c    # entire simulator
└── README.md
```

## Possible extensions

- Segmentation and segmented paging
- Buddy system allocator
- TLB simulation with hit/miss statistics
- Clock (second-chance) and LFU replacement
- Reading workloads from a file for batch runs

## License

Free to use for learning and coursework.
