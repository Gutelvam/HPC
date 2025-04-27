#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h> /* for _mkdir on Windows */
#endif

#define MAX_JOBS 100
#define MAX_MACHINES 100
#define MAX_OPERATIONS 100
#define MAX_LOG_ENTRIES 10000

/**
 * Structure to represent an operation in the scheduling problem
 */
typedef struct {
    int machine;         // The machine this operation must be processed on
    int duration;        // The time required to complete this operation
    int start_time;      // The scheduled start time of this operation (-1 if not scheduled)
} Operation;

/**
 * Structure to track execution data for logging purposes
 */
typedef struct {
    int job;             // Job ID
    int op;              // Operation ID
    double start_time;   // Wall clock time when operation was scheduled
    double duration;     // Wall clock duration of scheduling this operation
} ExecutionLogEntry;

/**
 * Structure to represent the entire Job Shop Scheduling Problem
 */
typedef struct {
    int num_jobs;                        // Number of jobs
    int num_machines;                    // Number of machines
    int num_operations;                  // Number of operations per job
    Operation jobs[MAX_JOBS][MAX_OPERATIONS]; // All operations for all jobs

    // Logging information
    ExecutionLogEntry log_entries[MAX_LOG_ENTRIES]; // Log of execution sequence
    int log_count;                       // Number of logged entries
} JobShopProblem;

/**
 * Create logs directory if it doesn't exist
 */
void ensure_logs_directory() {
    #ifdef _WIN32
    _mkdir("logs");
    #else
    mkdir("logs", 0777);
    #endif
}

/**
 * Read the problem definition from an input file
 *
 * @param filename Path to the input file
 * @param problem Array representing the problem structure
 * @return 1 if successful, 0 if failed
 */
int read_input_file(char filename[], JobShopProblem problem[]) {
    // Open input file
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        return 0;
    }

    // Read problem dimensions
    fscanf(file, "%d %d", &problem[0].num_jobs, &problem[0].num_machines);

    // Initialize the number of operations (assuming it equals the number of machines)
    problem[0].num_operations = problem[0].num_machines;

    // Initialize log counter
    problem[0].log_count = 0;

    // Read all operations data
    for (int job = 0; job < problem[0].num_jobs; job++) {
        for (int op = 0; op < problem[0].num_operations; op++) {
            fscanf(file, "%d %d",
                   &problem[0].jobs[job][op].machine,
                   &problem[0].jobs[job][op].duration);
            problem[0].jobs[job][op].start_time = -1; // Mark as unscheduled
        }
    }

    fclose(file);
    return 1;
}

/**
 * Write the solution to an output file
 *
 * @param filename Path to the output file
 * @param problem Array representing the problem structure
 */
void write_output_file(char filename[], JobShopProblem problem[]) {
    // Open output file
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        return;
    }

    // Calculate makespan (completion time of the last operation)
    int makespan = 0;
    for (int job = 0; job < problem[0].num_jobs; job++) {
        for (int op = 0; op < problem[0].num_operations; op++) {
            int end_time = problem[0].jobs[job][op].start_time + problem[0].jobs[job][op].duration;
            if (end_time > makespan) {
                makespan = end_time;
            }
        }
    }

    // Write makespan to output file
    fprintf(file, "%d\n", makespan);

    // Write all operation schedules to output file
    for (int job = 0; job < problem[0].num_jobs; job++) {
        for (int op = 0; op < problem[0].num_operations; op++) {
            fprintf(file, "%d,%d ",
                    problem[0].jobs[job][op].start_time,
                    problem[0].jobs[job][op].duration);
        }
        fprintf(file, "\n");
    }

    fclose(file);
}

/**
 * Find an available time slot for an operation on a specific machine
 *
 * @param problem Array representing the problem structure
 * @param machine The machine ID
 * @param duration The operation duration
 * @param earliest_start The earliest possible start time
 * @return The earliest available start time
 */
