#ifndef _IP_LIST_H_
#define _IP_LIST_H_

#include <stddef.h>

/**
 * This list provides a minimal set of functionality as needed
 * by parselinks. If you'd like to extend this list, feel free
 * to do so.
 *
 * To traverse the list:
 *   node_t *curr;
 *   for (curr = list->head; curr != NULL; curr = curr->next) {
 *     // Do something with 'curr'
 *   }
 *
 */

typedef struct node_t {
    void *data;
    struct node_t *next;
} node_t;

typedef struct list_t {
	node_t *head;
} list_t;

/**
 * Allocates memory for the list and does any necessary setup.
 * The user is responsible for freeing the memory by calling
 * list_free, below.
 */
void list_init(list_t **list);

/** 
 * Frees all memory explicitly allocated by the list and sets the 
 * pointer to null.
 */
void list_free(list_t **list);

/**
 * Inserts a new node holding the data at the end of the list (make sure
 * the data is malloced).
 */
void list_append(list_t *list, void *data);

/** 
 * Returns 1 if the list is empty, 0 if not.
 */
int list_empty(list_t *list);

#endif