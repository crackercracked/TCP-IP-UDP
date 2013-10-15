/**
 * circular_buffer.c
 * 
 * Stephen Poletto (spoletto)
 * Walter Blaurock (wblauroc)
 * Computer Networks -- Spring 2011
 */

#include "circular_buffer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Uncomment this to enable debugging.
// #define CIRCULAR_BUFFER_DEBUG

/**
 * Print buffer contents as if they are characters.
 */
void circular_buffer_print_contents(circular_buffer_t *cbuf);

/**
 * Calculate the number of slots in the buffer from this pointer (inclusive)
 * until the end of the buffer in memory. For example:
 *
 * [0,1,2,3,4,5,6,7,8,9]
 *            	^
 *
 * The result of circular_buffer_get_bytes_until_wrap(cbuf, 6) would be 4.
 *
 * Another way to think of this is as how many bytes can be written starting
 * at the current ptr before requiring a wrap around.
 */
uint16_t circular_buffer_get_bytes_until_wrap(circular_buffer_t *cbuf, unsigned char *ptr);

void circular_buffer_init(circular_buffer_t **cbuf, uint16_t capacity)
{
    // Allocate some memory for this buffer.
    *cbuf = (circular_buffer_t *) malloc(sizeof(circular_buffer_t));

    // Initialize the mutex (reentrant please!)
    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&(*cbuf)->pointer_lock, &mutexattr);
	
    // Initialize the condition variables
    pthread_cond_init(&(*cbuf)->cond_read, 0);
    pthread_cond_init(&(*cbuf)->cond_write, 0);
	
    // Initialize the data array
    (*cbuf)->data = (unsigned char *) malloc(capacity);
	
#ifdef CIRCULAR_BUFFER_DEBUG
    memset((*cbuf)->data, 0, capacity);
#endif
	
    // Initialize remaining fields
    (*cbuf)->write_pointer = (*cbuf)->data;
    (*cbuf)->read_pointer = (*cbuf)->data;
    (*cbuf)->capacity = capacity;
    (*cbuf)->size = 0;
    (*cbuf)->eof = 0;
}

void circular_buffer_free(circular_buffer_t **cbuf)
{
    // Only continue if the buffer is not null
    if (*cbuf != 0) {
	free((*cbuf)->data);
	free (*cbuf);
	*cbuf = 0;
    }
}

uint16_t circular_buffer_get_capacity(circular_buffer_t *cbuf)
{
    if (cbuf == NULL) {
        return 0;
    }
    uint16_t ret;
    pthread_mutex_lock(&cbuf->pointer_lock);
    ret = cbuf->capacity;
    pthread_mutex_unlock(&cbuf->pointer_lock);
    return ret;
}
 
uint16_t circular_buffer_get_size(circular_buffer_t *cbuf)
{
    if (cbuf == NULL) {
        return 0;
    }
    uint16_t ret;
    pthread_mutex_lock(&cbuf->pointer_lock);
    ret = cbuf->size;
    pthread_mutex_unlock(&cbuf->pointer_lock);
    return ret;
}

int circular_buffer_is_empty(circular_buffer_t *cbuf)
{
    if (cbuf == NULL) {
        return 1;
    }
    int ret;
    pthread_mutex_lock(&cbuf->pointer_lock);
    ret = (cbuf->size == 0);
    pthread_mutex_unlock(&cbuf->pointer_lock);
    return ret;
}

int circular_buffer_is_full(circular_buffer_t *cbuf)
{
    if (cbuf == NULL) {
        return 1;
    }
    int ret;
    pthread_mutex_lock(&cbuf->pointer_lock);
    ret = (cbuf->size == cbuf->capacity);
    pthread_mutex_unlock(&cbuf->pointer_lock);
    return ret;
}

