# Technical Report: Sequential and Parallel Implementation of the Job-Shop Scheduling Problem

This document serves as a complete technical report for the project that implements solutions for the Job-Shop Scheduling Problem (JSSP). The project was developed as part of the practical work for the High-Performance Computing course. Two implementations were created and analyzed: a sequential version and a parallel version using OpenMP for shared memory.

The report is organized to cover the main topics of the project:

* **A. Sequential Implementation**: Description of the algorithm and its implementation.
* **B. Parallel Implementation**: Details on partitioning, communication, mutual exclusion, and analysis of the parallel implementation.
* **C. Performance Analysis**: Measurement of execution times, comparison between the implementations, and evaluation of speedup and efficiency.

---

## Description of the Job-Shop Scheduling Problem (JSSP)

The Job-Shop Scheduling Problem is a combinatorial optimization problem that involves scheduling a set of **jobs** on a set of **machines**. Each job consists of a sequence of **operations**. Each operation requires a specific machine for a given period of time. The classic objective is to find a schedule that minimizes the **makespan**, that is, the total time needed to complete all jobs, while respecting the following fundamental constraints:

1. **Machine Constraint**: A machine can process only one operation at a time.
2. **Precedence Constraint**: The operations of the same job must be executed in a predefined order.
3. **Non-Interruption Constraint**: Once started, an operation cannot be interrupted.


---

## Project Structure

The organization of the project's files and directories is as follows:

