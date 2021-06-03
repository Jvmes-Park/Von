#ifndef Von_compiler_h
#define Von_compiler_h

#include "../vm/vm.h"

bool compile(const char* source, Chunk* chunk) {
	initScanner(source);
}

#endif
