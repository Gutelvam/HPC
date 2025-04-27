#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <omp.h>

/**
 * Parallel Job Shop Scheduling Problem (JSSP) Solver - Enforced Work Distribution
 *
 * This version enforces a strict distribution of operations across threads for small problems.
 * It implements a round-robin job assignment strategy to ensure all threads get work.
 */

#define MAX_JOBS 100
#define MAX_MACHINES 100
#define MAX_OPERATIONS 100
#define INF 999999 
#define MAX_LOG_ENTRIES 10000
#define MAX_THREADS 32

/**
 * Structure to represent an operation in the scheduling problem
 */
typedef struct {
    int machine;     // The machine this operation must be processed on
    int duration;    // The time required to complete this operation
    int start_time;  // The scheduled start time of this operation (-1 if not scheduled)
} Operation;

/**
 * Structure to track execution data for logging purposes
 */
typedef struct {
    int job;            // Job ID
    int op;             // Operation ID  
    double start_time;  // Wall clock time when operation was scheduled
    double duration;    // Wall clock duration of scheduling this operation
} ThreadLogEntry;

/**
 * Structure to represent the entire Job Shop Scheduling Problem
 */
typedef struct {
    int num_jobs;                           // Number of jobs
    int num_machines;                       // Number of machines
    int num_operations;                     // Number of operations per job
    Operation jobs[MAX_JOBS][MAX_OPERATIONS]; // All operations for all jobs
    
    // Thread logging data (statically allocated - no pointers)
    ThreadLogEntry thread_logs[MAX_THREADS][MAX_LOG_ENTRIES]; // Log data for each thread
    int thread_log_count[MAX_THREADS];      // Count of log entries per thread
} JobShopProblem;

// Global problem instance
JobShopProblem problem;

/**
 * Initialize the thread logging data structures
 *
 * @param num_threads Number of threads to initialize logs for
 */
void init_thread_logs(int num_threads) {
    if (num_threads > MAX_THREADS) {
        printf("Error: Requested %d threads, but maximum is %d\n", num_threads, MAX_THREADS);
        exit(1);
    }
    
    for (int i = 0; i < num_threads; i++) {
        problem.thread_log_count[i] = 0;
    }
}

/**
 * Read the problem definition from an input file
 *
 * @param filename Path to the input file
 * @return 1 if successful, 0 if failed
 */
int read_input_file(char filename[]) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening input file: %s\n", filename);
        return 0;
    }

    fscanf(file, "%d %d", &problem.num_jobs, &problem.num_machines);
    problem.num_operations = problem.num_machines; // Assuming num_ops == num_machines

    if (problem.num_jobs > MAX_JOBS || problem.num_machines > MAX_MACHINES) {
         printf("Error: Problem size exceeds MAX limits (%d/%d jobs, %d/%d machines)\n",
                problem.num_jobs, MAX_JOBS, problem.num_machines, MAX_MACHINES);
         fclose(file);
         return 0;
    }

    for (int job = 0; job < problem.num_jobs; job++) {
        for (int op = 0; op < problem.num_operations; op++) {
            if (fscanf(file, "%d %d",
                       &problem.jobs[job][op].machine,
                       &problem.jobs[job][op].duration) != 2) {
                 printf("Error reading operation data for job %d, op %d\n", job, op);
                 fclose(file);
                 return 0;
            }
             if (problem.jobs[job][op].machine < 0 || problem.jobs[job][op].machine >= problem.num_machines) {
                 printf("Error: Invalid machine number %d for job %d, op %d (max machines %d)\n",
                        problem.jobs[job][op].machine, job, op, problem.num_machines);
                 fclose(file);
                 return 0;
            }
            problem.jobs[job][op].start_time = -1; // Not scheduled yet
        }
    }

    fclose(file);
    return 1;
}

/**
 * Write the solution to an output file
 *
 * @param filename Path to the output file
 */
void write_output_file(char filename[]) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("Error opening output file: %s\n", filename);
        return;
    }

    int makespan = 0;
    
    // Check all operations for makespan
    for (int job = 0; job < problem.num_jobs; job++) {
        for (int op = 0; op < problem.num_operations; op++) {
            if (problem.jobs[job][op].start_time != -1) {
                int end_time = problem.jobs[job][op].start_time + problem.jobs[job][op].duration;
                if (end_time > makespan) {
                    makespan = end_time;
                }
            }
        }
    }

    fprintf(file, "%d\n", makespan);

    for (int job = 0; job < problem.num_jobs; job++) {
        for (int op = 0; op < problem.num_operations; op++) {
            fprintf(file, "%d,%d ",
                    problem.jobs[job][op].start_time,
                    problem.jobs[job][op].duration);
        }
        fprintf(file, "\n");
    }

    fclose(file);
}