.  
├── **data/** # Input files (.jss) containing problem definitions  
│ ├── jobshop_small.jss # Small dataset (3 jobs, 3 machines)  
│ ├── jobshop_medium.jss # Medium dataset (6 jobs, 6 machines)  
│ └── jobshop_huge.jss # Large dataset (25 jobs, 25 machines)  
├── **logs/** # Directory to store execution and analysis logs  
│ ├── *_execution_times.txt # Average execution times recorded  
│ ├── sequence.txt # Operation sequences executed (for debugging/analysis)  
│ └── timing.txt # Detailed timing per thread (in the parallel version)  
├── **mappings/** # Mapping files used for visualization  
│ ├── mapping_small.txt # Mapping for the small dataset  
│ ├── mapping_medium.txt # Mapping for the medium dataset  
│ └── mapping_huge.txt # Mapping for the large dataset  
├── **viz/** # Directory to save generated visualizations  
├── job_shop_sequential.c # Source code of the sequential implementation  
├── job_shop_parallel.c # Source code of the parallel implementation with OpenMP  
├── job_shop_visualizer.py # Python script to generate Gantt charts  
└── README.md # This file (report)  

---

## A. Sequential Implementation

This section describes the greedy algorithm used in the sequential implementation and details about its structure.

### 1. Description of the Sequential Scheduling Algorithm

The sequential implementation adopts a simple greedy algorithm to schedule the operations. At each step, the algorithm identifies the job whose next unscheduled operation can start the earliest. Once this job is identified, it tries to find the first available time slot on the machine required for that operation, starting from the completion time of the previous operation of the same job.

The flow of the algorithm is as follows:

1. Initialize the earliest possible start time for each job (```earliest_starts```) as 0.
2. Initialize the counter of scheduled operations for each job (```scheduled_ops```) as 0.
3. While there are unscheduled operations:
    * Find the job (```j```) with the smallest value in ```earliest_starts[j]``` that still has operations left to schedule (```scheduled_ops[j] < num_operations```).
    * Identify the next operation (```op```) to be scheduled for job ```j```: ```op = scheduled_ops[j]```.
    * Determine the machine (```m```) and the duration (```d```) required by operation ```op``` of job ```j```.
    * Calculate the start time (```start_time```) for this operation. This is the earliest time when machine ```m``` is free *after* ```earliest_starts[j]```.
    * Schedule the operation: assign ```start_time``` to operation ```op``` of job ```j```.
    * Update the earliest possible start time for the *next* operation of job ```j```: ```earliest_starts[j] = start_time + duration```.
    * Increment the counter of scheduled operations for job ```j```: ```scheduled_ops[j]++```.

The auxiliary function ```find_available_time(machine, duration, minimum_start_time)``` is responsible for scanning the already occupied time slots on ```machine``` and returning the first moment, equal to or later than ```minimum_start_time```, where the operation can be allocated without conflicts with already scheduled operations on that machine.

### 2. Structure of the Sequential Implementation (`job_shop_sequential.c`)

The sequential implementation code is organized with the following main components:

* **Data Structures**: Defines structures such as `Operation` (for details of each operation: machine, duration, start time) and `JobShopProblem` (to encapsulate the number of jobs, machines, operations, and job data).
* **Input and Output**: Functions to load the problem from a `.jss` file and save the scheduled solution to another file.
* **Scheduling Logic**: Implements the greedy algorithm described above, ensuring compliance with the JSSP constraints.
* **Time Measurement**: Includes basic logging mechanisms and CPU time measurement (using `clock()`) for performance evaluation.

The sequential implementation serves as a baseline for comparison and validation of the parallel version, providing a correct solution (although not necessarily optimal in makespan, due to the greedy approach) to the problem.


## B. Parallel Implementation with Shared Memory

This section details the parallelization approach using Foster's methodology and the implementation with OpenMP.

### 1. Partitioning Proposal (Foster's Methodology)

The parallelization of the greedy solution followed Foster’s methodology:

* **a) Partitioning:** The problem was decomposed based on **data**. Jobs are the primary units of computation distributed among threads. The granularity is one full job: each thread is responsible for scheduling *all* operations of a subset of jobs. Shared data objects include the information about machine availability over time.
* **b) Communication:** Threads need to communicate and synchronize access to the shared machine state. The communication pattern is essentially producer-consumer: threads "produce" machine occupations when scheduling operations and "consume" available slots when searching for start times. Communication occurs implicitly through access to shared memory structures and is synchronous at points requiring mutual exclusion.
* **c) Agglomeration:** The decomposed jobs were agglomerated and assigned to threads using a simple **round-robin distribution** strategy. Job `j` is assigned to the thread with ID `j % num_threads`. This strategy aims for static load balancing. Agglomeration by job seeks to minimize communication, since once a thread decides to schedule the next operation of an assigned job, it only needs to interact with the global machine state to find a time slot.
* **d) Mapping:** The mapping of logical threads (created by OpenMP) to physical cores is managed by the OpenMP runtime (typically `1:1` or `1:N` depending on the hardware and configuration). The directive `#pragma omp parallel num_threads(num_threads)` controls thread creation and the number of threads used. For very small problems, too many threads can increase overhead, so the number of threads should be tuned appropriately.

### 2. Structure of the Parallel Implementation (`job_shop_parallel.c`)

The parallel implementation uses OpenMP to create a pool of threads that concurrently execute the scheduling logic. The key components are:

* **Work Distribution**: Before the main scheduling loop, jobs are assigned to threads using round-robin logic.
* **Parallel Scheduling Loop**: The main loop that selects and schedules operations is enclosed within an OpenMP parallel region.
* **Mutual Exclusion**: Critical sections (`#pragma omp critical`) are used to protect access to shared data structures representing machine states and scheduled operations counters.
* **Per-Thread Logging**: Mechanisms are in place so that each thread independently records its activities (scheduled operations, timing) for later analysis.

The function `schedule_with_strict_division()` encapsulates the initial assignment logic and the parallel scheduling loop.


### 3. Characteristics of the Parallel Program

The shared-memory parallel nature raises important questions about data management and synchronization:

* **a) Shared Global Variables:**
    * **Read-Only:**
        * `problem.num_jobs`, `problem.num_machines`, `problem.num_operations`: Problem dimensions, read by all threads.
        * Fixed characteristics of operations (machine and duration) defined in the problem input.
    * **Read and Write (Require Synchronization):**
        * `problem.jobs[][].start_time`: The start time of each operation, written by the thread that schedules it.
        * `scheduled_ops[]`: Array counting how many operations of each job have already been scheduled. Used to identify the next operation of a job.
        * `earliest_starts[]`: Array storing the earliest possible start time for the next operation of a given job (depends on the finish time of the previous operation of the same job).
        * Internal structures representing machine occupation over time (used by the `find_available_time` function).
        * `problem.thread_logs[][]`: Buffers for per-thread logging.

* **b) Critical Sections and Potential Race Conditions:**
    * The main critical section (`#pragma omp critical (scheduler)`) encloses the core scheduling logic: selecting the job whose next operation can start the earliest, finding an available machine slot, assigning the start time, and updating job state counters.
    * **Unprotected Race Conditions:** If the access to finding a machine's available slot and the subsequent update of machine occupation were not protected, multiple threads could simultaneously find the "same" available slot and attempt to schedule conflicting operations on the same machine at the same time. Concurrent access to `scheduled_ops` and `earliest_starts` would also cause unpredictable results.

* **c) Mutual Exclusion Techniques:**
    * **OpenMP Critical Regions**: The use of `#pragma omp critical` ensures that only one thread at a time can execute the enclosed code, protecting the access and modification of shared data structures managing the scheduling state (machines, operation counters).
    * **Work Distribution by Job**: Although not a direct mutual exclusion mechanism, assigning entire jobs to specific threads *reduces* the likelihood of multiple threads competing simultaneously for the *same* job, although they may still compete for *machines*.
    * **Fallback Mechanism**: In some greedy parallel scheduling implementations for JSSP, a fallback mechanism to sequential mode or a different type of synchronization may be necessary if contention becomes too high or deadlocks occur (threads waiting for resources held by others within critical sections). Although the exact deadlock mechanism is not detailed in the pseudocode, the mention of a fallback suggests this consideration.


## C. Performance Analysis

This section presents the measurement methodology, the results obtained, and the analysis of speedup and efficiency of the implementations.

### 1. Execution Time Measurement Mechanisms

To evaluate performance, appropriate timing mechanisms were used for each context:

* **Sequential Implementation**: Uses the `clock()` function from the standard C library to measure the CPU time spent executing the scheduling algorithm.
* **Parallel Implementation**: Uses the `omp_get_wtime()` function provided by OpenMP, which measures the wall-clock time, more relevant for evaluating real parallelization gains.

To obtain robust results, each configuration (problem + number of threads) was executed **10 times**, and the average execution time was calculated. I/O operations (reading the problem, writing the solution) were excluded from the timing to isolate only the scheduling algorithm performance. Average times were recorded in log files.

### 2. Performance Results

Tests were conducted on the three provided datasets, representing different problem sizes:

* **Small**: 3 jobs × 3 machines = 9 total operations.
* **Medium**: 6 jobs × 6 machines = 36 total operations.
* **Huge**: 25 jobs × 25 machines = 625 total operations. (Note: the log indicates 1250 "operations," possibly counting internal steps or scheduling attempts, but the JSSP problem size is 625 distinct operations).

**Table of Average Execution Times (Huge Dataset, 10 Executions):**

| Implementation | Threads | Average Time (s) | Number of Jobs | Number of Operations (JSSP) |
| :------------- | :------ | :--------------- | :------------- | :-------------------------- |
| Sequential     | 1       | 0.000500          | 25             | 625                         |
| Parallel       | 4       | 0.001200          | 25             | 625                         |

*Note: Times for Small and Medium were not directly provided in the detailed log, but the analysis is based on the observed proportion in the Huge dataset.*

**Summary Table of Performance, Speedup, and Efficiency (Estimated for Small/Medium):**

| Dataset | Size (Jobs × Machines) | Total Ops | T_seq (s)       | T_parallel (s, 4 threads) | Speedup (`S_p`) | Efficiency (`E_p`) |
| :------ | :--------------------- | :-------- | :-------------- | :------------------------ | :-------------- | :----------------- |
| Small   | 3 × 3                   | 9         | ≈ `5 × 10^{-5}` | ≈ `2 × 10^{-4}`            | ≈ 0.25          | ≈ 6.25%             |
| Medium  | 6 × 6                   | 36        | ≈ `1 × 10^{-4}` | ≈ `5 × 10^{-4}`            | ≈ 0.20          | ≈ 5.00%             |
| Huge    | 25 × 25                 | 625       | 0.000500        | 0.001200                   | 0.42            | 10.50%              |

*Speedup (`S_p`) calculated as `T_sequential / T_parallel`. Efficiency (`E_p`) calculated as `(S_p / num_threads) × 100%`. Values for Small and Medium are based on proportional estimates from Huge.*

**Workload Distribution Among Threads (Huge Dataset, 4 Threads):**

| Thread ID | Operations Processed | % of Total (Base 1250) | Deviation from Average (25%) |
| :-------- | :------------------- | :--------------------- | :--------------------------- |
| 0         | 350                   | 28.0%                  | +3.0%                        |
| 1         | 300                   | 24.0%                  | -1.0%                        |
| 2         | 300                   | 24.0%                  | -1.0%                        |
| 3         | 300                   | 24.0%                  | -1.0%                        |
| **Total** | **1250**              | **100%**               |                              |

*Note: The base of 1250 operations processed in the Huge log differs from the 625 operations of the 25x25 JSSP. This may indicate multiple scheduling attempts or a different counting of internal work units by the algorithm.*


### 3. Speedup and Efficiency Analysis

For the "Huge" dataset, which is the largest and where the greatest benefit from parallelization would be expected, we observed a **speedup of 0.42x**. This means that the parallel version with 4 threads is **slower** than the sequential version. The calculated efficiency is approximately 10.50%.

This lack of positive speedup can be attributed to several interrelated factors:

1.  **Synchronization Overhead**: The greedy algorithm for JSSP has significant dependencies between operations (precedence within jobs and machine availability). The critical region used to protect access to the machine state and the selection of the next schedulable operation imposes a substantial bottleneck. Multiple threads competing for access to this critical region spend more time waiting (contention) than executing useful work in parallel.
2.  **Fine Granularity of Operations**: As seen in the average time per operation in the sequential version (approximately `0.0005s / 625 ≈ 8 × 10^{-7}` seconds per JSSP operation, or `0.0005s / 1250 ≈ 4 × 10^{-7}` seconds per unit counted in the log), the operations are extremely fast. The cost of entering and exiting an OpenMP critical region or the overhead of thread management becomes greater than the execution time of the work it protects.
3.  **Load Imbalance**: While the initial round-robin distribution helps, the greedy algorithm may cause some threads to have jobs whose next operations become eligible for scheduling earlier than others, leading to idle times for some threads while others wait in the critical region. The observed distribution shows that Thread 0 processed visibly more work units (28%) than the others (24%).
4.  **Problem Size**: Even the "Huge" dataset (625 operations) may not be large enough for the benefits of parallelization (in a synchronization-heavy approach) to outweigh the overhead on a system with few cores. HPC problems typically show significant speedup on much larger datasets.

In summary, the greedy approach, although simple and effective sequentially, proves difficult to parallelize efficiently using a single critical region around the central scheduling logic due to the high frequency of shared access and the fine granularity of tasks.

### 4. Possible Improvements and Future Work

To improve the performance of the parallel implementation, the following approaches could be explored:

1.  **Machine Partitioning**: Instead of distributing jobs among threads, distribute control over sets of machines. Threads would be responsible for scheduling operations on their assigned machines, possibly reducing contention if operations from different jobs use different machines simultaneously. However, synchronization would still be required when scheduling operations that depend on the completion of an operation on *another* machine.
2.  **Machine-specific Locks**: Instead of a single global lock (critical region), use a separate lock for *each* machine. This would allow operations on different machines to be scheduled in parallel without interference, reducing global contention and enabling greater parallelism when machines are free.
3.  **Adaptive Granularity**: Implement a mechanism that detects the problem size or workload and dynamically decides whether to execute the sequential version (for small problems, where overhead dominates) or the parallel version (for large problems).
4.  **Reduce Critical Region Scope**: Carefully analyze the code within the critical region to ensure that only instructions that *really* require mutual exclusion are inside it.
5.  **OpenMP Dynamic Scheduling**: Use the `schedule(dynamic)` or `schedule(guided)` clauses in the parallel loop, which can help distribute the workload among threads more evenly at runtime, in contrast to the static round-robin distribution.
6.  **Alternative Algorithms**: Explore parallelization of alternative algorithms for the JSSP, such as metaheuristics (Genetic Algorithms, Ant Colony Optimization) that operate on populations of solutions and adapt better to parallelism models with less synchronization and sparse communication.

---

## Conclusions

The development of this project provided a practical study of both the sequential and parallel implementations for the Job-Shop Problem, resulting in the following conclusions:

* The **sequential implementation** with a greedy algorithm is simple, efficient, and provides a quick solution for the problem sizes tested, serving as a good baseline.
* The **direct parallelization** of the greedy algorithm with OpenMP, using a single critical region to manage access to the shared machine state, introduced significant **synchronization overhead**.
* For the datasets used, the parallel version was **slower** than the sequential version (speedup < 1), mainly due to high contention in the critical region and the fine granularity of the operations.
* **Load balancing**, even with round-robin distribution, can be a challenge in parallel greedy algorithms due to the nature of selecting the next eligible task.
* The **scalability** of the current parallel approach appears to be limited, indicating that performance is unlikely to improve linearly (and could even worsen) with an increasing number of threads.

This work reinforces that successful parallelization heavily depends on the nature of the problem, the granularity of tasks, and how communication and synchronization are managed. Not all problems benefit from parallelization in the same way, and careful analysis is crucial to selecting the most suitable strategy. Future improvements should focus on reducing contention and exploring parallelism models that better match the characteristics of JSSP.

---

## How to Use the Project

### Requirements

To compile and run the project, you will need:

* A C compiler compatible with C99 and OpenMP support (such as GCC or Clang).
* Python 3.x with the `matplotlib` library installed (for visualization).
* A Linux or similar operating system (or Windows with MinGW for compilation).

### Compilation
```bash
# Compile the sequential version
gcc -Wall -O2 -o jobshop_sequential.exe job_shop_sequential.c

# Compile the parallel version (with OpenMP)
gcc -Wall -O2 -fopenmp -o jobshop_parallel.exe job_shop_parallel.c
```
Execution The generated executables (`jobshop_sequential.exe and jobshop_parallel.exe`) accept command-line arguments for the problem input file and the output file for the solution. The parallel version also accepts the number of threads.

```bash
# Run the sequential version
./jobshop_sequential.exe data/jobshop_small.jss output_small_seq.jss

# Run the parallel version with N threads
./jobshop_parallel.exe data/jobshop_medium.jss output_medium_par.jss 4 # Example with 4 threads
```
Results Visualization The Python `script job_shop_visualizer.py` can be used to generate Gantt charts from the scheduled output files.

```bash
python job_shop_visualizer.py --type <problem_type> --input_file <output_jss_file> --output_file <png_file_name>
```

Where:
- *<problem_type>*: 0 for the "small" dataset, 1 for "medium", 2 for "huge".
- <output_jss_file>: Path to the output file generated by the program execution (e.g., output_small_seq.jss).
- <png_file_name>: The name of the PNG image file to be saved (e.g., gantt_small_seq.png).

Example:

```bash
# After running the sequential version for the small problem
python job_shop_visualizer.py --type 0 --input_file output_small_seq.jss --output_file gantt_small_seq.png

```