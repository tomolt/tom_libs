/* tom_ply.h: Zero-Allocation PLY parser
 *
 * Copyright (C) 2026 Thomas Oltmann
 */

#ifndef _TOM_PLY_H_
#endif

#ifdef PLY_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define PLY_MAX_NAME 32

enum ply_format {
	PLY_FORMAT_UNKNOWN = 0,
	PLY_FORMAT_ASCII,
	PLY_FORMAT_BINARY_LITTLE_ENDIAN,
	PLY_FORMAT_BINARY_BIG_ENDIAN,
};

enum ply_type {
	PLY_TYPE_INT8,
	PLY_TYPE_UINT8,
	PLY_TYPE_INT16,
	PLY_TYPE_UINT16,
	PLY_TYPE_INT32,
	PLY_TYPE_UINT32,
	PLY_TYPE_FLOAT32,
	PLY_TYPE_FLOAT64,
};

struct ply_property {
	struct ply_property  *next;
	char                  name[PLY_MAX_NAME];
	enum ply_type         indexType;
	enum ply_type         dataType;
	int                   isList;
};

struct ply_element {
	struct ply_element   *next;
	char                  name[PLY_MAX_NAME];
	unsigned long         numTuples;
	struct ply_property  *properties;
};

struct ply_parser {
	enum ply_format       format;
	struct ply_element   *elements;
	struct ply_element  **elementsTail;
	struct ply_property **propertiesTail;
	char *workArea;
	size_t workSize;
	size_t workBreak;
};

union ply_datum {
	int8_t   i8;
	uint8_t  u8;
	int16_t  i16;
	uint16_t u16;
	int32_t  i32;
	uint32_t u32;
	float    f32;
	double   f64;
};

static const char *ply_type_names[] = {
	"int8",    "char",
	"uint8",   "uchar",
	"int16",   "short",
	"uint16",  "ushort",
	"int32",   "int",
	"uint32",  "uint",
	"float32", "float",
	"float64", "double",
	NULL
};

static unsigned ply_type_sizes[] = {
	1,
	1,
	2,
	2,
	4,
	4,
	4,
	8,
};

void *
ply_reserve(struct ply_parser *ply, size_t size)
{
	if (size > ply->workBreak) return NULL;
	ply->workBreak -= size;
	ply->workBreak &= ~(size_t)0xF;
	void *pointer = ply->workArea + ply->workBreak;
	memset(pointer, 0, size);
	return pointer;
}

char *
ply_next_token(char **strPtr, int delim)
{
	char *start = *strPtr;
	while (*start && *start == delim) {
		start++;
	}

	char *end = start;
	while (*end && *end != delim) {
		end++;
	}

	*strPtr = *end ? end + 1 : end;
	*end = 0;
	return *start ? start : NULL;
}

int
ply_parse_type(const char *str, enum ply_type *type)
{
	for (size_t i = 0; ply_type_names[i]; i++) {
		if (!strcmp(str, ply_type_names[i])) {
			*type = i / 2;
			return 0;
		}
	}
	return -1;
}

int
ply_parse_format(struct ply_parser *ply, char **tokenState)
{
	char *token;
	token = ply_next_token(tokenState, ' ');
	if (!token) return -1;

	if (!strcmp(token, "ascii")) {
		ply->format = PLY_FORMAT_ASCII;
	} else if (!strcmp(token, "binary_little_endian")) {
		ply->format = PLY_FORMAT_BINARY_LITTLE_ENDIAN;
	} else if (!strcmp(token, "binary_big_endian")) {
		ply->format = PLY_FORMAT_BINARY_BIG_ENDIAN;
	} else {
		return -1;
	}

	token = ply_next_token(tokenState, ' ');
	if (!token) return -1;

	if (!!strcmp(token, "1.0")) return -1;

	return 0;
}

int
ply_parse_element(struct ply_parser *ply, char **tokenState)
{
	struct ply_element *element = ply_reserve(ply, sizeof *element);

	char *token;
	token = ply_next_token(tokenState, ' ');
	if (!token) return -1;

	strncpy(element->name, token, PLY_MAX_NAME - 1);

	token = ply_next_token(tokenState, ' ');
	if (!token) return -1;

	char *end;
	element->numTuples = strtoul(token, &end, 10);
	if (*end) return -1;

	element->next = *ply->elementsTail;
	ply->elementsTail = &element->next;
	ply->propertiesTail = &element->properties;

	return 0;
}

int
ply_parse_property(struct ply_parser *ply, char **tokenState)
{
	if (!ply->propertiesTail) return -1;

	struct ply_property *property = ply_reserve(ply, sizeof *property);

	char *token;
	token = ply_next_token(tokenState, ' ');
	if (!token) return -1;

	if (ply_parse_type(token, &property->dataType) < 0) return -1;

	token = ply_next_token(tokenState, ' ');
	if (!token) return -1;

	strncpy(property->name, token, PLY_MAX_NAME - 1);

	property->next = *ply->propertiesTail;
	ply->propertiesTail = &property->next;

	return 0;
}

int
ply_parse_header_line(struct ply_parser *ply, char *line)
{
	char *tokenState = line;
	char *token = ply_next_token(&tokenState, ' ');
	if (!token) return -1;

	if (!strcmp(token, "end_header")) {
		return 0;
	} else if (!strcmp(token, "format")) {
		if (ply_parse_format(ply, &tokenState) < 0) return -1;
	} else if (!strcmp(token, "element")) {
		if (ply_parse_element(ply, &tokenState) < 0) return -1;
	} else if (!strcmp(token, "property")) {
		if (ply_parse_property(ply, &tokenState) < 0) return -1;
	} else {
		if (!!strcmp(token, "comment")) {
			return -1;
		}
	}

	token = ply_next_token(&tokenState, ' ');
	if (!token) return -1;

	return 1;
}

int
ply_load_file(struct ply_parser *ply, const char *filename)
{
	if (!filename || !ply) {
		return -1;
	}

	FILE *file = fopen(filename, "rb");
	if (!file) {
		return -1;
	}

	const size_t maxLine = 1024;
	char line[maxLine];

	if (!fgets(line, maxLine, file) || !!strcmp(line, "ply\n")) {
		fclose(file);
		return -1;
	}

	memset(ply, 0, sizeof *ply);
	ply->elementsTail = &ply->elements;

	for (;;) {
		if (!fgets(line, maxLine, file)) {
			fclose(file);
			return -1;
		}

		int s = ply_parse_header_line(ply, line);
		if (s < 0) {
			fclose(file);
			return -1;
		}
		if (s == 0) break;
	}

	fclose(file);
	return 0;
}

#endif
