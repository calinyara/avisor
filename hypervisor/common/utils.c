// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (mr.dengjie@gmail.com).
 */

#include <stddef.h>
#include <stdint.h>

#include "common/types.h"
#include "common/utils.h"

int abs(int n)
{
	return n < 0 ? -n : n;
}

char *strncpy(char *dest, const char *src, size_t n)
{
	size_t i;

	for (i = 0; i < n && src[i] != '\0'; i++) {
		dest[i] = src[i];
	}

	for (; i < n; i++)
		dest[i] = '\0';

	return dest;
}

size_t strlen(const char *s)
{
	size_t i;

	for (i = 0; *s != '\0'; i++, s++)
		;

	return i;
}

size_t strnlen(const char *s, size_t n)
{
	size_t i;

	for (i = 0; i < n && *s != '\0'; i++, s++)
		;

	return i;
}

int strcmp(const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}

	return *s1 - *s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	size_t i;

	for (i = 0; i < n && *s1 && (*s1 == *s2); i++, s1++, s2++)
		;

	return (i != n) ? (*s1 - *s2) : 0;
}

void *memset(void *s, int c, size_t n)
{
	for (size_t i = 0; i < n; i++)
		*(uint8_t *)s++ = (uint8_t)c;

	return s;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
	size_t i;
	uint8_t *p1 = (uint8_t *)s1;
	uint8_t *p2 = (uint8_t *)s2;

	for (i = 0; i < n; i++, p1++, p2++) {
		if (*p1 != *p2)
			break;
	}

	if (i == n)
		return 0;

	else
		return *p1 - *p2;
}

void *memmove(void *dest, const void *src, size_t n)
{
	if (src + n > dest) {
		src += n - 1;
		dest += n - 1;

		for (size_t i = 0; i < n; i++)
			*(uint8_t *)dest-- = *(uint8_t *)src--;
	} else {
		memcpy(dest, src, n);
	}

	return dest;
}

void *memchr(const void *s, int c, size_t n)
{
	uint8_t *p = (uint8_t *)s;

	for (size_t i = 0; i < n; i++, p++) {
		if (*p == c)
			return p;
	}

	return NULL;
}

char *strchr(const char *s, int c)
{
	char *p = (char *)s;

	while (*p != '\0' && *p != c)
		p++;

	if (*p == '\0')
		return NULL;

	else
		return p;
}

char *strcpy(char *dest, const char *src)
{
	do {
		*dest++ = *src;
	} while (*src++ != '\0');

	return dest;
}

char *strncat(char *dest, const char *src, size_t n)
{
	size_t destlen = strlen(dest);
	size_t i;

	for (i = 0; i < n; i++) {
		dest[destlen + i] = src[i];
	}

	dest[destlen + i] = '\0';
	return dest;
}

char *strcat(char *dest, const char *src)
{
	size_t destlen = strlen(dest);

	strcpy(dest + destlen, src);
	return dest;
}

int isdigit(int c)
{
	return (c >= '0' && c <= '9');
}

int isspace(int c)
{
	return (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' ||
		c == '\v');
}

int toupper(int c)
{
	if (c >= 'a' && c <= 'z')
		return c - ('a' - 'A');

	else
		return c;
}

int tolower(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c + ('a' - 'A');

	else
		return c;
}

int strcasecmp(const char *s1, const char *s2)
{
	int c1, c2;

	do {
		c1 = tolower(*s1++);
		c2 = tolower(*s2++);
	} while (c1 == c2 && c1 != 0);

	return c1 - c2;
}

/**
 * strncasecmp - Case insensitive, length-limited string comparison
 */
int strncasecmp(const char *s1, const char *s2, size_t len)
{
	unsigned char c1, c2;

	if (!len)
		return 0;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (!c1 || !c2)
			break;

		if (c1 == c2)
			continue;

		c1 = tolower(c1);
		c2 = tolower(c2);

		if (c1 != c2)
			break;
	} while (--len);

	return (int)c1 - (int)c2;
}

/*
 * Convert a string to a long integer - decimal support only.
 */
int64_t strtol_deci(const char *nptr)
{
	const char *s = nptr;
	uint64_t acc, cutoff, cutlim;
	int32_t neg = 0, any;
	uint64_t base = 10UL;
	char c;

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 */
	do {
		c = *s;
		s++;
	} while (isspace(c));

	if (c == '-') {
		neg = 1;
		c = *s;
		s++;
	} else if (c == '+') {
		c = *s;
		s++;
	} else {
		/* No sign character. */
	}

	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] and the input base is 10,
	 * cutoff will be set to 214748364 and cutlim to either
	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
	 * the number is too big, and we will return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff = (neg != 0) ? LONG_MIN : LONG_MAX;
	cutlim = cutoff % base;
	cutoff /= base;
	acc = 0UL;
	any = 0;

	while ((c >= '0') && (c <= '9')) {
		c -= '0';
		if ((acc > cutoff) ||
		    ((acc == cutoff) && ((uint64_t)c > cutlim))) {
			any = -1;
			break;
		} else {
			acc *= base;
			acc += (uint64_t)c;
		}

		c = *s;
		s++;
	}

	if (any < 0) {
		acc = (neg != 0) ? LONG_MIN : LONG_MAX;
	} else if (neg != 0) {
		acc = ~acc + 1UL;
	} else {
		/* There is no overflow and no leading '-' exists. In such case
		 * acc already holds the right number. No action required. */
	}
	return (long)acc;
}
