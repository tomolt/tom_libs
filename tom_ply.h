/* tom_ply.h: Zero-Allocation PLY parser
 *
 * Copyright (C) 2026 Thomas Oltmann
 */

#ifndef _TOM_PLY_H_
#define _TOM_PLY_H_

#include <stdint.h>
#include <stdbool.h>

#define PLY_MAX_LINE 1024
#define PLY_MAX_NAME 32

#define PLY_ERR_SPACE    -100
#define PLY_ERR_LIMIT    -200
#define PLY_ERR_READ     -300
#define PLY_ERR_SYNTAX   -400
#define PLY_ERR_INTERNAL -500

typedef int (*ply_read_cb)(void *readData, void *buffer, unsigned max);

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

union ply_datum {
	int32_t  i;
	uint32_t u;
	float    f;
	double   d;
};

struct ply_handler {
	bool (*startElement)(void *userdata, const char *name);
	bool (*endElement)(void *userdata);
	bool (*startTuple)(void *userdata);
	bool (*endTuple)(void *userdata);
	bool (*startList)(void *userdata, uint32_t length, enum ply_type itemType);
	bool (*endList)(void *userdata);
	bool (*onListItem)(void *userdata, union ply_datum value);
	bool (*onDatum)(void *userdata, enum ply_type type, union ply_datum value);
	void *userdata;
};

enum ply_format {
	PLY_FORMAT_UNKNOWN = 0,
	PLY_FORMAT_ASCII,
	PLY_FORMAT_BINARY_LITTLE_ENDIAN,
	PLY_FORMAT_BINARY_BIG_ENDIAN,
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
	ply_read_cb readFunc;
	void *readData;
	struct ply_handler handler;
	char line[PLY_MAX_LINE];
	unsigned lineLength;
};

extern const char *ply_type_names[];
extern unsigned ply_type_sizes[];

const char *ply_strerror(int status);
int ply_parse_header(struct ply_parser *ply);
struct ply_element  *ply_get_element_by_name(const struct ply_parser *ply, const char *name);
struct ply_property *ply_get_property_by_name(const struct ply_element *element, const char *name);
int ply_parse_contents(struct ply_parser *ply);

#endif

#ifdef PLY_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

const char *ply_type_names[] = {
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

unsigned ply_type_sizes[] = {
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
	case PLY_ERR_SPACE:    return "Not Enough Space";
	case PLY_ERR_LIMIT:    return "Internal Limit Exceeded";
	case PLY_ERR_READ:     return "I/O Read Error";
	case PLY_ERR_SYNTAX:   return "Syntax Error";
	case PLY_ERR_INTERNAL: return "Parser State Inconsistency";
	default:               return "";
	}
}

static void *
ply_reserve(struct ply_parser *ply, size_t size)
{
	if (size > ply->workBreak) return NULL;
	ply->workBreak -= size;
	ply->workBreak &= ~(size_t)0xF;
	void *pointer = ply->workArea + ply->workBreak;
	memset(pointer, 0, size);
	return pointer;
}

static char *
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

static int
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

static int
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

static int
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

static int
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
		if (property->indexType == PLY_TYPE_FLOAT32 ||
			property->indexType == PLY_TYPE_FLOAT64) {
			return PLY_ERR_SYNTAX;
		}

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

