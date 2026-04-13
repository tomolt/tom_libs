/* tom_ply.h: Zero-Allocation PLY parser
 *
 * Copyright (C) 2026 Thomas Oltmann
 */

#ifndef _TOM_PLY_H_
#define _TOM_PLY_H_

#include <stdint.h>

#define PLY_MAX_NAME 32

#define PLY_ERR_SPACE  -100
#define PLY_ERR_LIMIT  -200
#define PLY_ERR_READ   -300
#define PLY_ERR_SYNTAX -400

typedef int (*ply_read_cb)(void *userdata, void *buffer, unsigned max);

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
	ply_read_cb read_cb;
	void *userdata;
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
	char     raw[8];
};

#endif

#ifdef PLY_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

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

const char *
ply_strerror(int status)
{
	switch (status) {
	case PLY_ERR_SPACE:  return "Not Enough Space";
	case PLY_ERR_LIMIT:  return "Internal Limit Exceeded";
	case PLY_ERR_READ:   return "I/O Read Error";
	case PLY_ERR_SYNTAX: return "Syntax Error";
	default:             return "";
	}
}

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
	return PLY_ERR_SYNTAX;
}

int
ply_parse_format(struct ply_parser *ply, char **tokenState)
{
	char *token;
	token = ply_next_token(tokenState, ' ');
	if (!token) return PLY_ERR_SYNTAX;

	if (!strcmp(token, "ascii")) {
		ply->format = PLY_FORMAT_ASCII;
	} else if (!strcmp(token, "binary_little_endian")) {
		ply->format = PLY_FORMAT_BINARY_LITTLE_ENDIAN;
	} else if (!strcmp(token, "binary_big_endian")) {
		ply->format = PLY_FORMAT_BINARY_BIG_ENDIAN;
	} else {
		return PLY_ERR_SYNTAX;
	}

	token = ply_next_token(tokenState, ' ');
	if (!token) return PLY_ERR_SYNTAX;

	if (!!strcmp(token, "1.0")) return PLY_ERR_SYNTAX;

	return 0;
}

int
ply_parse_element(struct ply_parser *ply, char **tokenState)
{
	struct ply_element *element = ply_reserve(ply, sizeof *element);

	char *token;
	token = ply_next_token(tokenState, ' ');
	if (!token) return PLY_ERR_SYNTAX;

	strncpy(element->name, token, PLY_MAX_NAME - 1);

	token = ply_next_token(tokenState, ' ');
	if (!token) return PLY_ERR_SYNTAX;

	char *end;
	element->numTuples = strtoul(token, &end, 10);
	if (*end) return PLY_ERR_SYNTAX;

	element->next = *ply->elementsTail;
	*ply->elementsTail = element;
	ply->elementsTail = &element->next;
	ply->propertiesTail = &element->properties;

	return 0;
}

int
ply_parse_property(struct ply_parser *ply, char **tokenState)
{
	if (!ply->propertiesTail) return PLY_ERR_SYNTAX;

	struct ply_property *property = ply_reserve(ply, sizeof *property);

	char *token;
	token = ply_next_token(tokenState, ' ');
	if (!token) return PLY_ERR_SYNTAX;

	if (!strcmp(token, "list")) {
		property->isList = 1;

		token = ply_next_token(tokenState, ' ');
		if (!token) return PLY_ERR_SYNTAX;

		int s = ply_parse_type(token, &property->indexType);
		if (s < 0) return s;

		token = ply_next_token(tokenState, ' ');
		if (!token) return PLY_ERR_SYNTAX;
	}

	int s = ply_parse_type(token, &property->dataType);
	if (s < 0) return s;

	token = ply_next_token(tokenState, ' ');
	if (!token) return PLY_ERR_SYNTAX;

	strncpy(property->name, token, PLY_MAX_NAME - 1);

	property->next = *ply->propertiesTail;
	*ply->propertiesTail = property;
	ply->propertiesTail = &property->next;

	return 0;
}

int
ply_parse_header_line(struct ply_parser *ply, char *line)
{
	char *tokenState = line;
	char *token = ply_next_token(&tokenState, ' ');
	if (!token) return PLY_ERR_SYNTAX;

	int s;
	if (!strcmp(token, "end_header")) {
		return 0;
	} else if (!strcmp(token, "format")) {
		s = ply_parse_format(ply, &tokenState);
		if (s < 0) return s;
	} else if (!strcmp(token, "element")) {
		s = ply_parse_element(ply, &tokenState);
		if (s < 0) return s;
	} else if (!strcmp(token, "property")) {
		s = ply_parse_property(ply, &tokenState);
		if (s < 0) return s;
	} else if (!strcmp(token, "comment")) {
		return 1;
	} else {
		return PLY_ERR_SYNTAX;
	}

	token = ply_next_token(&tokenState, ' ');
	if (token) return PLY_ERR_SYNTAX;

	return 1;
}

