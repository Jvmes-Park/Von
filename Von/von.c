#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../VM/common.h"
#include "../VM/chunk.h"
#include "../VM/debug.h"
#include "../VM/vm.h"

void indent() {
	printf(">> ");
}

static void REPL() {
	char line[1024];
	for (;;) {
		indent();
		if (!fgets(line, sizeof(line), stdin)) {
			printf("\n");
			break;
		}
		intepret(line);
	}
}

static char* readFile(const char* path) {
	FILE* file = fopen(path, "rb");
	if (file == NULL) {
		fprintf(stderr, "Could not open file \"%s\".\n", path);
		exit(74);
	}
	fseek(file, 0L, SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);

	char* buffer = (char*)malloc(fileSize + 1);
	size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
	if (bytesRead < fileSize) {
		fprintf(stderr, "Could not read file \"%s\".\n", path);
		exit(74);
	}
	buffer[bytesRead] = '\0';
	fclose(file);
	return buffer;
}

static void runFile(const char* path) {
	char* source = readFile(path);
	IntepretResult result = interpret(source);
	free(source);

	if (result == INTERPRET_COMPILE_ERROR) exit(65);
	if (result == INTEPRET_RUNTIME_ERROR) exit(70);
}

int main (int argc, const char* argv[]) {
	initVM();
	if (argc == 1) {
		REPL();
	}
	else if (argc == 2) {
		runFile(argv[1]);
	}
	else {
		fprintf(stderr, "Usage: Von [path]\n");
		exit(64);
	}
	freeVM();
	freeChunk(&chunk);
	return 0;
}