int find_available_time(JobShopProblem problem[], int machine, int duration, int earliest_start) {
    int start_time = earliest_start;

    while (1) {
        int end_time = start_time + duration;
        int can_schedule = 1;
        int earliest_possible = start_time;

        // Check all scheduled operations on this machine for conflicts
        for (int job = 0; job < problem[0].num_jobs; job++) {
            for (int op = 0; op < problem[0].num_operations; op++) {
                // Only consider operations that are already scheduled on this machine
                if (problem[0].jobs[job][op].start_time != -1 &&
                    problem[0].jobs[job][op].machine == machine) {
                    int op_start = problem[0].jobs[job][op].start_time;
                    int op_end = op_start + problem[0].jobs[job][op].duration;

                    // Check for overlap between the proposed time and the existing operation
                    if ((start_time < op_end) && (end_time > op_start)) {
                        can_schedule = 0;
                        // Update the earliest possible time to try next
                        if (op_end > earliest_possible) {
                            earliest_possible = op_end;
                        }
                    }
                }
            }
        }

        // If we found a valid time slot, return it
        if (can_schedule) {
            return start_time;
        }
        // Otherwise, try the next earliest possible time
        start_time = earliest_possible;
    }
}

/**
 * Schedule jobs sequentially using a greedy algorithm
 *
 * @param problem Array representing the problem structure
 */
void schedule_jobs_sequential(JobShopProblem problem[]) {
    int scheduled_ops[MAX_JOBS] = {0};   // Number of operations scheduled per job
    int earliest_starts[MAX_JOBS] = {0}; // Earliest start time for the next operation of each job
    int total_ops = problem[0].num_jobs * problem[0].num_operations;
    int scheduled_count = 0;

    // Continue until all operations are scheduled
    while (scheduled_count < total_ops) {
        // Find the job with the earliest possible start time for its next operation
        int earliest_job = -1;
        int earliest_time = 999999;

        // Track timing for this scheduling step
        clock_t step_start = clock();

        for (int job = 0; job < problem[0].num_jobs; job++) {
            // Only consider jobs that still have operations to schedule
            if (scheduled_ops[job] < problem[0].num_operations) {
                // Find the job with the earliest possible start time
                if (earliest_starts[job] < earliest_time) {
                    earliest_time = earliest_starts[job];
                    earliest_job = job;
                }
            }
        }

        if (earliest_job == -1) break; // Should never happen in a valid problem

        // Get the next operation to schedule for this job
        int op = scheduled_ops[earliest_job];
        int machine = problem[0].jobs[earliest_job][op].machine;
        int duration = problem[0].jobs[earliest_job][op].duration;
        int job_earliest_start = earliest_starts[earliest_job];

        // Find a valid start time for this operation on its machine
        int start_time = find_available_time(problem, machine, duration, job_earliest_start);

        // Schedule the operation
        problem[0].jobs[earliest_job][op].start_time = start_time;
        scheduled_ops[earliest_job]++;
        scheduled_count++;

        // Update the earliest start time for the next operation of this job
        // (ensures operations within a job are processed in sequence)
        if (scheduled_ops[earliest_job] < problem[0].num_operations) {
            earliest_starts[earliest_job] = start_time + duration;
        }

        // Record the execution log entry
        clock_t step_end = clock();
        double step_duration = ((double)(step_end - step_start)) / CLOCKS_PER_SEC;

        // Only log if we have space
        if (problem[0].log_count < MAX_LOG_ENTRIES) {
            problem[0].log_entries[problem[0].log_count].job = earliest_job;
            problem[0].log_entries[problem[0].log_count].op = op;
            problem[0].log_entries[problem[0].log_count].start_time = ((double)step_start) / CLOCKS_PER_SEC;
            problem[0].log_entries[problem[0].log_count].duration = step_duration;
            problem[0].log_count++;
        }
    }
}

