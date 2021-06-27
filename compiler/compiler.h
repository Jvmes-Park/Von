#ifndef Von_compiler_h
#define Von_compiler_h

#include "../vm/object.h"
#include "../vm/vm.h"

ObjFunction* compile(const char* source);
void markCompilerRoots();

#endif
