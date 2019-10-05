#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool sprint(char* dst, const char* data, size_t length) {
	for (size_t i = 0; i < length; i++)
		dst[i] = data[i];

	return true;
}

int sprintf(char* dst, const char* restrict format, ...) {
	char itoa_buffer[128];

	va_list parameters;
	va_start(parameters, format);

	int written = 0;

	while (*format != '\0') {
		size_t maxrem = INT_MAX - written;

		if (format[0] != '%' || format[1] == '%') {
			if (format[0] == '%')
				format++;

			size_t amount = 1;
			while (format[amount] && format[amount] != '%')
				amount++;

			if (maxrem < amount) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!sprint(dst, format, amount))
				return -1;

			format += amount;
			written += amount;

			continue;
		}

		const char* format_begun_at = format++;

		if (*format == 'c') {
			format++;
			char c = (char) va_arg(parameters, int /* char promotes to int */);

			if (!maxrem) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!sprint(dst, &c, sizeof(c)))
				return -1;

			written++;
			dst++;
		}
		else if (*format == 'x') {
			format++;
			uint64_t val = (uint64_t) va_arg(parameters, uint64_t);

			itoa(val, itoa_buffer, 16);
			size_t len = strlen(itoa_buffer);

			if (!maxrem) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!sprint(dst, itoa_buffer, len))
				return -1;

			written += len;
			dst += len;
		}
		else if (*format == 'i') {
			format++;
			uint64_t val = (uint64_t) va_arg(parameters, uint64_t);

			itoa(val, itoa_buffer, 10);
			size_t len = strlen(itoa_buffer);

			if (!maxrem) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!sprint(dst, itoa_buffer, len))
				return -1;

			written += len;
			dst += len;
		}
        else if (*format == 's') {
			format++;
			const char* str = va_arg(parameters, const char*);
			size_t len = strlen(str);

			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!sprint(dst, str, len))
				return -1;

			written += len;
			dst += len;
		}
        else {
			format = format_begun_at;
			size_t len = strlen(format);

			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!sprint(dst, format, len))
				return -1;

			written += len;
			format += len;
			dst += len;
		}
	}
	dst[0] = '\0';

	va_end(parameters);
	return written;
}