static int
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
	int r = ply->readFunc(ply->readData, ply->line, PLY_MAX_LINE);
	if (r < 0) return PLY_ERR_READ;
	ply->lineLength = (unsigned)r;

	if (ply->lineLength < 4 || !!memcmp(ply->line, "ply\n", 4)) {
		return PLY_ERR_SYNTAX;
	}
	ply->lineLength -= 4;
	memmove(ply->line, ply->line + 4, ply->lineLength);

	ply->format = PLY_FORMAT_UNKNOWN;
	ply->elements = NULL;
	ply->elementsTail = &ply->elements;
	ply->propertiesTail = NULL;

	for (;;) {
		int r = ply->readFunc(ply->readData,
			ply->line + ply->lineLength, PLY_MAX_LINE - ply->lineLength);
		if (r < 0) return PLY_ERR_READ;
		ply->lineLength += (unsigned)r;

		char *nl = strchr(ply->line, '\n');
		if (!nl) return PLY_ERR_SYNTAX;
		*nl = 0;

		int s = ply_parse_header_line(ply, ply->line);
		if (s < 0) return s;

		ply->lineLength -= nl + 1 - ply->line;
		memmove(ply->line, nl + 1, ply->lineLength);

		if (s == 0) break;
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

static int
ply_read_datum_ascii(const char *str, enum ply_type type, union ply_datum *datum)
{
	char *end;
	switch (type) {
	case PLY_TYPE_INT8:
		datum->i = strtol(str, &end, 10);
		if (datum->i < INT8_MIN) return PLY_ERR_SYNTAX;
		if (datum->i > INT8_MAX) return PLY_ERR_SYNTAX;
		break;

	case PLY_TYPE_UINT8:
		datum->u = strtoul(str, &end, 10);
		if (datum->u > UINT8_MAX) return PLY_ERR_SYNTAX;
		break;

	case PLY_TYPE_INT16:
		datum->i = strtol(str, &end, 10);
		if (datum->i < INT16_MIN) return PLY_ERR_SYNTAX;
		if (datum->i > INT16_MAX) return PLY_ERR_SYNTAX;
		break;

	case PLY_TYPE_UINT16:
		datum->u = strtoul(str, &end, 10);
		if (datum->u > UINT16_MAX) return PLY_ERR_SYNTAX;
		break;

	case PLY_TYPE_INT32:
		datum->i = strtol(str, &end, 10);
		if (datum->i < INT32_MIN) return PLY_ERR_SYNTAX;
		if (datum->i > INT32_MAX) return PLY_ERR_SYNTAX;
		break;

	case PLY_TYPE_UINT32:
		datum->u = strtoul(str, &end, 10);
		if (datum->u > UINT32_MAX) return PLY_ERR_SYNTAX;
		break;
	
	case PLY_TYPE_FLOAT32:
		datum->f = strtof(str, &end);
		break;

	case PLY_TYPE_FLOAT64:
		datum->d = strtod(str, &end);
		break;
	
	default:
		return PLY_ERR_INTERNAL;
	}
	if (*end) return PLY_ERR_SYNTAX;
	return 0;
}

static int
ply_read_datum_le(const char *raw, enum ply_type type, union ply_datum *datum)
{
	uint64_t q;
	switch (type) {
	case PLY_TYPE_INT8:
		datum->i  = raw[0];
		break;

	case PLY_TYPE_INT16:
		datum->i  = (int32_t)raw[0] << 0;
		datum->i |= (int32_t)raw[1] << 8;
		break;

	case PLY_TYPE_INT32:
		datum->i  = (int32_t)raw[0] <<  0;
		datum->i |= (int32_t)raw[1] <<  8;
		datum->i |= (int32_t)raw[2] << 16;
		datum->i |= (int32_t)raw[3] << 24;
		break;

	case PLY_TYPE_UINT8:
		datum->u  = (uint32_t)raw[0];
		break;

	case PLY_TYPE_UINT16:
		datum->u  = (uint32_t)raw[0] << 0;
		datum->u |= (uint32_t)raw[1] << 8;
		break;

	case PLY_TYPE_UINT32:
	case PLY_TYPE_FLOAT32:
		datum->u  = (uint32_t)raw[0] <<  0;
		datum->u |= (uint32_t)raw[1] <<  8;
		datum->u |= (uint32_t)raw[2] << 16;
		datum->u |= (uint32_t)raw[3] << 24;
		break;
	
	case PLY_TYPE_FLOAT64:
		q  = (uint32_t)raw[0] <<  0;
		q |= (uint32_t)raw[1] <<  8;
		q |= (uint32_t)raw[2] << 16;
		q |= (uint32_t)raw[3] << 24;
		datum->d = (double)q;
		break;
	
	default:
		return PLY_ERR_INTERNAL;
	}
	return 0;
}

#if 0
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
#endif

static int
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

int
ply_parse_contents_ascii(struct ply_parser *ply)
{
	const struct ply_element *element = ply->elements;
	while (element) {
		if (ply->handler.startElement) {
			ply->handler.startElement(ply->handler.userdata, element->name);
		}

		for (unsigned long t = 0; t < element->numTuples; t++) {
			int r = ply->readFunc(ply->readData, ply->line + ply->lineLength, PLY_MAX_LINE - ply->lineLength);
			if (r < 0) return PLY_ERR_READ;
			ply->lineLength += (unsigned)r;

			char *nl = strchr(ply->line, '\n');
			if (!nl) return PLY_ERR_SYNTAX;
			*nl = 0;

			char *tokenState = ply->line;
			char *token;

			if (ply->handler.startTuple) {
				ply->handler.startTuple(ply->handler.userdata);
			}

			const struct ply_property *property = element->properties;
			while (property) {
				token = ply_next_token(&tokenState, ' ');
				if (!token) return PLY_ERR_SYNTAX;

				if (property->isList) {
					union ply_datum indexDatum;
					r = ply_read_datum_ascii(token, property->indexType, &indexDatum);
					if (r < 0) return r;

					unsigned length;
					switch (property->indexType) {
					case PLY_TYPE_INT8:
					case PLY_TYPE_INT16:
					case PLY_TYPE_INT32:
						if (indexDatum.i < 0) {
							return PLY_ERR_SYNTAX;
						}
						length = (unsigned)indexDatum.i;
						break;

					case PLY_TYPE_UINT8:
					case PLY_TYPE_UINT16:
					case PLY_TYPE_UINT32:
						length = indexDatum.u;
						break;

					default:
						return PLY_ERR_SYNTAX;
					}

					if (ply->handler.startList) {
						ply->handler.startList(ply->handler.userdata,
							length, property->dataType);
					}

					for (unsigned i = 0; i < length; i++) {
						token = ply_next_token(&tokenState, ' ');
						if (!token) return PLY_ERR_SYNTAX;

						union ply_datum datum;
						r = ply_read_datum_ascii(token, property->dataType, &datum);
						if (r < 0) return r;

						if (ply->handler.onListItem) {
							ply->handler.onListItem(ply->handler.userdata, datum);
						}
					}

					if (ply->handler.endList) {
						ply->handler.endList(ply->handler.userdata);
					}
				} else {
					union ply_datum datum;
					r = ply_read_datum_ascii(token, property->dataType, &datum);
					if (r < 0) return r;

					if (ply->handler.onDatum) {
						ply->handler.onDatum(ply->handler.userdata, property->dataType, datum);
					}
				}

				property = property->next;
			}

			token = ply_next_token(&tokenState, ' ');
			if (token) return PLY_ERR_SYNTAX;

			if (ply->handler.endTuple) {
				ply->handler.endTuple(ply->handler.userdata);
			}

			ply->lineLength -= nl + 1 - ply->line;
			memmove(ply->line, nl + 1, ply->lineLength);
		}

		if (ply->handler.endElement) {
			ply->handler.endElement(ply->handler.userdata);
		}

		element = element->next;
	}

	return 0;
}

int
ply_parse_contents(struct ply_parser *ply)
{
	switch (ply->format) {
	case PLY_FORMAT_ASCII:
		return ply_parse_contents_ascii(ply);
	
	default:
		return PLY_ERR_INTERNAL;
	}
}

#if 0
union ply_datum
ply_cast(enum ply_type desiredType, enum ply_type dataType, union ply_datum datum)
{
	union ply_datum castDatum;
	switch (desiredType) {
	case PLY_TYPE_INT32:
		switch (dataType) {
		case PLY_TYPE_FLOAT:
			castDatum.i = (int32_t)datum.f;
			break;
		}
		break;
	}
}
#endif

/*

Concept for stream-based API:

ply_next_element();

ply_next_tuple();



 */

#endif