int circular_buffer_write(circular_buffer_t *cbuf, const void *buf, size_t count)
{
    if (cbuf == NULL) {
        return -1;
    }
    pthread_mutex_lock(&cbuf->pointer_lock);
    if (cbuf->eof) {
	pthread_mutex_unlock(&cbuf->pointer_lock);
	return 0;
    }

    int ret = -1;
    // Loop until we manually break out (when some data is written) -- skip if count == 0
    while (count > 0) {
	// If there is room to write some data, write as much as possible.
	if (!circular_buffer_is_full(cbuf)) {
	    // Determine exactly how much data we can write.
	    uint16_t length_to_write = count;
	    uint16_t available_capacity = circular_buffer_get_available_capacity(cbuf);
	    if (length_to_write > available_capacity) {
		length_to_write = available_capacity;
	    }
	    
#ifdef CIRCULAR_BUFFER_DEBUG
	    printf("writing %d bytes of data (of %d passed in)\n", length_to_write, count);
#endif
	    // Calculate bytes to write before we wrap
	    uint16_t bytes_until_wrap = circular_buffer_get_bytes_until_wrap(cbuf, cbuf->write_pointer);
	    
	    // Don't wrap if we dont have to
	    if (bytes_until_wrap > length_to_write) {
		memcpy(cbuf->write_pointer, buf, length_to_write);
		cbuf->write_pointer += length_to_write;    
	    } else {
		// Copy all bytes that will fit before wrap.
		memcpy(cbuf->write_pointer, buf, bytes_until_wrap);
		
		// Copy remaining bytes after we wrap (from beginning).
		memcpy(cbuf->data, buf + bytes_until_wrap, length_to_write - bytes_until_wrap);
		
		// Update pointer
		cbuf->write_pointer = cbuf->data + (length_to_write - bytes_until_wrap);
	    }
	    
	    // Update size, set return value, and wake up someone waiting to read the data.
	    cbuf->size += length_to_write;
	    ret = length_to_write;
	    pthread_cond_signal(&cbuf->cond_read);
	    break;
	} else {
#ifdef CIRCULAR_BUFFER_DEBUG
	    printf("Circular buffer sleeping as there is no room to write right now!\n");
#endif
	    // Sleep until room is available to write
	    pthread_cond_wait(&cbuf->cond_write, &cbuf->pointer_lock);
	}
    }
    
#ifdef CIRCULAR_BUFFER_DEBUG
    circular_buffer_print_contents(cbuf);
#endif
    
    pthread_mutex_unlock(&cbuf->pointer_lock);
    return ret;
}

int circular_buffer_read(circular_buffer_t *cbuf, void *buf, size_t count)
{
    if (cbuf == NULL) {
        return -1;
    }
    pthread_mutex_lock(&cbuf->pointer_lock);
	
    int ret = -1;
    // Loop until we manually break out (when some data is read) -- skip if count == 0
    while (count > 0) {
	// If there is data to be read, read as much as possible.
	if (!circular_buffer_is_empty(cbuf)) {	
	    // Determine exactly how much data we can read.
	    uint16_t length_to_read = count;
	    if (length_to_read > cbuf->size) {
		length_to_read = cbuf->size;
	    }
#ifdef CIRCULAR_BUFFER_DEBUG		
	    printf("read %d bytes of data (of %d requested)\n", length_to_read, count);
#endif
	    // Calculate bytes to read before we wrap.
	    uint16_t bytes_until_wrap = circular_buffer_get_bytes_until_wrap(cbuf, cbuf->read_pointer);
	    
	    // Don't wrap if we dont have to
	    if (bytes_until_wrap > length_to_read) {
		memcpy(buf, cbuf->read_pointer, length_to_read);
#ifdef CIRCULAR_BUFFER_DEBUG
		// 0 bytes for easier debugging
		memset(cbuf->read_pointer, 0, length_to_read);
#endif
		cbuf->read_pointer += length_to_read;
	    } else {
		// Copy all bytes that will fit before wrap.
		memcpy(buf, cbuf->read_pointer, bytes_until_wrap);
		
#ifdef CIRCULAR_BUFFER_DEBUG
		// 0 bytes for easier debugging
		memset(cbuf->read_pointer, 0, bytes_until_wrap);
#endif
		// Copy remaining bytes after we wrap (from beginning).
		memcpy(buf + bytes_until_wrap, cbuf->data, length_to_read - bytes_until_wrap);
		
#ifdef CIRCULAR_BUFFER_DEBUG
		// 0 bytes for easier debugging
		memset(cbuf->data, 0, length_to_read - bytes_until_wrap);
#endif
		// Update pointer
		cbuf->read_pointer = cbuf->data + (length_to_read - bytes_until_wrap);
	    }
	    
	    // Update size, set return value and wake up someone waiting to write data.
	    cbuf->size -= length_to_read;
	    ret = length_to_read;
	    pthread_cond_signal(&cbuf->cond_write);
	    break;
	} else {
	    if (cbuf->eof) {
		ret = 0;
		break;
	    }
#ifdef CIRCULAR_BUFFER_DEBUG
	    printf("Going to sleep...\n");
#endif
	    // Sleep until there is some data available.
	    pthread_cond_wait(&cbuf->cond_read, &cbuf->pointer_lock);
	}
    }
    
#ifdef CIRCULAR_BUFFER_DEBUG
    circular_buffer_print_contents(cbuf);
#endif
    
    pthread_mutex_unlock(&cbuf->pointer_lock);
    return ret;
}