/**
 * Save the execution log to files
 *
 * @param problem Array representing the problem structure
 * @param input_base_name Base name of the input file (for naming log files)
 */
void save_execution_logs(JobShopProblem problem[], const char *input_base_name) {
    // Ensure logs directory exists
    ensure_logs_directory();
    
    // Create filenames based on input name
    char timing_filename[256];
    char sequence_filename[256];
    sprintf(timing_filename, "logs/%s_timing_sequential.txt", input_base_name);
    sprintf(sequence_filename, "logs/%s_sequence_sequential.txt", input_base_name);

    // Save timing summary
    FILE* timing_file = fopen(timing_filename, "w");
    if (timing_file == NULL) {
        return;
    }

    // Write header
    fprintf(timing_file, "Total Operations | Total Time (s) | Avg Time per Op (s)\n");
    fprintf(timing_file, "------------------------------------------------------\n");

    // Calculate total execution time
    double total_time = 0.0;
    for (int i = 0; i < problem[0].log_count; i++) {
        total_time += problem[0].log_entries[i].duration;
    }

    // Write summary
    double avg_time = (problem[0].log_count > 0) ? (total_time / problem[0].log_count) : 0.0;
    fprintf(timing_file, "%16d | %13.8f | %16.8f\n",
            problem[0].log_count, total_time, avg_time);

    fclose(timing_file);

    // Save execution sequence
    FILE* sequence_file = fopen(sequence_filename, "w");
    if (sequence_file == NULL) {
        return;
    }

    // Write header
    fprintf(sequence_file, "Execution Order | Job | Operation | Time (s)\n");
    fprintf(sequence_file, "------------------------------------------\n");

    // Write each log entry
    for (int i = 0; i < problem[0].log_count; i++) {
        fprintf(sequence_file, "%14d | %3d | %9d | %0.8f seconds\n",
                i+1,
                problem[0].log_entries[i].job,
                problem[0].log_entries[i].op,
                problem[0].log_entries[i].duration);
    }

    fclose(sequence_file);
}

/**
 * Main function
 */
int main(int argc, char* argv[]) {
    // Check command line arguments
    if (argc != 3) {
        return 1;
    }

    // Get input and output filenames
    char input_file[256];
    char output_file[256];
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
        *dot = '\0'; // Truncate at the dot
    }

    // Initialize problem structure as an array
    JobShopProblem problem[1];

    // Read problem from input file
    if (!read_input_file(input_file, problem)) {
        return 1;
    }

    // Ensure the logs directory exists
    ensure_logs_directory();

    // Run multiple repetitions to get average performance
    const int num_repetitions = 10;
    double total_time = 0.0;

    for (int i = 0; i < num_repetitions; i++) {
        // Reset start times for each repetition
        for (int job = 0; job < problem[0].num_jobs; job++) {
            for (int op = 0; op < problem[0].num_operations; op++) {
                problem[0].jobs[job][op].start_time = -1;
            }
        }

        // Reset log count (only keep logs for the last run)
        if (i < num_repetitions - 1) {
            problem[0].log_count = 0;
        }

        // Run the scheduler and time it
        clock_t start = clock();
        schedule_jobs_sequential(problem);
        clock_t end = clock();

        double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;
        total_time += time_taken;
    }
    
    // Calculate average execution time
    double avg_time = total_time / num_repetitions;

    // Save detailed execution logs from the last run
    save_execution_logs(problem, input_base_name);

    // Write solution to output file
    write_output_file(output_file, problem);

    // Save summary to a file
    char summary_filename[256];
    sprintf(summary_filename, "logs/%s_execution_times.txt", input_base_name);
    FILE* time_file = fopen(summary_filename, "a");
    if (time_file != NULL) {
        fprintf(time_file, "Input: %s, Sequential, Avg Time: %.6f seconds\n",
                input_base_name, avg_time);
        fclose(time_file);
    }

    return 0;
}