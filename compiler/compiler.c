#include <stdio.h>
#include "../vm/common.h"
#include "scanner.h"

void compiler(const char* source) {
	initScanner(source);
}
