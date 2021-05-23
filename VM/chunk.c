#include <stdlib.h>
#include "chunk.h"
#include "memory.h"

void initChunk(Chunk* chunk) {
	chunk -> count = 0;
	chunk -> capacity = 0;
	chunk -> code = NULL;
	chunk -> lines = NULL;
	initValueArray(&chunk -> constants);
}

void freeChunk(Chunk* chunk) {
	FREE_ARRAY(uint8_t, chunk -> code, chunk -> capacity);
	FREE_ARRAY(int, chunk -> lines, chunk -> capacity);
	freeValueArray(&chunk -> constants);
	initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
	//Check if the current chunk has space, if not we grow it.	
	if (chunk -> capacity < chunk -> count + 1) {
		int oldCapacity = chunk -> capacity;
		chunk -> capacity = GROW_CAPACITY(oldCapacity);
		chunk -> code = GROW_ARRAY(uint8_t, chunk -> code, oldCapacity, chunk -> capacity);
		chunk -> liness = GROW_ARRAY(int, chunk -> lines, oldCapacity, chunk -> capacity);
	}
	//If there is space, put the chunk at the latest spot.
	chunk -> code[chunk -> count] = byte;
	chunk -> lines[chunk -> count] = line;
	chunk -> count++;
}

int addConstant(Chunk* chunk, Value value) {
	writeValueArray(&chunk -> constants, value);
	return chunk -> constants.count - 1;
}