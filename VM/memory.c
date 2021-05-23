#include <stdlib.h>
#include "memory.h"

//Function to move new array to the new doubled array.
void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
	if (newSize == 0) {
		free(pointer);
		return NULL;
	}
	void* result = realloc(pointer, newSize);
	if (result == NULL)
		exit(1);
	return result;
}
© 2021 GitHub, Inc.
