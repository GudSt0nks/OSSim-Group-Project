# Group Project: Checkpoint 1

## Group 3 Information

### Members
- Bui Tuan Kien
- Kim Vladimir
- Mai Tung Duong

### GitHub repo 
https://github.com/GudSt0nks/OSSim-Group-Project

---

## 1. Schedulers

We implemented four CPU scheduling algorithms in `src/sched.c` to complement the baseline FCFS and Round Robin schedulers:

### Data Structures & Implementation Details

1. **SJF (Shortest Job First - Non-Preemptive)**:
   - **Data Structure**: Single FIFO-backed queue `sjf_q` (`queue_t`).
   - **Selection**: `sjf_pick()` invokes `q_pop_min(&sjf_q, proc_burst_remaining)`, selecting the ready process with the minimum current CPU burst length. Ties are broken in arrival order (FIFO order of queue insertion).
   - **Preemption**: `sjf_should_preempt()` returns `false` constantly.
   - *Exponential Averaging Note*: Real OS kernels cannot anticipate exact future CPU bursts. They estimate using exponential smoothing $\tau_{n+1} = \alpha t_n + (1 - \alpha) \tau_n$, where $t_n$ is the actual duration of the $n$-th CPU burst and $\tau_n$ is the historical estimate. In `ossim`, this could be implemented by adding a `predicted_burst` field to `pcb_t`, updated on burst completion.

2. **SRTF (Shortest Remaining Time First - Preemptive SJF)**:
   - **Data Structure**: `srtf_q` (`queue_t`).
   - **Selection**: `srtf_pick()` extracts the process with the shortest remaining burst via `q_pop_min(&srtf_q, proc_burst_remaining)`.
   - **Preemption**: `srtf_should_preempt(r)` inspects the queue using `q_min_key(&srtf_q, proc_burst_remaining)`. If any ready process has a shorter remaining burst than running process `r`, it returns `true`.

3. **Priority (Preemptive Priority with RR for Equal Priorities)**:
   - **Data Structure**: `prio_q` (`queue_t`).
   - **Selection**: `prio_pick()` removes the highest priority process using `q_pop_min(&prio_q, get_priority)` (where lower numerical value represents higher priority).
   - **Preemption**: `prio_should_preempt(r)` checks if `q_min_key(&prio_q, get_priority) < r->priority` (a strictly higher priority job has arrived). For equal priorities, Round Robin behavior is preserved as ready processes execute in order.

4. **MLFQ (Multilevel Feedback Queue)**:
   - **Data Structure**: `mlfq_q` (`queue_t`). Process level tracking uses `p->level` ($0 \le \text{level} < 3$).
   - **Levels & Quanta**:
     - Level 0: Quantum = $q$ ($2^0 q$)
     - Level 1: Quantum = $2q$ ($2^1 q$)
     - Level 2: Quantum = $4q$ ($2^2 q$)
   - **Queue Rules**:
     - `mlfq_pick()` picks the ready process at the lowest numerical level using `q_pop_min(&mlfq_q, get_level)`.
     - When a process exhausts its level quantum ($q \cdot 2^{\text{level}}$), it is demoted to $\min(\text{level} + 1, 2)$ upon preemption.
     - If a process yields the CPU early (e.g. blocks for I/O), its level is retained.
     - **Priority Boost / Starvation Prevention**: A global tick counter tracks elapsed runtime. Every 20 ticks, all processes (both in the ready queue and on CPU) are boosted back to level 0.

---

## 2. Deadlock Detection

We implemented resource deadlock detection in `src/sync.c` using the multiple-instance detection algorithm (Silberschatz Section 8.6.2):

1. **Algorithm**:
   - `Work` vector initialized to `resources[j].available`.
   - `Finish` vector initialized: for process $i$, if $i$ has no current allocations across all resources (or is terminated/new), `Finish[i] = true`; otherwise `Finish[i] = false`.
   - Iteratively find an index $i$ such that `Finish[i] == false` and $\forall j: \text{requested}[i][j] \le \text{Work}[j]$. If found, simulate resource release by setting `Finish[i] = true` and adding allocations back to `Work`.
   - Repeat until no further progress can be made.