/**
 * Find an available time slot for an operation on a specific machine
 *
 * @param machine The machine ID
 * @param duration The operation duration
 * @param earliest_start The earliest possible start time
 * @return The earliest available start time
 */
int find_available_time(int machine, int duration, int earliest_start) {
    int start_time = earliest_start;

    while (1) {
        int end_time = start_time + duration;
        int can_schedule = 1;
        int earliest_possible_next_try = start_time;

        // Check all previously scheduled operations on this machine
        for (int job = 0; job < problem.num_jobs; job++) {
            for (int op = 0; op < problem.num_operations; op++) {
                // Check only operations already scheduled AND on the target machine
                if (problem.jobs[job][op].start_time != -1 &&
                    problem.jobs[job][op].machine == machine)
                {
                    int op_start = problem.jobs[job][op].start_time;
                    int op_end = op_start + problem.jobs[job][op].duration;

                    // Check for overlap
                    if (start_time < op_end && end_time > op_start) {
                        can_schedule = 0;
                        if (op_end > earliest_possible_next_try) {
                            earliest_possible_next_try = op_end;
                        }
                    }
                }
            }
        }

        if (can_schedule) {
            return start_time; // Found a valid slot
        } else {
            start_time = earliest_possible_next_try;
        }
    }
}

/**
 * Explicit work division approach for small problems
 * Each thread is assigned specific operations to schedule
 * 
 * @param num_threads Number of threads to use
 * @return 1 if successful
 */
int schedule_with_strict_division(int num_threads) {
    // Calculate total operations
    int total_ops = problem.num_jobs * problem.num_operations;
    
    // Create arrays to track scheduling state
    int scheduled_ops[MAX_JOBS] = {0};
    int earliest_starts[MAX_JOBS] = {0};
    int op_assigned[MAX_JOBS][MAX_OPERATIONS];
    int assigned_count = 0;
    int iteration_count = 0;
    int max_iterations = total_ops * 10; // Safety limit
    
    // Job-based assignment instead of operation-based assignment
    // Assign entire jobs to threads to avoid dependency issues
    for (int job = 0; job < problem.num_jobs; job++) {
        int thread = job % num_threads;
        for (int op = 0; op < problem.num_operations; op++) {
            op_assigned[job][op] = thread;
        }
    }
    
    // Flag to track if each thread actually did work
    int thread_worked[MAX_THREADS] = {0};
    
    // Keep scheduling until all operations are scheduled
    while (assigned_count < total_ops && iteration_count < max_iterations) {
        iteration_count++;
        int scheduled_this_iteration = 0;
        
        // Each thread tries to schedule operations
        #pragma omp parallel num_threads(num_threads) reduction(+:scheduled_this_iteration)
        {
            int thread_id = omp_get_thread_num();
            int thread_scheduled = 0;
            
            // Each thread checks all operations in sequence
            #pragma omp critical (scheduler)
            {
                // Try to schedule any operations this thread is responsible for
                for (int job = 0; job < problem.num_jobs; job++) {
                    // Only process jobs assigned to this thread
                    if (op_assigned[job][0] == thread_id) {
                        // Check if there's an operation ready to be scheduled
                        int op = scheduled_ops[job];
                        if (op < problem.num_operations) {
                            // This operation is ready to be scheduled
                            double op_start_time = omp_get_wtime();
                            
                            // Get operation details
                            int machine = problem.jobs[job][op].machine;
                            int duration = problem.jobs[job][op].duration;
                            int job_earliest_start = earliest_starts[job];
                            
                            // Find available time on the machine
                            int start_time = find_available_time(machine, duration, job_earliest_start);
                            
                            // Schedule the operation
                            problem.jobs[job][op].start_time = start_time;
                            scheduled_ops[job]++; // Increment count of scheduled operations for this job
                            assigned_count++;
                            thread_scheduled = 1;
                            scheduled_this_iteration++;
                            
                            // Update earliest start time for next operation of this job
                            earliest_starts[job] = start_time + duration;
                            
                            double op_end_time = omp_get_wtime();
                            double op_duration = op_end_time - op_start_time;
                            
                            // Log this operation
                            int log_idx = problem.thread_log_count[thread_id];
                            if (log_idx < MAX_LOG_ENTRIES) {
                                problem.thread_logs[thread_id][log_idx].job = job;
                                problem.thread_logs[thread_id][log_idx].op = op;
                                problem.thread_logs[thread_id][log_idx].start_time = op_start_time;
                                problem.thread_logs[thread_id][log_idx].duration = op_duration;
                                problem.thread_log_count[thread_id]++;
                            }
                        }
                    }
                }
            } // End critical section
            
            if (thread_scheduled) {
                thread_worked[thread_id] = 1;
            }
        } // End parallel region
        
        // If no progress was made in this iteration, we might have a deadlock
        if (scheduled_this_iteration == 0) {
            // Check if all jobs have been distributed among active threads
            int all_jobs_assigned = 1;
            for (int job = 0; job < problem.num_jobs; job++) {
                if (scheduled_ops[job] < problem.num_operations) {
                    // If we find a job with unscheduled operations, redistribute it
                    // Find an idle thread (one that hasn't worked yet)
                    for (int t = 0; t < num_threads; t++) {
                        if (thread_worked[t] == 0) {
                            // Reassign this job to the idle thread
                            for (int op = 0; op < problem.num_operations; op++) {
                                op_assigned[job][op] = t;
                            }
                            all_jobs_assigned = 0;
                            break;
                        }
                    }
                }
            }
            
            // If all jobs are assigned but we still can't make progress, we're deadlocked
            if (all_jobs_assigned) {
                printf("Warning: Possible deadlock detected after %d iterations. Using fallback approach.\n", 
                       iteration_count);
                
                // Fallback: Single-threaded approach to complete remaining operations
                for (int job = 0; job < problem.num_jobs; job++) {
                    for (int op = scheduled_ops[job]; op < problem.num_operations; op++) {
                        double op_start_time = omp_get_wtime();
                        
                        // Get operation details
                        int machine = problem.jobs[job][op].machine;
                        int duration = problem.jobs[job][op].duration;
                        int job_earliest_start = earliest_starts[job];
                        
                        // Find available time on the machine
                        int start_time = find_available_time(machine, duration, job_earliest_start);
                        
                        // Schedule the operation
                        problem.jobs[job][op].start_time = start_time;
                        scheduled_ops[job]++; // Increment count of scheduled operations for this job
                        assigned_count++;
                        
                        // Update earliest start time for next operation of this job
                        earliest_starts[job] = start_time + duration;
                        
                        double op_end_time = omp_get_wtime();
                        double op_duration = op_end_time - op_start_time;
                        
                        // Log this operation - assign to the first thread for simplicity
                        int thread_id = 0;
                        int log_idx = problem.thread_log_count[thread_id];
                        if (log_idx < MAX_LOG_ENTRIES) {
                            problem.thread_logs[thread_id][log_idx].job = job;
                            problem.thread_logs[thread_id][log_idx].op = op;
                            problem.thread_logs[thread_id][log_idx].start_time = op_start_time;
                            problem.thread_logs[thread_id][log_idx].duration = op_duration;
                            problem.thread_log_count[thread_id]++;
                        }
                    }
                }
                break; // Exit the main while loop
            }
        }
    }
    
    // Verify all operations were scheduled
    if (assigned_count != total_ops) {
        printf("Error: Only %d of %d operations were scheduled after %d iterations!\n", 
               assigned_count, total_ops, iteration_count);
        return 0;
    }
    
    return 1;
}