int
ply_parse_header(struct ply_parser *ply)
{
	const unsigned maxLine = 1024;
	char line[maxLine];
	unsigned length;

	int r = ply->read_cb(ply->userdata, line, maxLine);
	if (r < 0) return PLY_ERR_READ;
	length = (unsigned)r;

	if (length < 4 || !!memcmp(line, "ply\n", 4)) {
		return PLY_ERR_SYNTAX;
	}
	length -= 4;
	memmove(line, line + 4, length);

	ply->format = PLY_FORMAT_UNKNOWN;
	ply->elements = NULL;
	ply->elementsTail = &ply->elements;
	ply->propertiesTail = NULL;

	for (;;) {
		int r = ply->read_cb(ply->userdata, line + length, maxLine - length);
		if (r < 0) return PLY_ERR_READ;
		length += (unsigned)r;

		char *nl = strchr(line, '\n');
		if (!nl) return PLY_ERR_SYNTAX;
		*nl = 0;

		int s = ply_parse_header_line(ply, line);
		if (s < 0) return s;
		if (s == 0) break;

		length -= nl + 1 - line;
		memmove(line, nl + 1, length);
	}

	return 0;
}

struct ply_element *
ply_get_element_by_name(const struct ply_parser *ply, const char *name)
{
	struct ply_element *element = ply->elements;
	while (element) {
		if (!strcmp(element->name, name)) {
			return element;
		}
		element = element->next;
	}
	return NULL;
}

struct ply_property *
ply_get_property_by_name(const struct ply_element *element, const char *name)
{
	struct ply_property *property = element->properties;
	while (property) {
		if (!strcmp(property->name, name)) {
			return property;
		}
		property = property->next;
	}
	return NULL;
}

int
ply_read_datum_ascii(const char *str, enum ply_type type, union ply_datum *datum)
{
	char *end;
	long l;
	unsigned long u;
	switch (type) {
	case PLY_TYPE_INT8:
		l = strtol(str, &end, 10);
		if (l < INT8_MIN) return PLY_ERR_SYNTAX;
		if (l > INT8_MAX) return PLY_ERR_SYNTAX;
		datum->i8 = (int8_t)l;
		break;

	case PLY_TYPE_UINT8:
		u = strtoul(str, &end, 10);
		if (u > UINT8_MAX) return PLY_ERR_SYNTAX;
		datum->u8 = (uint8_t)u;
		break;

	case PLY_TYPE_INT16:
		l = strtol(str, &end, 10);
		if (l < INT16_MIN) return PLY_ERR_SYNTAX;
		if (l > INT16_MAX) return PLY_ERR_SYNTAX;
		datum->i16 = (int16_t)l;
		break;

	case PLY_TYPE_UINT16:
		u = strtoul(str, &end, 10);
		if (u > UINT16_MAX) return PLY_ERR_SYNTAX;
		datum->u16 = (uint16_t)u;
		break;

	case PLY_TYPE_INT32:
		l = strtol(str, &end, 10);
		if (l < INT32_MIN) return PLY_ERR_SYNTAX;
		if (l > INT32_MAX) return PLY_ERR_SYNTAX;
		datum->i32 = (int32_t)l;
		break;

	case PLY_TYPE_UINT32:
		u = strtoul(str, &end, 10);
		if (u > UINT32_MAX) return PLY_ERR_SYNTAX;
		datum->u32 = (uint32_t)u;
		break;
	
	case PLY_TYPE_FLOAT32:
		datum->f32 = strtof(str, &end);
		break;

	case PLY_TYPE_FLOAT64:
		datum->f64 = strtod(str, &end);
		break;
	}
	if (*end) return PLY_ERR_SYNTAX;
	return 0;
}

int
ply_read_datum_native(const char *raw, enum ply_type type, union ply_datum *datum)
{
	unsigned size = ply_type_sizes[type];
	// FIXME this won't work on big-endian machines!
	memcpy(datum->raw, raw, size);
	return 0;
}

int
ply_read_datum_reversed(const char *raw, enum ply_type type, union ply_datum *datum)
{
	unsigned size = ply_type_sizes[type];
	// FIXME this won't work on big-endian machines!
	memcpy(datum->raw, raw, size);
	for (unsigned i = 0, j = size - 1; i < j; i++, j--) {
		char tmp      = datum->raw[i];
		datum->raw[i] = datum->raw[j];
		datum->raw[j] = tmp;
	}
	return 0;
}

int
ply_read_datum(enum ply_format format, const char *raw, enum ply_type type, union ply_datum *datum)
{
	switch (format) {
	case PLY_FORMAT_ASCII:
		return ply_read_datum_ascii(raw, type, datum);
	// TODO binary formats
	default:
		return -1;
	}
}

#endif
