/* Executes basic tests on components of the server. */

#include <stdio.h>
#include <stdlib.h>

#include "list.h"

void test_list()
{
	printf("Testing list...\n");

	list_t list;
	list_init(&list);

	for (int i = 0; i < 10; ++i) {
		int* p = (int*)malloc(sizeof(int));
		*p = i + 1;
		if (list_append(&list, (void*)p) < 0)
			printf("FAILED: list_append");
	}

	for (int i = 0; i < 10; ++i) {
		int* p = (int*)list_at(&list, i);
		if (*p != i + 1)
			printf("FAILED: list_at");
	}

	for (int i = 0; i < 5; ++i) {
		list_remove(&list, 0);
	}

	for (int i = 0; i < 5; ++i) {
		int* p = (int*)list_at(&list, i);
		if (*p != i + 6)
			printf("FAILED: list_remove");
	}

	printf("Finished testing list.\n");
}

int main(int argc, char **argv)
{
	test_list();
	return 0;
}