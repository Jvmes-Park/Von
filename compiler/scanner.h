#ifndef Von_scanner_h
#define Von_scanner_h

typedef enum {
	T_LEFT_PAREN, T_RIGHT_PAREN, T_LEFT_BRACE,
       	T_RIGHT_BRACE, T_COMMA, T_DOT, T_MINUS, T_PLUS, 
	T_SEMI_COLON, T_SLASH, T_STAR,
	
	T_BANG, T_BANG_EQUAL, T_EQUAL, T_EQUAL_EQUAL, T_GREATER,
	T_GREATER_EQUAL, T_LESS, T_LESS_EQUAL,

	T_IDENTIFIER, T_STRING, T_NUMBER,

	T_HELP, T_EXIT,

	T_AND, T_CLASS, T_ELSE, T_FALSE, T_FOR, T_FUN, T_IF, T_NIL, 
	T_OR, T_PRINT, T_RETURN, T_SUPER, T_THIS, T_TRUE, T_VAR, 
	T_WHILE, T_IMPORT,

	T_ERROR, T_EOF
} TokenType;

typedef struct {
	TokenType type;
	const char* start;
	int length;
	int line;
} Token;

void initScanner(const char* source);
Token scanToken();

#endif
