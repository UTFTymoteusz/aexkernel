#pragma once

char stringbuffer[32];

void write_debug(const char* str, size_t value, int base) {
	printf(str, itoa(value, stringbuffer, base));
}