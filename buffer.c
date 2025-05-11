/**
 * CSE 344 - Homework #4
 * Buffer Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "buffer.h"

extern volatile sig_atomic_t terminate;

buffer_t* buffer_create(int capacity) {
    buffer_t *buffer = malloc(sizeof(buffer_t));
    if (buffer == NULL) {
        return NULL;
    }
    
    buffer->items = malloc(capacity * sizeof(char*));
    if (buffer->items == NULL) {
        free(buffer);
        return NULL;
    }
    
    buffer->capacity = capacity;
    buffer->size = 0;
    buffer->front = 0;
    buffer->rear = -1;
    
    // Initialize synchronization primitives
    if (pthread_mutex_init(&buffer->mutex, NULL) != 0) {
        free(buffer->items);
        free(buffer);
        return NULL;
    }
    
    if (pthread_cond_init(&buffer->not_full, NULL) != 0) {
        pthread_mutex_destroy(&buffer->mutex);
        free(buffer->items);
        free(buffer);
        return NULL;
    }
    
    if (pthread_cond_init(&buffer->not_empty, NULL) != 0) {
        pthread_cond_destroy(&buffer->not_full);
        pthread_mutex_destroy(&buffer->mutex);
        free(buffer->items);
        free(buffer);
        return NULL;
    }
    
    return buffer;
}

void buffer_destroy(buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }
    
    // Clean up synchronization primitives
    pthread_mutex_destroy(&buffer->mutex);
    pthread_cond_destroy(&buffer->not_full);
    pthread_cond_destroy(&buffer->not_empty);
    
    // Free any remaining items in the buffer
    for (int i = 0; i < buffer->size; i++) {
        int index = (buffer->front + i) % buffer->capacity;
        free(buffer->items[index]);
    }
    
    free(buffer->items);
    free(buffer);
}

void buffer_add(buffer_t *buffer, char *item) {
    pthread_mutex_lock(&buffer->mutex);
    
    // Wait until there's space in the buffer or termination signal
    while (buffer->size == buffer->capacity && !terminate) {
        pthread_cond_wait(&buffer->not_full, &buffer->mutex);
    }
    
    // Check for termination signal
    if (terminate) {
        pthread_mutex_unlock(&buffer->mutex);
        if (item != NULL) {
            free(item);  // Free the item if we're terminating
        }
        return;
    }
    
    // Add item to buffer
    buffer->rear = (buffer->rear + 1) % buffer->capacity;
    buffer->items[buffer->rear] = item;
    buffer->size++;
    
    // Signal that buffer is not empty
    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);
}

char* buffer_remove(buffer_t *buffer) {
    pthread_mutex_lock(&buffer->mutex);
    
    // Wait until there's an item in the buffer or termination signal
    while (buffer->size == 0 && !terminate) {
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
    }
    
    // Check for termination signal
    if (terminate) {
        pthread_mutex_unlock(&buffer->mutex);
        return NULL;
    }
    
    // If buffer is empty after waiting, something went wrong
    if (buffer->size == 0) {
        pthread_mutex_unlock(&buffer->mutex);
        return NULL;
    }
    
    // Remove item from buffer
    char *item = buffer->items[buffer->front];
    buffer->front = (buffer->front + 1) % buffer->capacity;
    buffer->size--;
    
    // Signal that buffer is not full
    pthread_cond_signal(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);
    
    return item;
}

void buffer_signal_all(buffer_t *buffer) {
    pthread_mutex_lock(&buffer->mutex);
    pthread_cond_broadcast(&buffer->not_empty);
    pthread_cond_broadcast(&buffer->not_full);
    pthread_mutex_unlock(&buffer->mutex);
}