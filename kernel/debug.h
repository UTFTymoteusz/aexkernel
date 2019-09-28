#pragma once

char debugbuffer[64];

void write_debug(const char* str, size_t value, int base) {
	printf(str, itoa(value, debugbuffer, base));
}