/**
 * Reset all operation start times to not scheduled (-1)
 */
void reset_problem() {
    for (int job = 0; job < problem.num_jobs; job++) {
        for (int op = 0; op < problem.num_operations; op++) {
            problem.jobs[job][op].start_time = -1;
        }
    }
}

/**
 * Save thread execution logs to files
 *
 * @param num_threads Number of threads with logs
 * @param input_base_name Base name of the input file (for naming log files)
 */
void save_thread_logs(int num_threads, const char *input_base_name) {
    // Create logs directory if it doesn't exist
    #ifdef _WIN32
    system("if not exist logs mkdir logs");
    #else
    system("mkdir -p logs");
    #endif
    
    // Create filenames based on input name in logs directory
    char thread_timing_filename[256];
    char thread_sequence_filename[256];
    sprintf(thread_timing_filename, "logs/%s_timing_%d_threads.txt", input_base_name, num_threads);
    sprintf(thread_sequence_filename, "logs/%s_sequence_%d_threads.txt", input_base_name, num_threads);
    
    // File for thread timing summary
    FILE* timing_file = fopen(thread_timing_filename, "w");
    if (timing_file == NULL) {
        printf("Error: Failed to open %s for writing\n", thread_timing_filename);
        return;
    }
    
    // Write header
    fprintf(timing_file, "Thread ID | Operation Count | Total Time (s) | Avg Time per Op (s)\n");
    fprintf(timing_file, "---------------------------------------------------------------\n");
    
    // File for detailed thread execution sequence
    FILE* sequence_file = fopen(thread_sequence_filename, "w");
    if (sequence_file == NULL) {
        printf("Error: Failed to open %s for writing\n", thread_sequence_filename);
        fclose(timing_file);
        return;
    }
    
    // Write header for sequence file
    fprintf(sequence_file, "Thread ID | Job | Operation | Time (s)\n");
    fprintf(sequence_file, "----------------------------------------\n");
    
    // Process each thread's data
    for (int thread = 0; thread < num_threads; thread++) {
        double total_time = 0.0;
        int count = problem.thread_log_count[thread];
        
        // Calculate total execution time for this thread
        for (int i = 0; i < count; i++) {
            total_time += problem.thread_logs[thread][i].duration;
            
            // Write detailed sequence entry
            fprintf(sequence_file, "Thread %2d | Job %2d | Op %2d | %0.8f seconds\n", 
                    thread, 
                    problem.thread_logs[thread][i].job,
                    problem.thread_logs[thread][i].op,
                    problem.thread_logs[thread][i].duration);
        }
        
        // Write summary entry
        double avg_time = (count > 0) ? (total_time / count) : 0.0;
        fprintf(timing_file, "Thread %2d | %14d | %12.8f | %16.8f\n", 
                thread, count, total_time, avg_time);
    }
    
    fclose(timing_file);
    fclose(sequence_file);
}

