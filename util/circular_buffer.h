#ifndef __CIRCULAR_BUFFER_H__
#define __CIRCULAR_BUFFER_H__

/**
 * circular_buffer.h
 * 
 * Stephen Poletto (spoletto)
 * Walter Blaurock (wblauroc)
 * Computer Networks -- Spring 2011
 */

#include <pthread.h>
#include <stdint.h>

typedef struct {
    unsigned char *write_pointer;
    unsigned char *read_pointer;
    pthread_mutex_t pointer_lock;
    pthread_cond_t cond_read;
    pthread_cond_t cond_write;
    unsigned char *data;
    uint16_t capacity; // Total capacity of the buffer
    uint16_t size;     // Length of all data contained in circular buffer
    int eof;
} circular_buffer_t;

/**
 * Call this function to initialize a circular buffer.
 * The caller is responsible for freeing memory associated
 * with the struct; however, a function is provided for this
 * purpose below.
 */
void circular_buffer_init(circular_buffer_t **cbuf, uint16_t capacity);

/**
 * Frees all memory associated with this circular buffer
 * and sets pointer to null to ensure no further use.
 */
void circular_buffer_free(circular_buffer_t **cbuf);

/**
 * Returns max size of this buffer.
 */
uint16_t circular_buffer_get_capacity(circular_buffer_t *cbuf);

/**
 * Returns the size of data contained by this buffer.
 */
uint16_t circular_buffer_get_size(circular_buffer_t *cbuf);
 
/**
 * Checks if the buffer is empty.
 */
int circular_buffer_is_empty(circular_buffer_t *cbuf);

/**
 * Checks if the buffer is full.
 */
int circular_buffer_is_full(circular_buffer_t *cbuf);

/**
 * Write data to the circular buffer. Return the number
 * of bytes copied. Blocks until at least some of the data
 * is written. Same behavior as write.
 */
int circular_buffer_write(circular_buffer_t *cbuf, const void *buf, size_t count);

/**
 * Read data from the circular buffer. Blocks until at least
 * some data is available. Some behavior as read.
 */
int circular_buffer_read(circular_buffer_t *cbuf, void *buf, size_t count);
 
/**
 * Print buffer contents as if they are characters. Will only print
 * the buffer contents between read_pointer and read_pointer + size.
 */
void circular_buffer_print_unread_contents(circular_buffer_t *cbuf);

/**
 * Returns the number of bytes left in this circular buffer.
 */
uint16_t circular_buffer_get_available_capacity(circular_buffer_t *cbuf);
 
/** 
 * Signal that the data being written into the buffer has reached
 * end of file. Subsequent calls to circular_buffer_write will fail
 * and return 0. circular_buffer_read will continue to read data
 * from the buffer until its size falls to 0, at which point it will
 * return 0.
 */
void circular_buffer_signal_eof(circular_buffer_t *cbuf);
 
#endif