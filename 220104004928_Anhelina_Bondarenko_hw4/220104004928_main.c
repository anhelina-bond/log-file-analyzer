/**
 * CSE 344 - Homework #4
 * Multithreaded Log File Analyzer
 * valgrind ./LogAnalyzer 10 4 logs/sample.log "ERROR"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include "buffer.h"

// Global variables for cleanup on signal
buffer_t *buffer = NULL;
pthread_t *workers = NULL;
int num_workers = 0;
pthread_barrier_t barrier;
volatile sig_atomic_t terminate = 0;

// Function prototypes
void *manager_thread(void *arg);
void *worker_thread(void *arg);
void signal_handler(int sig);

typedef struct {
    buffer_t *buffer;
    char *search_term;
    int worker_id;
    int matches_found;
} worker_args_t;

typedef struct {
    buffer_t *buffer;
    char *filename;
} manager_args_t;

void cleanup_resources() {
    if (buffer != NULL) {
        buffer_destroy(buffer);
    }
    if (workers != NULL) {
        free(workers);
    }
    pthread_barrier_destroy(&barrier);
}

void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nReceived SIGINT, cleaning up and exiting...\n");
        terminate = 1;
        if (buffer != NULL) {
            buffer_signal_all(buffer); // Wake up any waiting threads
        }
    }
}

void *manager_thread(void *arg) {
    manager_args_t *args = (manager_args_t *)arg;
    buffer_t *buffer = args->buffer;
    char *filename = args->filename;
    
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        // Add EOF markers for each worker to signal them to exit
        for (int i = 0; i < num_workers; i++) {
            buffer_add(buffer, NULL);
        }
        terminate = 1;
        return NULL;
    }
    
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    
    // Read file line by line
    while (!terminate) {
        read = getline(&line, &len, file);
        if (read == -1) {
            if (feof(file)) {
                break; // End of file
            } else if (errno == EINTR) {
                // Interrupted by signal, check if we need to terminate
                if (terminate) {
                    break;
                } else {
                    continue; // Retry the read
                }
            } else {
                perror("Error reading file");
                break;
            }
        }
        
        // Remove newline character if present
        if (read > 0 && line[read-1] == '\n') {
            line[read-1] = '\0';
        }
        
        // Make a copy of the line to store in buffer
        char *line_copy = strdup(line);
        if (line_copy == NULL) {
            perror("Memory allocation error");
            terminate = 1;
            break;
        }
        
        // Add line to buffer
        buffer_add(buffer, line_copy);
    }
    
    // Add EOF marker to signal end of file (unconditionally)
    for (int i = 0; i < num_workers; i++) {
        buffer_add(buffer, NULL);
    }
    
    free(line);
    fclose(file);
    return NULL;
}

void *worker_thread(void *arg) {
    worker_args_t *args = (worker_args_t *)arg;
    buffer_t *buffer = args->buffer;
    char *search_term = args->search_term;
    int worker_id = args->worker_id;
    int matches = 0;
    
    while (!terminate) {
        char *line = buffer_remove(buffer);
        
        // Check if it's the EOF marker
        if (line == NULL) {
            break;
        }
        
        // Search for the keyword
        if (strstr(line, search_term) != NULL) {
            printf("Worker %d found match: %s\n", worker_id, line);
            matches++;
        }
        
        free(line);
    }
    
    args->matches_found = matches;
    pthread_barrier_wait(&barrier); // Wait for all workers to finish
    
    // Only one thread prints the final summary
    if (worker_id == 0 && !terminate) {
        printf("\n--- Summary Report ---\n");
        int total_matches = 0;
        for (int i = 0; i < num_workers; i++) {
            worker_args_t *worker_arg = (worker_args_t *)(args - worker_id + i);
            printf("Worker %d found %d matches\n", i, worker_arg->matches_found);
            total_matches += worker_arg->matches_found;
        }
        printf("Total matches found: %d\n", total_matches);
    }
    
    return NULL;
}

int main(int argc, char *argv[]) {
    // Check command line arguments
    if (argc != 5) {
        printf("Usage: %s <buffer_size> <num_workers> <log_file> <search_term>\n", argv[0]);
        return 1;
    }
    
    // Parse command line arguments
    int buffer_size = atoi(argv[1]);
    num_workers = atoi(argv[2]);
    char *log_file = argv[3];
    char *search_term = argv[4];
    
    // Validate arguments
    if (buffer_size <= 0 || num_workers <= 0) {
        printf("Buffer size and number of workers must be positive integers\n");
        return 1;
    }
    
    // Register signal handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    
    // Initialize buffer
    buffer = buffer_create(buffer_size);
    if (buffer == NULL) {
        printf("Failed to create buffer\n");
        return 1;
    }
    
    // Initialize barrier
    if (pthread_barrier_init(&barrier, NULL, num_workers) != 0) {
        perror("Failed to initialize barrier");
        buffer_destroy(buffer);
        return 1;
    }
    
    // Create worker threads and arguments
    workers = malloc(num_workers * sizeof(pthread_t));
    worker_args_t *worker_args = malloc(num_workers * sizeof(worker_args_t));
    
    if (workers == NULL || worker_args == NULL) {
        perror("Memory allocation error");
        cleanup_resources();
        free(worker_args);
        return 1;
    }
    
    // Create manager arguments
    manager_args_t manager_args = {
        .buffer = buffer,
        .filename = log_file
    };
    
    // Start worker threads
    for (int i = 0; i < num_workers; i++) {
        worker_args[i].buffer = buffer;
        worker_args[i].search_term = search_term;
        worker_args[i].worker_id = i;
        worker_args[i].matches_found = 0;
        
        if (pthread_create(&workers[i], NULL, worker_thread, &worker_args[i]) != 0) {
            perror("Failed to create worker thread");
            terminate = 1;
            break;
        }
    }
    
    // Start manager thread
    pthread_t manager;
    if (pthread_create(&manager, NULL, manager_thread, &manager_args) != 0) {
        perror("Failed to create manager thread");
        terminate = 1;
    }
    
    // Wait for manager thread to finish
    pthread_join(manager, NULL);
    
    // If termination was requested, ensure EOF markers are added
    if (terminate) {
        for (int i = 0; i < num_workers; i++) {
            buffer_add(buffer, NULL);
        }
    }
    
    // Wait for worker threads to finish
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i], NULL);
    }
    
    // Clean up resources
    free(worker_args);
    cleanup_resources();
    
    return 0;
}