#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "list.h"

void list_init(list_t **list)
{
	*list = (list_t *)malloc(sizeof(list_t));
	memset(*list, 0, sizeof(list_t));
}

void list_free(list_t **list)
{
	if ((*list) != NULL) { 
		node_t *curr, *next;
		for (curr = (*list)->head; curr != NULL; curr = next) {
			next = curr->next;
			free(curr);
		}
		free(*list);
		*list = NULL;
	}
}

void list_append(list_t *list, void *data)
{
	node_t *new_node = (node_t *)malloc(sizeof(node_t));
	memset(new_node, 0, sizeof(node_t));
	new_node->data = data;
	
	if (list_empty(list)) {
		list->head = new_node;
	} else {
		node_t *curr;
		for (curr = list->head; curr->next != NULL; curr = curr->next);
		curr->next = new_node;
	}	
}

int list_empty (list_t *list)
{
	return list->head == NULL;
}
