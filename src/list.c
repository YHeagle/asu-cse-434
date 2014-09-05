/* A list data structure used in the server. */

#include <string.h>
#include <stdlib.h>

#include "list.h"

/* Initializes the list. */
void list_init(list_t *list)
{
	memset(list, 0, sizeof(list_t));
}

/* Returns the pointer held at the specified index. */
void * list_at(list_t *list, size_t index)
{
	if (index < 0 || index >= list->size)
		return (void*)0;

	return list->elements[index];
}

/* Appends the given pointer to the list. Returns 0 if successful,
   -1 if unsuccessful. */
int list_append(list_t *list, void *element)
{
	if (list->capacity == 0) {
		void **p = (void**)malloc(sizeof(void*) * 4);
		if (!p)
			return -1;
		list->elements = p;
		list->capacity = 4;
		list->elements[0] = element;
		list->size = 1;
	} else if (list->size == list->capacity) {
		void **p = (void**)malloc(sizeof(void*) * list->capacity * 2);
		if (!p)
			return -1;
		memcpy(p, list->elements, sizeof(void*) * list->size);
		free(list->elements);
		list->elements = p;
		list->capacity = list->capacity * 2;
		list->elements[list->size] = element;
		list->size = list->size + 1;
	} else {
		list->elements[list->size] = element;
		list->size = list->size + 1;
	}

	return 0;
}

/* Removes and returns the pointer at the given index. */
void * list_remove(list_t *list, size_t index)
{
	if (index < 0 || index >= list->size)
		return (void*)0;

	void *element = list->elements[index];
	memmove(list->elements + index, list->elements + index + 1, 
		sizeof(void*) * (list->size - index - 1));
	list->size = list->size - 1;
	return element;
}