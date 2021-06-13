#include <stdio.h>
#include <stdlib.h>
#include <string.h>	

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

typedef struct {
	Token name;
	int depth;
} Local;

typedef enum {
	TYPE_FUNCTION,
	TYPE_SCRIPT
} FunctionType;

typedef struct Compiler {
	struct Compiler* enclosing;
	ObjFunction* function;
	FunctionType type;
	Local locals[UINT8_COUNT];
	int localCount;
	int scopeDepth;
};

Parser parser;
Compiler* current = NULL;
initCompiler(&compiler, TYPE_SCRIPT);
Chunk* compilingChunk;

static Chunk* currentChunk() {
	return &current -> function -> chunk;
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

static void emitLoop(int loopStart) {
	emitByte(OP_LOOP);
	
	int offset = currentChunk() -> count - loopStart + 2;
	if (offset > UINT16_MAX) 
		error("Loop body too large");
	emitByte((offset >> 8) & 0xff);
	emitByte(offset & 0xff);
}

static void emitReturn() {
	emitByte(OP_RETURN);
	emitByte(OP_NIL);
}

static int emitJump(uint8_t instruction) {
	emitByte(instruction);
	emitByte(0xff);
	emitByte(0xff);
	return currentChunk() -> count - 2;
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

static void patchJump(int offset) {
	int jump = currentChunk() -> count - offset - 2;
	if (jump > UINT16_MAX) {
		error("Too much code to jump over.");
	}
	currentChunk() -> code[offset] = (jump >> 8) & 0xff;
	currentChunk() -> code[offset + 1] = ump & 0xff;
}

static void initCompiler(Compiler* compiler, FunctionType type) {
	compiler -> enclosing = current;
	compiler -> function = NULL;
	compiler -> type = type;
	compiler -> localCount = 0;
	compiler -> scopeDepth = 0;
	compiler -> function = newFunction();
	current = compiler;

	if (type != TYPE_SCRIPT) {
		current -> function -> name = copyString(parser.previous.start, parser.previous.length);
	}

	Local* local = &current -> locals[current -> localCount++];
	local -> depth = 0;
	local -> name.start = "";
	local -> name.length = 0;
}

static ObjFunction* endCompiler() {
	emitReturn();
	ObjFunction* function = current -> function;
#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError) {
		disassembleChunk(currentChunk(), function -> name != NULL ? function -> name -> chars : "<script>");
	}
#endif
	current = current -> enclosing;
	return function;
}

static void beginScope() {
	current -> scopeDepth++;
}

static void endScope() {
	current -> scopeDepth--;
	while (current -> localCount > 0 && current -> locals[current -> localCount - 1].depth > current -> scopeDepth) {
		emitByte(OP_POP);
		current -> localCount--;
	}
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

static bool identifersEqual(Token* a, Token* b) {
	if (a -> length != b -> length)
		return false;
	return memcp(a -> start, b -> start, a -> length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
	for (int i = compiler -> localCount - 1; i >= 0; i--) {
		Local* local = &compiler -> locals[i];
		if (identifierEqual(name, &local -> name)) {
			if (local -> depth == -1) {
				error("Can't read local variable in its own initializer.");
			}
			return i;
		}
	}
	return -1;
}

static void addLocal(Token name) {
	if (current -> localCount == UINT8_COUNT) {
		error("Too many local variables in function.");
		return;
	}
	Local* local = &current -> locals[current -> localCount++];
	local -> name = name;
	local -> depth = -1;
}

static void declareVariable() {
	if (current -> scopeDepth == 0)
		return 0;
	Token* name = &parser.previous;
	for (int i = current -> localCount - 1; i >= 0; i--) {
		Local* local = &current -> locals[i];
		if (local -> depth != -1 && local -> depth < current -> scopeDepth) {
			break;
		}
		if (identifiersEqual(name, &local -> name)) {
			error("Already variable with this name in this scope.");
		}
	}
	addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage) {
	consume(T_IDENTIFIER, errorMessage);
	declareVariable();
	if (current -> scopeDepth > 0)
		return 0;
	return identifierConstatn(&parser.previous);
}

static void markInitialized() {
	if (current -> scopeDepth == 0)
		return;
	current -> locals[current -> localCount - 1].depth = current -> scopeDepth;
}

static void defineVariable(uint8_t global) {
	if (current -> scopeDepth > 0) {
		markInitialized();
		return;	
	}
	emitBytes(OP_DEFINE_GLOBAL, global);
}

static uint8_t argumentList() {
	uint8_t argCount = 0;
	if (!check(T_RIGHT_PAREN)) {
		do {
			expression();
			if (argCount == 255) {
				error("Can't have more than 255 arguments.");
			}
			argCount++;
		} while (match(T_COMMA));
	}
	consume(T_RIGHT_PAREN, "Expect ')' after arguments.");
	return argCount;
}

static void and_operator(bool canAssign) {
	int endJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	parserPrecedence(P_AND);
	patchJump(endJump);
}

static void or_operator(bool canAssign) {
	int elseJump = emitJump(OP_JUMP_IF_FALSE);
	int endJump = emitJump(OP_JUMP);

	patchJump(elseJump);
	emitByte(OP_POP);

	parsePrecedence(P_OR);
	patchJump(endJump);
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

static void call(bool canAssign) {
	uint8_t argCount = argumentList();
	emitBytes(OP_CALL, argCount);
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
	uint8_t getOp, setOp;
	int arg = resolveLocal(current, &name);
	if (arg != -1) {
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	else {
		arg = identifierConstant(&name);
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	if (canAssign && match(T_EQUAL)) {
		expression();
		emitBytes(setOp, (uint8_t)arg);
	}
	else {
		emitBytes(getOp, (uint8_t)arg);
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
	[T_LEFT_PAREN] = {grouping, call, P_NONE},
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
	[T_AND] = {NULL, and_operator, P_NONE},	
	[T_CLASS] = {NULL, NULL, P_NONE},	
	[T_ELSE] = {NULL, NULL, P_NONE},	
	[T_FALSE] = {literal, NULL, P_NONE},	
	[T_FOR] = {NULL, NULL, P_NONE},	
	[T_FUN] = {NULL, NULL, P_NONE},	
	[T_IF] = {NULL, NULL, P_NONE},	
	[T_NIL] = {literal, NULL, P_NONE},	
	[T_OR] = {NULL, or_operator, P_NONE},	
	[T_PRINT] = {NULL, NULL, P_NONE},	
	[T_RETURN] = {NULL, NULL, P_NONE},	
	[T_SUPER] = {NULL, NULL, P_NONE},	
	[T_THIS] = {NULL, NULL, P_NONE},	
	[T_TRUE] = {NULL, NULL, P_NONE},	
	[T_VAR] = {NULL, NULL, P_NONE},	
	[T_WHILE] = {NULL, NULL, P_NONE},	
	[T_ERROR] = {NULL, NULL, P_NONE},	
	[T_EOF] = {NULL, NULL, P_NONE},	
	[T_SWITCH] = {NULL, NULL, P_NONE};
	[T_CASE] = {NULL, NULL, P_NONE};
	[T_DEFAULT] = {NULL, NULL, P_NONE};
};

static void expression() {
	parsePrecedence(P_ASSIGNMENT);
}

static void block() {
	while (!check(T_RIGHT_BRACE) && !check(T_EOF)) {
		declaration();
	}
	consume(T_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
	Compiler compiler;
	initCompiler(&compiler, type);
	beginScope();

	consume(T_LEFT_PAREN, "Expect '(' after function name.");
	if (!check(T_RIGHT_PAREN)) {
		do {
			current -> function -> arity++;
			if (current -> function -> arity > 255) {
				errorAtCurrent("Can't have more than 255 parameters.");
			}
			uint8_t constant = parseVariable("Expect parameter name.");
			defineVariable(constant);
		} while (match(T_COMMA));
	}		
	consume(T_RIGHT_PAREN, "Expect ')' after paramerters.");
	consume(T_LEFT_BRACE, "Expect '{' before function body.");
	block();

	ObjFunction* function = endCompiler();
	emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));
}

static void funDeclaration() {
	uint8_t global = parseVarialbe("Expect function name");
	markInitialized();
	function(T_FUNCTION);
	defineVariable(global);
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

static void forStatement() {
	beginScope();
	consume(T_LEFT_PAREN, "Expect '(' after 'for'.");
	if (match(T_SEMI_COLON)) {
		
	}
	else if (match(T_VAR)) {
		varDeclaration();
	}	
	else {
		expressionStatement();
	}
	int loopStart = currentChunk() -> count;
	int exitJump = -1;

	if (!match(T_SEMI_COLON)) {
		expression();
		consume(T_SEMI_COLON, "Expect ';' after loop condition.");
		exitJump = emitJump(OP_JUMP_IF_FALSE);
		emitByte(OP_POP);
	}

	if (!match(T_RIGHT_PAREN)) {
		int bodyJump = emitJump(OP_JUMP);
		int incrementStart = currentChunk() -> count;
		expression();
		emitByte(OP_POP);
		consume(T_RIGHT_PAREN, "Expect ')' after for clauses.");

		emitLoop(loopStart);
		loopStart = incrementStart;
		patchJump(bodyJump);
	}
	
	statement();
	
	emitLoop(loopStart);
	
	if (exitJump != -1) {
		patchJump(exitJump);
		emitByte(OP_POP);
	}
	
	endScope();	
}

static void switchStatement() {
	consume(T_LEFT_PAREN, "Expect '(' after 'switch'.");
	expression();
	consume(T_RIGHT_PAREN, "Expect ')' after condition.");

	consume(T_LEFT_BRACE, "Expect '{'.");
	if (!match(T_RIGHT_BRACE)) {
		if (match(T_CASE)) {
			expression();
			consume(T_COLON, "Expect ':' after expression.");
			statement();
		}
		else if (match(T_DEFAULT_CASE)) {
			statement();
		}
		else 
			consume(T_RIGHT_BRACE, "Expect '}.");
	}
}

static void ifStatement() {
	consume(T_LEFT_PAREN, "Expect '(' after 'if'.");
	expression();
	consume(T_RIGHT_PAREN, "Expect ')' after condition.");

	int thenJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();

	int elseJump = emitJump(OP_JUMP);

	patchJump(thenJump);
	emitByte(OP_POP);
	if (match(T_ELSE))
		patchJump(elseJump);
}

static void printStatement() {
	expression();
	consume(T_SEMI_COLON, "Expect ';' after value.");
	emitByte(OP_PRINT);
}

static void returnStatement() {
	if (match(T_SEMI_COLON)) {
		emitReturn();
	}
	else {
		expression();
		consume(T_SEMI_COLON, "Expect ';' after return value.");
		emitByte(OP_RETURN);
	}
}
	
static void whileStatement() {
	int loopStart = currentChunk() -> count;
	consume(T_LEFT_PAREN, "Expect '(' after 'while'.");
	expression();
	consume(T_RIGHT_PAREN, "Expect ')' after condition.");

	int exitJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();
	emitLoop(loopStart);

	patchJump(exitJump);
	emitByte(OP_POP);
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
	if (match(T_FUN)) {
		funDeclaration();
	}
	else if (match(T_VAR)) {
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
	else if (match(T_FOR)) {
		forStatement();
	}
	else if (match(T_IF)) {
		ifStatement();
	}
	else if (match(T_RETURN)) {
		returnStatement();
	}
	else if (match(T_WHILE)) {
		whileStatement();
	}
	else if (match(T_SWITCH)) {
		switchStatement(); //To be completed
	}
	else if (match(T_LEFT_BRACE)) {
		beginScope();
		block();
		endScope();
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
	ObjFunction* function = endCompiler();
	return parser.hadError ? NULL : function;
}

