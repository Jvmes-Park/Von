#include <stdio.h>
#include <string.h>
#include "../vm/common.h"
#include "scanner.h"

typedef struct {
	const char* start;
	const char* current;
	int line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
	scanner.start = source;
	scanner.current = source;
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

static char advance() {
	scanner.current++;
	return scanner.current[-1];
}

static bool match(char expected) {
	if (isAtEnd())
		return false;
	if (*scanner.current != expected)
		return false;
	scanner.current++;
	return true;
}
	
static char peek() {
	return *scanner.current;
}

static char peekNext() {
	if (isAtEnd())
		return '\0';
	return scanner.current[1];
}

static void skipWhiteSpace() {
	for(;;) {
		char c = peek();
		switch (c) {
			case ' ':
			case '\r':
			case '\t':
				advance();
				break;
			case '\n':
				scanner.line++;
				advance();
				break;
			case '#':
				while (peek() != '\n' && !isAtEnd())
					advance();
					break;
			default:
				return;
		}
	}
}

static Token String() {
	while (peek() != '"' && !isAtEnd()) {
		if (peek() == '\n')
			scanner.line++;
		advance();
	}
	if (isAtEnd())
		return errorToken("Unterminated String.");
	advance();
	return makeToken(T_STRING);
}

static bool isDigit(char c) {
	return c >= '0' && c <= '9';
}

static Token Number() {
	while (isDigit(peek()))
		advance();
	if (peek() == '.' && isDigit(peekNext())) {
		advance();
		while (isDigit(peek()))
			advance();
	}
	return makeToken(T_NUMBER);
}

static bool isAlpha(char c) {
	return (c >= 'a' && c <= 'z' ||
		c >= 'A' && c <= 'Z' ||
		c == '_');
}

static TokenType checkKeyword(int start, int length, const char* rest, TokenType type) {
	if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0) {
		return type;
	}
	return T_IDENTIFIER;
}

static TokenType identifierType() {
	switch (scanner.start[0]) {
		case 'a':
			return checkKeyword(1, 2, "nd", T_AND);
		case 'e':
			return checkKeyword(1, 3, "lse", T_ELSE);
		case 'n':
			return checkKeyword(1, 2, "il", T_NIL);
		case 'o':
			return checkKeyword(1, 1, "r", T_OR);
		case 'p':
			return checkKeyword(1, 4, "rint", T_PRINT);
		case 'r':
			return checkKeyword(1, 5, "eturn", T_RETURN);
		case 'v':
			return checkKeyword(1, 2, "ar", T_VAR);
		case 'w':
			return checkKeyword(1, 4, "hile", T_WHILE);
		case 'd':
			return checkKeyword(1, 6, "efault", T_DEFAULT);
		case 'c':
			if (scanner.current - scanner.start > 1) {
				switch(scanner.start[1]) {
					case 'l':
						return checkKeyword(2, 3, "ass", T_CLASS);
					case 'a':
						return checkKeyword(2, 2, "se", T_CASE);
				}
			}
		case 's':
			if (scanner.current - scanner.start > 1) {
				switch(scanner.start[1]) {
					case 'u':
						return checkKeyword(2, 3, "uper", T_SUPER);
					case 'w':
						return checkKeyword(2, 4, "itch", T_SWITCH);
				}
			}
		case 'i':
			if (scanner.current - scanner.start > 1) {
				switch(scanner.start[1]) {
					case 'm':
						return checkKeyword(2, 4, "port", T_IMPORT);
					case 'f':
						return checkKeyword(1, 0, "", T_IF);
				}
			}
		case '.':
			if (scanner.current - scanner.start > 1) {
				switch(scanner.start[1]) {
					case 'h':
						return checkKeyword(2, 3, "elp", T_HELP);
					case 'e':
						return checkKeyword(2, 3, "xit", T_EXIT);
				}
			}
			break;
		case 'f':
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1]) {
					case 'a':
						return checkKeyword(2, 3, "lse", T_FALSE);
					case 'o':
						return checkKeyword(2, 1, "r", T_OR);
					case 'u':
						return checkKeyword(2, 1, "n", T_FUN);
				}
			}
			break;
		case 't':
			if (scanner.current - scanner.start > 1) {
				switch (scanner.start[1]) {
					case 'h':
						return checkKeyword(2, 3, "his", T_THIS);
					case 'r':
						return checkKeyword(2, 2, "ue", T_TRUE);
				}
			}
			break;
	}
	return T_IDENTIFIER;
}

static Token identifier() {
	while (isAlpha(peek()) || isDigit(peek()))
		advance();
	return makeToken(identifierType());
}

Token scanToken() {
	skipWhiteSpace();
	scanner.start = scanner.current;
	if (isAtEnd()) 
		return makeToken(T_EOF);
	char c = advance();
	if (isAlpha(c))
		return identifier();
	if (isDigit(c))
		return Number();
	switch(c) {
		case '(':
			return makeToken(T_LEFT_PAREN);
		case ')':
			return makeToken(T_RIGHT_PAREN);
		case '{':
			return makeToken(T_LEFT_BRACE);
		case '}':
			return makeToken(T_RIGHT_BRACE);
		case ';':
			return makeToken(T_SEMI_COLON);
		case ',':
			return makeToken(T_COMMA);
		case '.':
			return makeToken(T_DOT);
		case '-':
			return makeToken(T_MINUS);
		case '+':
			return makeToken(T_PLUS);
		case '/':
			return makeToken(T_SLASH);
		case '*':
			return makeToken(T_STAR);
		case '!':
			return makeToken(
				match('=') ? T_BANG_EQUAL : T_BANG);
		case '=':
			return makeToken(
				match('=') ? T_EQUAL_EQUAL : T_EQUAL);
		case '<':
			return makeToken(
				match('=') ? T_LESS_EQUAL : T_LESS);
		case '>':
			return makeToken(
				match('=') ? T_GREATER_EQUAL : T_GREATER);
		case ':': 
			return makeToken(T_COLON);
		case '"':
			return String();
	}
	return errorToken("Unexpected character.");
}
