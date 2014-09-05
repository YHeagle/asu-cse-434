/* A list data structure used in the server. */

#ifndef LIST_H
#define LIST_H

typedef struct {
	size_t size;
	size_t capacity;
	void **elements;
} list_t;

void list_init(list_t *list);
void * list_at(list_t *list, size_t index);
int list_append(list_t *list, void *element);
void * list_remove(list_t *list, size_t index);

#endif /* LIST_H */