/**
 * Main function
 */
int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <input_file> <output_file> <num_threads>\n", argv[0]);
        return 1;
    }

    char input_file[256];
    char output_file[256];
    int num_threads = atoi(argv[3]);
    
    if (num_threads <= 0) {
        printf("Error: Number of threads must be positive.\n");
        return 1;
    }
    
    strcpy(input_file, argv[1]);
    strcpy(output_file, argv[2]);

    // Extract base name from input file for log file naming
    char input_base_name[256] = {0};
    char *lastSlash = strrchr(input_file, '/');
    char *lastBackslash = strrchr(input_file, '\\');
    char *filename = input_file;
    
    if (lastSlash != NULL) {
        filename = lastSlash + 1;
    } else if (lastBackslash != NULL) {
        filename = lastBackslash + 1;
    }
    
    // Remove extension if present
    strcpy(input_base_name, filename);
    char *dot = strrchr(input_base_name, '.');
    if (dot != NULL) {
        *dot = '\0';  // Truncate at the dot
    }
    
    // Initialize problem
    init_thread_logs(num_threads);

    // Read problem from input file
    if (!read_input_file(input_file)) {
        return 1;
    }

    // Calculate problem size
    int total_ops = problem.num_jobs * problem.num_operations;
    
    // For small problems, limit thread count to avoid overhead
    int effective_num_threads = num_threads;
    if (effective_num_threads > total_ops) {
        effective_num_threads = total_ops;
    }
    if (effective_num_threads > 8 && total_ops < 100) {
        effective_num_threads = 8; // For small problems, don't use too many threads
    }
    
    if (effective_num_threads < 1) effective_num_threads = 1;
    
    const int num_repetitions = 10;
    double total_time = 0.0;
    double start_time_total, end_time_total;
    
    start_time_total = omp_get_wtime();

    // Execute multiple times for timing
    for (int i = 0; i < num_repetitions; i++) {
        // Reset problem for each iteration
        reset_problem();
        
        // For all iterations except the last, clear thread logs
        if (i < num_repetitions - 1) {
            for (int t = 0; t < effective_num_threads; t++) {
                problem.thread_log_count[t] = 0;
            }
        }

        double start = omp_get_wtime();
        
        // Use the strict division approach for better thread distribution
        schedule_with_strict_division(effective_num_threads);
        
        double end = omp_get_wtime();
        total_time += (end - start);
    }
    
    end_time_total = omp_get_wtime();
    
    // Save logs from the last iteration
    save_thread_logs(effective_num_threads, input_base_name);

    // Calculate and print average execution time
    double avg_time = total_time / num_repetitions;
    printf("Average execution time (parallel): %.6f seconds\n", avg_time);
    printf("Total time for %d repetitions: %.6f seconds\n", 
           num_repetitions, end_time_total - start_time_total);

    // Write solution to output file
    write_output_file(output_file);
    printf("Output written to %s\n", output_file);

    // Save overall execution summary
    char summary_filename[256];
    sprintf(summary_filename, "logs/%s_execution_times.txt", input_base_name);
    
    // Create logs directory if it doesn't exist (for the summary file)
    #ifdef _WIN32
    system("if not exist logs mkdir logs");
    #else
    system("mkdir -p logs");    
    #endif
    
    FILE* time_file = fopen(summary_filename, "a");
    if (time_file != NULL) {
        fprintf(time_file, "Input: %s, Requested Threads: %d, Effective Threads: %d, Avg Time: %.6f seconds\n", 
                input_base_name, num_threads, effective_num_threads, avg_time);
        fclose(time_file);
    } else {
        printf("Warning: Could not open %s for appending.\n", summary_filename);
    }

    return 0;
}