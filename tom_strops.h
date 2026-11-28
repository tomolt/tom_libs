/* tom_strops.h: Portable string  manipulation functions
 * version: 0.1
 *
 * Copyright (C) 2026 Thomas Oltmann
 * 
 * Permission to use, copy, modify, and/or distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright notice
 * and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, HETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifndef _TOM_STROPS_H_
#define _TOM_STROPS_H_

#include <stddef.h>

// TODO Windows
#define STROPS_PATH_SEPARATOR '/'

/**
 * 100% compatible with OpenBSD's strlcpy().
 */
size_t strops_lcpy(char *dst, const char *src, size_t size);

/**
 * 100% compatible with OpenBSD's strlcat().
 */
size_t strops_lcat(char *dst, const char *src, size_t size);

/**
 * strcasecmp().
 */
int strops_casecmp(const char *s1, const char *s2);

// TODO strncasecmp() or similar

#endif

#ifdef STROPS_IMPLEMENTATION

#include <string.h>

#define STROPS_MIN(a,b) ((a)<(b)?(a):(b))

size_t strops_lcpy(char *dst, const char *src, size_t size)
{
	size_t i = 0;
	while (i + 1 < size && src[i]) {
		dst[i] = src[i];
		i++;
	}
	if (size) {
		dst[i] = 0;
	}
	while (src[i]) {
		i++;
	}
	return i;
}

size_t strops_lcat(char *dst, const char *src, size_t size)
{
	size_t dlen = size ? strnlen(dst, size - 1) : 0;
	return dlen + strops_strlcpy(dst + dlen, src, size - dlen);
}

int strops_casecmp(const char *s1, const char *s2)
{
	for (;;) {
		if (*s1 != *s2) {
			return (int)*s1 -(int)*s2;
		}
		if (!*s1) return 0;
		s1++;
		s2++;
	}
}

#endif
