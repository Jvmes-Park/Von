#include <stdio.h>
#include <string.h>
#include "../vm/common.h"
#include "../vm/scanner.h"

typedef struct {
	const char* start;
	const char* current;
	int line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
	scanner.start = start;
	scanner.current = current;
	scanner.line = 1;
}

static bool isAtEnd() {
	return *scanner.current == '\0';
}

static Token makeToken(TokenType type) {
	Token token;
	token.type = type;
	token.start = scanner.start;
	token.length = (int)(scanner.current - scanner.start);
	token.line = scanner.line;
	return token;
}

static Token errorToken(const char* message) {
	Token token;
	token.type = T_ERROR;
	token.start = message;
	token.length = (int)strlen(message);
	token.line = scanner.line;
	return token;
}

Token scanToken() {
	scanner.start = scanner.current;
	if (isAtEnd()) 
		return makeToken(T_EOF);
	return errorToken("Unexpected character.");
}