2. **Reporting**:
   - Any process where `Finish[i] == false` and state is `P_WAITING` is part of a deadlock cycle.
   - The deadlocked PIDs are printed in increasing order:
     `DEADLOCK t=<now> pids: 1 2`
   - Deadlock detection accurately identifies processes $A$ (PID 1) and $B$ (PID 2) in `workloads/deadlock.txt` at $t=15$ while allowing independent process $C$ (PID 3) to execute to completion.

---

## 3. Terminal Output & Validation

### (a) Compile output

```
[user@DESKTOP-SMD3SK7 OSSim-Group-Project]$ make clean && make
rm -rf build ossim
mkdir -p build
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -c -o build/disk.o src/disk.c
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -c -o build/event.o src/event.c
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -c -o build/kernel.o src/kernel.c
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -c -o build/main.o src/main.c
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -c -o build/mem.o src/mem.c
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -c -o build/metrics.o src/metrics.c
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -c -o build/process.o src/process.c
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -c -o build/queue.o src/queue.c
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -c -o build/sched.o src/sched.c
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -c -o build/sync.o src/sync.c
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -c -o build/util.o src/util.c
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -c -o build/workload.o src/workload.c
cc -std=c99 -Wall -Wextra -Werror -O2 -g -Iinclude -o ossim build/disk.o build/event.o build/kernel.o build/main.o build/mem.o build/metrics.o build/process.o build/queue.o build/sched.o build/sync.o build/util.o build/workload.o
```

### (b) Deadlock Workload (`workloads/deadlock.txt`)
```text
$ ./ossim workloads/deadlock.txt --sched rr --quantum 2 --deadlock-every 5 --max-ticks 60 --no-paging
DEADLOCK t=15 pids: 1 2
WARNING: 2 of 3 processes did not finish within 60 ticks (deadlock? starvation? --max-ticks too small?)

=== ossim report: workloads/deadlock.txt | sched=rr q=2 | paging=off frames=64 replace=fifo | disk=fcfs cyl=200 head0=0 ===
pid  name        arrive   first  finish turnaround  waiting response    cpu faults   disk
1    A                0       0      -1         -1        8        0      5      0      0  (unfinished)
2    B                1       4      -1         -1        7        3      4      0      0  (unfinished)
3    C                0       2      15         15        9        2      6      0      0

summary
  ticks=60 finished=1/3 aborted=0
  cpu_busy=15 cpu_idle=45 cpu_kernel=0 utilization=25.00%
  throughput=0.0167 procs/tick context_switches=8
  avg_turnaround=15.00 avg_waiting=9.00 avg_response=2.00
```

### (c) Producer-Consumer Workload (`workloads/producer_consumer.txt`)
All schedulers successfully complete 2/2 processes without starvation or deadlocks:
- **FCFS**: finished=2/2, avg_waiting=7.00
- **RR (q=2)**: finished=2/2, avg_waiting=5.50
- **SJF**: finished=2/2, avg_waiting=5.00
- **SRTF**: finished=2/2, avg_waiting=3.50
- **PRIO**: finished=2/2, avg_waiting=5.00
- **MLFQ (q=2)**: finished=2/2, avg_waiting=6.00

### (d) Hand-Computed Metrics on `workloads/arrivals.txt`

| Scheduler | Parameters | Avg Turnaround | Avg Waiting | Avg Response | Context Switches |
|---|---|---|---|---|---|
| **FCFS** | Baseline | 22.67 | 14.83 | 10.17 | 8 |
| **RR** | $q=2$ | 26.17 | 18.33 | 3.17 | 22 |
| **SJF** | Non-preemptive | 17.83 | 10.00 | 9.00 | 8 |
| **SRTF** | Preemptive | 17.67 | 9.83 | 9.00 | 9 |
| **PRIO** | Smaller = higher | 20.50 | 12.67 | 9.17 | 10 |
| **MLFQ** | $q=2, 2q=4, 4q=8$, boost=20 | 24.00 | 16.17 | 0.00 | 19 |