uint16_t circular_buffer_get_available_capacity(circular_buffer_t *cbuf)
{
    if (cbuf == NULL) {
        return 0;
    }
    uint16_t ret;
    pthread_mutex_lock(&cbuf->pointer_lock);
    ret = cbuf->capacity - cbuf->size;
    pthread_mutex_unlock(&cbuf->pointer_lock);
    return ret;
}

void circular_buffer_signal_eof(circular_buffer_t *cbuf) {
     if (cbuf == NULL) {
        return;
     }
     pthread_mutex_lock(&cbuf->pointer_lock);
     cbuf->eof = 1;
     pthread_cond_signal(&cbuf->cond_read);
     pthread_mutex_unlock(&cbuf->pointer_lock);
}
     
void circular_buffer_print_unread_contents(circular_buffer_t *cbuf) 
{
    if (cbuf == NULL) {
        return;
    }
    pthread_mutex_lock(&cbuf->pointer_lock);
    printf("Circular buffer contents: ");
    
    if (!circular_buffer_is_empty(cbuf)) {
        char *buf = (char *)malloc(cbuf->size);
        bzero(buf, cbuf->size);
        // Calculate bytes to read before we wrap
        uint16_t bytes_until_wrap = circular_buffer_get_bytes_until_wrap(cbuf, cbuf->read_pointer);
        uint16_t length_to_read = cbuf->size;	
	
        // Don't wrap if we dont have to
        if (bytes_until_wrap > length_to_read) {
            memcpy(buf, cbuf->read_pointer, length_to_read);			
        } else {			
            // Copy all bytes that will fit before wrap
            memcpy(buf, cbuf->read_pointer, bytes_until_wrap);		
            // Copy remaining bytes after we wrap (from beginning)
            memcpy(buf + bytes_until_wrap, cbuf->data, length_to_read - bytes_until_wrap);	
        }
        printf("%s", buf);
        free(buf);
    }
    
    printf("\n");
    pthread_mutex_unlock(&cbuf->pointer_lock);
}

/* Private function definitions */

uint16_t circular_buffer_get_bytes_until_wrap(circular_buffer_t *cbuf, unsigned char *ptr)
{
    uint16_t ret;
    pthread_mutex_lock(&cbuf->pointer_lock);
    // Memory begins at 14, capacity is 8, ptr = 18, ret = 4.
    ret = cbuf->data + cbuf->capacity - ptr;
    pthread_mutex_unlock(&cbuf->pointer_lock);
    return ret;
}

void circular_buffer_print_contents(circular_buffer_t *cbuf)
{
    pthread_mutex_lock(&cbuf->pointer_lock);
    printf("Circular buffer contents: ");
    int index;
    for (index = 0; index < cbuf->capacity; index++) {
	if (cbuf->data[index] != 0) {
	    printf("%c", cbuf->data[index]);
	} else {
	    printf(".");
	}
    }
    printf("\n");
    pthread_mutex_unlock(&cbuf->pointer_lock);
}
