#ifndef Von_value_h
#define Von_value_h

#include "common.h"

typedef enum {
	VAL_BOOL,
	VAL_NIL,
	VAL_NUMBER,
} ValueType;

typedef struct {
	ValueType type;
	union {
		bool boolean;
		double number;
	} as;
} Value;

typedef double Value;

typedef struct {
	int capacity;
	int count;
	Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value values);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif
