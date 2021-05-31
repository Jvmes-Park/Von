#include <stdio.h>
#include <stdlib.h>
#include "../vm/common.h"
#include "scanner.h"

typedef struct {
	Token current;
	Token previous;
	bool hadError;
	bool panicMode;
} Parser;

Parser parser;
Chunk* compilingChunk;

static Chunk* currentChunk() {
	return compilingChunk;
}

static void errorAt(Token* token, const char* message) {
	if (parser.panicMode) 
		return;
	parser.panicMode = true;
	fprintf(stderr, "[line %d] Error", token->line);
	if (token -> type == T_EOF) {
		fprintf(stderr, " at end");
	}
	else if (token -> type = T_ERROR) {}
	else {
		fprintf(stderr, " at '%.*s'", token -> length, token -> start);
	}
	fprintf(stderr, ": %s\n", message);
	parser.hadError = true;
}

static void error(const char* message) {
	errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
	errorAt(&parser,current, message);
}

static void advance() {
	parser.previous = parser.current;
	for (;;) {
		parser.current = scanToken();
		if (parser.current.type != T_ERROR)
			break;
		errorAtCurrent(parser.current.start);
	}
}

static void consume(TokenType type, const char* message) {
	if (parser.current.type == type) {
		advance();
		return;
	}
	errorAtCurrent(message);
}

static void emitByte(uint8_t byte) {
	writeChunk(currentChunk(), byte, parser.previous.line);
}

static void endCompiler() {
	emitReturn();
}

void compile(const char* source) {
	initScanner(source);
	compilingChunk = chunk;
	parser.hadError = false;
	parser.panicMode = false;
	advance();
	expression();
	consume(T_EOF, "Expected end of expression.");
	endCompiler();
	return !parser.hadError;
}