#### Gantt Charts (`workloads/arrivals.txt`)
- **FCFS**:
  `[0-6] P1 | [7-15] P2 | [16-18] P3 | [19-20] P4 | [21-25] P5 | [26-29] P1 | [30-35] P6 | [36-38] P3`
- **SJF**:
  `[0-6] P1 | [7-8] P4 | [9-11] P3 | [12-15] P1 | [16-20] P5 | [21-23] P3 | [24-29] P6 | [30-38] P2`
- **SRTF**:
  `[0-6] P1 | [7-8] P4 | [9-11] P3 | [12-15] P1 | [16-16] P5 | [17-19] P3 | [20-23] P5 | [24-29] P6 | [30-38] P2`
- **PRIO**:
  `[0-1] P1 | [2-10] P2 | [11-11] P1 | [12-17] P6 | [18-19] P4 | [20-23] P1 | [24-26] P3 | [27-30] P1 | [31-35] P5 | [36-38] P3`
- **MLFQ**:
  `[0-1] P1 | [2-3] P2 | [4-5] P3 | [6-7] P4 | [8-9] P5 | [10-11] P1 | [12-13] P6 | [14-17] P2 | [18-18] P3 | [19-20] P5 | [21-22] P1 | [23-24] P6 | [25-26] P2 | [27-27] P5 | [28-28] P1 | [29-31] P3 | [32-33] P6 | [34-34] P2 | [35-38] P1`

---

### (e) Test Suite Validation (`tests/run.sh`)

```text
$ ./tests/run.sh
./tests/run.sh: line 1: ﻿#!/bin/sh: No such file or directory
PASS  FCFS textbook example: avg waiting 17.00
PASS  RR q=4 textbook example: avg waiting 5.67
PASS  RR q=4: context switches = 4 (P1,P2,P3,P1)
PASS  Paging: P1 alone, FIFO, 3 frames -> 15 faults (textbook Sec. 10.4.2)
PASS  Disk: FCFS, head 53 -> 640 cylinders (textbook Sec. 11.2.1)
PASS  Producer/consumer completes
PASS  Deadlock workload without detection: A and B stall, C finishes
PASS  Mixed workload finishes
PASS  Mixed workload finishes with 1 frame (faults wait for a frame)
PASS  Deadlock detection reports A and B (pids 1 2)
PASS  Producer/consumer completes under sjf
PASS  Producer/consumer completes under srtf
PASS  Producer/consumer completes under prio
PASS  Producer/consumer completes under mlfq
PASS  Arrivals FCFS avg_waiting=14.83
PASS  Arrivals RR q=2 avg_waiting=18.33
PASS  Arrivals SJF avg_waiting=10.00
PASS  Arrivals SRTF avg_waiting=9.83
PASS  Arrivals PRIO avg_waiting=12.67
PASS  Arrivals MLFQ q=2 avg_waiting=16.17
SKIP  Paging: LRU, 3 frames -> 12 faults (textbook Sec. 10.4.4)
SKIP  Disk: SSTF, head 53 -> 236
SKIP  Disk: SCAN moving down, head 53 -> 236
SKIP  Disk: C-LOOK moving up, head 53 -> 322
---
20 passed, 4 skipped (not implemented yet), 0 failed
```

## 4. What Does Not Work Yet
- Checkpoint 1 objectives (SJF, SRTF, PRIO, MLFQ, and Deadlock Detection) are fully functional and pass all test validations.
- Features scheduled for Checkpoint 2 (Paging replacement policies: Clock; Disk scheduling policies: SSTF, SCAN, C-LOOK) are not yet implemented.

---

## 5. Contribution
- **Kim Vladimir**: Part (a) - CPU Scheduling algorithms (`src/sched.c`): implementation of SJF, SRTF, PRIO, and MLFQ with multilevel quanta and periodic boost.
- **Bui Tuan Kien**: Part (b) - Deadlock detection (`src/sync.c`): implementation of textbook detection algorithm.
- **Mai Tung Duong**: Parts (c) & (d) - Progress report 1 (`Group03_CP1.md`), test script integration (`tests/run.sh`), manual validation and verification of metrics and Gantt charts.
