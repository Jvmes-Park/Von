#include <stdio.h>
#include <stdlib.h>
#include "../vm/common.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "../vm/debug.h"
#endif

typedef struct {
	Token current;
	Token previous;
	bool hadError;
	bool panicMode;
} Parser;

typedef enum {
	P_NONE,
	P_ASSIGNMENT,
	P_OR,
	P_AND,
	P_EQUALITY,
	P_COMPARISON,
	P_TERM,
	P_FACTOR,
	P_UNARY,
	P_CALL,
	P_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef void (*ParseFn)();

typedef struct {
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

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

static bool match(TokenType type) {
	if (!check(type))
		return false;
	advance();
	return true;
}

static bool check(TokenType type) {
	return parser.current.type == type;
}

static void emitByte(uint8_t byte) {
	writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
	emitByte(byte1);
	emitByte(byte2);
}

static void emitReturn() {
	emitByte(OP_RETURN);
}

static uin8_t makeConstant(Value value) {
	int constant = addConstant(currentChunk(), value);
	if (constant > UINT_MAX) {
		error("Too many constants in one chunk.");
		return 0;
	}
	return (uint8_t)constant;
}

static void emitConstant(Value value) {
	emitBytes(OP_CONSTANT, makeConstant(value));
}

static void endCompiler() {
	emitReturn();
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		disassembleChunk(currentChunk(), "code");
	}
#endif
}

static ParseRule* getRule(TokenType type) {
	return &rules[type];
}

static void parsePrecendence(Precendence precedence) {
	advance();
	ParseFn prefixRule = getRule(parser.previous.type) -> prefix;
	if (prefixRule == NULL) {
		error("Expect expression.");
		return;
	}
	prefixRule();
	while (precedence <= getRule(parser.current.type) -> precedence) {
		advance();
		ParseFn infixRule = getRule(parser.previous.type) -> infix;
		infixRule(canAssign);
	}
	if (canAssign && match(T_EQUAL)) {
		error("Invalid assignment target.");
	}
}

bool canAssign = precedence <= P_ASSIGNMENT;
prefixRule(canAssign);

static uint8_t identifierConstant(Token* name) {
	return makeConstant(OBJ_VAL(copyString(name -> start, name -> length)));
}

static uint8_t parseVariable(const char* errorMessage) {
	consume(T_IDENTIFIER, errorMessage);
	return identifierConstatn(&parser.previous);
}

static void defineVariable(uint8_t global) {
	emitBytes(OP_DEFINE_GLOBAL, global);
}

static void expression();
static void statement();
static void declaration();
static void ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static void binary(bool canAssign) {
	TokenType operatorType = parser.previous.type;
	ParseRule* rule = getRule(operatorType);
	parsePrecedence((Precedence)(rule -> precedence + 1));
	switch (operatorType) {
		case T_PLUS:
			emitByte(OP_ADD);
			break;
		case T_MINUS:
			emitByte(OP_SUBTRACT);
			break;
		case T_STAR:
			emitByte(OP_MULTIPLY);
			break;
		case T_SLASH:
			emitByte(OP_DIVIDE);
			break;
		case T_BANG_EQUAL:
			emitBytes(OP_EQUAL, OP_NOT);
			break;
		case T_EQUAL_EQUAL:
			emitByte(OP_EQUAL);
			break;
		case T_GREATER:
			emitByte(OP_GREATER);
			break;
		case T_GREATER_EQUAL:
			emitBytes(OP_LESS, OP_NOT);
			break;
		case T_LESS:
			emitByte(OP_LESS);
			break;
		case T_LESS_EQUAL:
			emitBytes(OP_GREATER, OP_NOT);
			break;	
		default:
			return;
	}
}

static void literal(bool canAssign) {
	switch(parser.previous.type) {
		case T_FALSE:
			emitByte(OP_FALSE);
			break;
		case T_NIL:
			emitByte(OP_NIL);
			break;
		case T_TRUE:
			emitByte(OP_TRUE);
			break;
		default:
			return;
	}
}

static void grouping(bool canAssign) {
	expression();
	consume(T_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign) {
	emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign) {
	emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void namedVariable(Token name, bool canAssign) {
	uint8_t arg = identifierConstant(&name);
	if (canAssign && match(T_EQUAL)) {
		expression();
		emitBytes(OP_SET_GLOBAL, arg);
	}
	else {
		emitBytes(OP_GET_GLOBAL, arg);
	}
}

static void variable(bool canAssign) {
	namedVariable(parser.previous);
}

static void unary() {
	TokenType operatorType = parser.previous.type;
	parsePrecedence(P_UNARY);
	switch(operatorType) {
		case T_MINUS: 
			emitByte(OP_NEGATE);
			break;
		case T_BANG:
			emitByte(OP_NOT);
			break;
		default:
			return;
	}
}

ParseRule rules[] = {
	[T_LEFT_PAREN] = {grouping, NULL, P_NONE},
	[T_RIGHT_PAREN] = {NULL, NULL, P_NONE},
	[T_LEFT_BRACE] = {NULL, NULL, P_NONE},
	[T_RIGHT_BRACE] = {NULL, NULL, P_NONE},	
	[T_COMMA] = {NULL, NULL, P_NONE},	
	[T_DOT] = {NULL, NULL, P_NONE},	
	[T_MINUS] = {unary, binary, P_TERM},	
	[T_PLUS] = {NULL, binary, P_TERM},	
	[T_SEMI_COLON] = {NULL, NULL, P_NONE},	
	[T_SLASH] = {NULL, binary, P_FACTOR},	
	[T_STAR] = {NULL, binary, P_FACTOR},	
	[T_EQUAL] = {NULL, NULL, P_NONE},
	[T_EQUAL_EQUAL] = {NULL, binary, P_EQUALITY},
	[T_BANG] = {unary, NULL, P_NONE},	
	[T_BANG_EQUAL] = {NULL, binary, P_EQUALITY},	
	[T_LESS] = {NULL, binary, P_COMPARISON},	
	[T_LESS_EQUAL] = {NULL, binary, P_COMPARISON},	
	[T_GREATER] = {NULL, binary, P_COMPARISON},	
	[T_GREATER_EQUAL] = {NULL, binary, P_COMPARISON},	
	[T_IDENTIFIER] = {variable, NULL, P_NONE},	
	[T_STRING] = {string, NULL, P_NONE},	
	[T_NUMBER] = {number, NULL, P_NONE},	
	[T_AND] = {NULL, NULL, P_NONE},	
	[T_CLASS] = {NULL, NULL, P_NONE},	
	[T_ELSE] = {NULL, NULL, P_NONE},	
	[T_FALSE] = {literal, NULL, P_NONE},	
	[T_FOR] = {NULL, NULL, P_NONE},	
	[T_FUN] = {NULL, NULL, P_NONE},	
	[T_IF] = {NULL, NULL, P_NONE},	
	[T_NIL] = {literal, NULL, P_NONE},	
	[T_OR] = {NULL, NULL, P_NONE},	
	[T_PRINT] = {NULL, NULL, P_NONE},	
	[T_RETURN] = {NULL, NULL, P_NONE},	
	[T_SUPER] = {NULL, NULL, P_NONE},	
	[T_THIS] = {NULL, NULL, P_NONE},	
	[T_TRUE] = {NULL, NULL, P_NONE},	
	[T_VAR] = {NULL, NULL, P_NONE},	
	[T_WHILE] = {NULL, NULL, P_NONE},	
	[T_ERROR] = {NULL, NULL, P_NONE},	
	[T_EOF] = {NULL, NULL, P_NONE},	
};

static void expression() {
	parsePrecedence(P_ASSIGNMENT);
}

static void varDeclaration() {
	uint32_t global = parseVariable("Expect variable name");
	if (match(T_EQUAL)) {
		expression();
	}
	else {
		emitByte(OP_NIL);
	}
	consume(T_SEMI_COLON, "Expect ';' after variable declaration");
	defineVariable(global);
}

static void expressionStatement() {
	expression();
	consume(T_SEMI_COLON, "Expect ';' after expression.");
	emitByte(OP_POP);
}

static void printStatement() {
	expression();
	consume(T_SEMI_COLON, "Expect ';' after value.");
	emitByte(OP_PRINT);
}

static void synchronize() {
	parser.panicMode = false;
	while (parser.current.type != T_EOF) {
		if (parser.previous.type == T_SEMI_COLON)
			return;
		switch (parser.current.type) {
			case T_CLASS:
			case T_FUN:
			case T_VAR:
			case T_FOR:
			case T_IF:
			case T_WHILE:
			case T_PRINT:
			case T_RETURN:
				return;
			default:
				;
		}
		advance();
	}
}

static void declaration() {
	if (match(T_VAR)) {
		varDeclaration();
	}
	else {
		statement();
	}
	if (parser.panicMode)
		synchronize();
}

static void statement() {
	if (match(T_PRINT)) {
		printStatement();
	}
	else {
		expressionStatement();
	}
}

void compile(const char* source) {
	initScanner(source);
	compilingChunk = chunk;
	parser.hadError = false;
	parser.panicMode = false;
	advance();
	while (!match(T_EOF)) {
		declaration();
	}
	endCompiler();
	return !parser.hadError;
}

