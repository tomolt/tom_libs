/* tom_pathutil.h: Zero-dependency portable file-path manipulation functions
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

#ifndef _TOM_PATHUTIL_H_
#define _TOM_PATHUTIL_H_

#include <stddef.h>

// TODO Windows
#define PATHUTIL_SEPARATOR '/'

/**
 * Returns 1 if the path starts with a Windows drive letter, 0 otherwise.
 * On non-Windows systems, this function always returns 0.
 */
int pathutil_hasdrive(const char *path);

/**
 * Normalize a file path in-place.
 * The buffer that the path is stored in needs to be at least two bytes large.
 */
void pathutil_normpath(char *path);

#endif

#ifdef PATHUTIL_IMPLEMENTATION

//#define PATHUTIL_MIN(a,b) ((a)<(b)?(a):(b))

static const char *pathutil_nextcomponent(const char *path)
{
	const char *p = path;
	while (*p && *p != PATHUTIL_SEPARATOR) p++;
	while (*p == PATHUTIL_SEPARATOR) p++;
	return p;
}

static void pathutil_normcomponents(char *components, int absolute)
{
	char *r = components;
	char *w = components;

	// right after the root slash, we may encounter some duplicate slashes
	while (*r == PATHUTIL_SEPARATOR) {
		r++;
	}

	unsigned depth = 0;
	while (*r) {
		char *n = (char *)pathutil_nextcomponent(r);

		if (r[0] == '.' && r[1] == '.' && (!r[2] || r[2] == PATHUTIL_SEPARATOR)) {
			if (depth > 0) {
				w--;
				while (w > components && *(w-1) != PATHUTIL_SEPARATOR) {
					w--;
				}
				depth--;
			} else {
				if (!absolute) {
					while (*r && *r != PATHUTIL_SEPARATOR) {
						*w++ = *r++;
					}
					if (*r) { // don't lose the nul
						*w++ = PATHUTIL_SEPARATOR;
					}
				}
			}
		} else if (r[0] == '.' && (!r[1] || r[1] == PATHUTIL_SEPARATOR)) {
			// do nothing
		} else {
			while (*r && *r != PATHUTIL_SEPARATOR) {
				*w++ = *r++;
			}
			if (*r) { // don't lose the nul
				*w++ = PATHUTIL_SEPARATOR;
			}
			depth++;
		}
			
		r = n;
	}

	// single dot elision may have left over some slashes at the end
	while (w > components && *(w-1) == PATHUTIL_SEPARATOR) {
		w--;
	}
	
	*w = 0;
}

static int pathutil_isletter(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int pathutil_hasdrive(const char *path)
{
	// TODO always false on Unix
	return pathutil_isletter(path[0]) && path[1] == ':';
}

void pathutil_normpath(char *path)
{
	// TODO lowercase on Windows
	// TODO convert all slashes to backward slashes on Windows

	char *components = path;

	if (pathutil_hasdrive(components)) {
		components += 2;
	}
	
	int absolute = 0;
	if (components[0] == PATHUTIL_SEPARATOR) {
		absolute = 1;
		components++;
	}

	pathutil_normcomponents(components, absolute);

	if (!path[0]) {
		path[0] = '.';
		path[1] = 0;
	}
}

#endif
