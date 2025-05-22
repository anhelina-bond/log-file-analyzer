/**
 * CSE 344 - Homework #4
 * Buffer Header File
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>

/**
 * Buffer structure for thread-safe producer-consumer pattern
 * Implemented as a circular queue for efficiency
 */
typedef struct {
    char **items;           // Array of string pointers
    int capacity;           // Maximum buffer size
    int size;               // Current number of items
    int front;              // Index of the front item
    int rear;               // Index of the last item
    pthread_mutex_t mutex;  // Mutex for thread safety
    pthread_cond_t not_full;    // Condition variable for producers
    pthread_cond_t not_empty;   // Condition variable for consumers
} buffer_t;

/**
 * Create a new buffer with specified capacity
 * 
 * @param capacity Maximum number of items the buffer can hold
 * @return Pointer to newly created buffer, or NULL on failure
 */
buffer_t* buffer_create(int capacity);

/**
 * Destroy buffer and free all resources
 * 
 * @param buffer Pointer to buffer to destroy
 */
void buffer_destroy(buffer_t *buffer);

/**
 * Add an item to the buffer
 * Blocks if buffer is full until space is available
 * 
 * @param buffer Pointer to the buffer
 * @param item String to add to buffer (will be NULL for EOF marker)
 */
void buffer_add(buffer_t *buffer, char *item);

/**
 * Remove and return an item from the buffer
 * Blocks if buffer is empty until an item is available
 * 
 * @param buffer Pointer to the buffer
 * @return String removed from buffer (caller must free), or NULL for EOF marker
 */
char* buffer_remove(buffer_t *buffer);

/**
 * Signal all threads waiting on buffer conditions
 * Used for cleanup when terminating
 * 
 * @param buffer Pointer to the buffer
 */
void buffer_signal_all(buffer_t *buffer);

#endif /* BUFFER_H */