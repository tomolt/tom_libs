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

typedef int (*ply_read_callback)(void *readData, void *buffer, unsigned max);

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

typedef const struct ply_property *PLY_PROPERTY;
typedef const struct ply_element  *PLY_ELEMENT;

struct ply_handler {
	bool (*startElement)(void *userdata, PLY_ELEMENT element);
	bool (*endElement)(void *userdata);
	bool (*startTuple)(void *userdata, unsigned long tupleIndex);
	bool (*endTuple)(void *userdata);
	bool (*onDatum)(void *userdata, PLY_PROPERTY property, union ply_datum value);
	bool (*startList)(void *userdata, PLY_PROPERTY property, uint32_t length);
	bool (*endList)(void *userdata);
	bool (*onListItem)(void *userdata, union ply_datum value);
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
	unsigned char         isList;
};

struct ply_element {
	struct ply_element   *next;
	char                  name[PLY_MAX_NAME];
	unsigned long         numTuples;
	//TODO count properties in elements
	//unsigned long         numProperties;
	struct ply_property  *properties;
};

struct ply_parser {
	enum ply_format       format;
	struct ply_element   *elements;
	struct ply_element  **elementsTail;
	struct ply_property **propertiesTail;
	unsigned long         numElements;

	char  *workArea;
	size_t workSize;
	size_t workBreak;

	ply_read_callback readFunc;
	void             *readData;

	const struct ply_handler *handler;

	// TODO fold this into the workArea
	char line[PLY_MAX_LINE];
	unsigned lineLength;

	// For the streaming API
	PLY_ELEMENT   currentElement;
	unsigned long currentTuple;
	PLY_PROPERTY  currentProperty;
	unsigned long currentItem;
	unsigned long offset;
};

static inline const char *
ply_property_get_name(PLY_PROPERTY property) { return property->name; }

static inline enum ply_type
ply_property_is_list(PLY_PROPERTY property) { return property->isList; }

static inline enum ply_type
ply_property_get_index_type(PLY_PROPERTY property) { return property->indexType; }

static inline enum ply_type
ply_property_get_data_type(PLY_PROPERTY property) { return property->dataType; }

static inline const char *
ply_element_get_name(PLY_ELEMENT element) { return element->name; }

static inline unsigned long
ply_element_get_tuple_count(PLY_ELEMENT element) { return element->numTuples; }

static inline unsigned long
ply_parser_get_element_count(struct ply_parser *ply) { return ply->numElements; }

static inline enum ply_format
ply_parser_get_format(struct ply_parser *ply) { return ply->format; }

extern const char   *ply_type_names[];
extern unsigned      ply_type_sizes[];

const char          *ply_strerror(int status);

void                 ply_parser_reset(struct ply_parser *ply);
void                 ply_parser_set_input(struct ply_parser *ply, ply_read_callback readFunc, void *readData);
void                 ply_parser_set_work_area(struct ply_parser *ply, void *workArea, size_t workSize);
void                 ply_parser_set_handler(struct ply_parser *ply, const struct ply_handler *handler);

int                  ply_parse_header(struct ply_parser *ply);

PLY_PROPERTY         ply_element_get_property(PLY_ELEMENT element, unsigned long propertyIndex);
PLY_PROPERTY         ply_element_get_property_by_name(PLY_ELEMENT element, const char *name);
PLY_ELEMENT          ply_parser_get_element(struct ply_parser *ply, unsigned long elementIndex);
PLY_ELEMENT          ply_parser_get_element_by_name(const struct ply_parser *ply, const char *name);

union ply_datum      ply_cast(enum ply_type desiredType, enum ply_type dataType, union ply_datum datum);
union ply_datum      ply_cast_normalized(enum ply_type desiredType, enum ply_type dataType, union ply_datum datum);

int  ply_parser_start_streaming(struct ply_parser *ply);
int  ply_stream_value(struct ply_parser *ply, union ply_datum *datum);
int  ply_stream_list_item(struct ply_parser *ply, union ply_datum *datum);

#endif

#ifdef PLY_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

#include <limits.h>

/* These macros are short-hands for longer case-lists inside switch statements.
 * They are really ugly, but at least they play well with auto-formatting ...
 */
#define PLY_TYPE_INT_      PLY_TYPE_INT8:  case PLY_TYPE_INT16:  case PLY_TYPE_INT32
#define PLY_TYPE_UINT_     PLY_TYPE_UINT8: case PLY_TYPE_UINT16: case PLY_TYPE_UINT32
#define PLY_TYPE_FLOAT_    PLY_TYPE_FLOAT32: case PLY_TYPE_FLOAT64

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

void
ply_parser_reset(struct ply_parser *ply)
{
	ply->format = PLY_FORMAT_UNKNOWN;
	ply->elements = NULL;
	ply->elementsTail = NULL;
	ply->propertiesTail = NULL;
	ply->numElements = 0;
	ply->lineLength = 0;

	ply->workBreak  = ply->workSize;
	ply->workBreak &= ~(size_t)0xF;
}

void
ply_parser_set_input(struct ply_parser *ply, ply_read_callback readFunc, void *readData)
{
	ply->readFunc = readFunc;
	ply->readData = readData;

	ply->lineLength = 0;
}

void
ply_parser_set_work_area(struct ply_parser *ply, void *workArea, size_t workSize)
{
	ply->workArea   = workArea;
	ply->workSize   = workSize;

	ply->workBreak  = ply->workSize;
	ply->workBreak &= ~(size_t)0xF;
}

void
ply_parser_set_handler(struct ply_parser *ply, const struct ply_handler *handler)
{
	ply->handler = handler;
}

static void *
ply_reserve(struct ply_parser *ply, size_t size)
{
	if (ply->workBreak < PLY_MAX_LINE + size) return NULL;
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
	ply->numElements++;

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
	ply->numElements = 0;

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

PLY_PROPERTY
ply_element_get_property(PLY_ELEMENT element, unsigned long propertyIndex)
{
	struct ply_property *property = element->properties;
	for (unsigned long i = 0; i < propertyIndex; i++) {
		if (!property) return NULL;
		property = property->next;
	}
	return property;
}

PLY_PROPERTY
ply_element_get_property_by_name(PLY_ELEMENT element, const char *name)
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

PLY_ELEMENT
ply_parser_get_element(struct ply_parser *ply, unsigned long elementIndex)
{
	const struct ply_element *element = ply->elements;
	for (unsigned long i = 0; i < elementIndex; i++) {
		if (!element) return NULL;
		element = element->next;
	}
	return element;
}

PLY_ELEMENT
ply_parser_get_element_by_name(const struct ply_parser *ply, const char *name)
{
	const struct ply_element *element = ply->elements;
	while (element) {
		if (!strcmp(element->name, name)) return element;
		element = element->next;
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
	if (!end || end == str) return PLY_ERR_SYNTAX;
	while (*end == ' ') end++;
	return end - str;
}

static int
ply_read_datum_le(const unsigned char *raw, enum ply_type type, union ply_datum *datum)
{
	uint64_t q;
	switch (type) {
	case PLY_TYPE_INT8:
		datum->i  = (int32_t)raw[0];
		return 1;

	case PLY_TYPE_UINT8:
		datum->u  = (uint32_t)raw[0];
		return 1;

	case PLY_TYPE_INT16:
		datum->i  = (int32_t)raw[0] << 0;
		datum->i |= (int32_t)raw[1] << 8;
		return 2;

	case PLY_TYPE_UINT16:
		datum->u  = (uint32_t)raw[0] << 0;
		datum->u |= (uint32_t)raw[1] << 8;
		return 2;

	case PLY_TYPE_INT32:
	case PLY_TYPE_UINT32:
	case PLY_TYPE_FLOAT32:
		datum->u  = (uint32_t)raw[0] <<  0;
		datum->u |= (uint32_t)raw[1] <<  8;
		datum->u |= (uint32_t)raw[2] << 16;
		datum->u |= (uint32_t)raw[3] << 24;
		return 4;
	
	case PLY_TYPE_FLOAT64:
		q  = (uint64_t)raw[0] <<  0;
		q |= (uint64_t)raw[1] <<  8;
		q |= (uint64_t)raw[2] << 16;
		q |= (uint64_t)raw[3] << 24;
		q |= (uint64_t)raw[4] << 32;
		q |= (uint64_t)raw[5] << 40;
		q |= (uint64_t)raw[6] << 48;
		q |= (uint64_t)raw[7] << 56;
		datum->d = (double)q;
		return 8;
	
	default:
		return PLY_ERR_INTERNAL;
	}
}

#define PLY_STORE_CAST_VALUE(destType, dest, dataType, datum)\
	switch (dataType) {\
		case PLY_TYPE_INT_:    dest = (destType)datum.i; break;\
		case PLY_TYPE_UINT_:   dest = (destType)datum.u; break;\
		case PLY_TYPE_FLOAT32: dest = (destType)datum.f; break;\
		case PLY_TYPE_FLOAT64: dest = (destType)datum.d; break;\
	}

union ply_datum
ply_cast(enum ply_type desiredType, enum ply_type dataType, union ply_datum datum)
{
	union ply_datum castDatum;
	switch (desiredType) {
	case PLY_TYPE_INT_:    PLY_STORE_CAST_VALUE(int32_t,  castDatum.i, dataType, datum); break;
	case PLY_TYPE_UINT_:   PLY_STORE_CAST_VALUE(uint32_t, castDatum.u, dataType, datum); break;
	case PLY_TYPE_FLOAT32: PLY_STORE_CAST_VALUE(float,    castDatum.f, dataType, datum); break;
	case PLY_TYPE_FLOAT64: PLY_STORE_CAST_VALUE(double,   castDatum.d, dataType, datum); break;
	}
	return castDatum;
}

#define PLY_NORMALIZE(v, min, max) ((v) < 0 ? -((v) / (min)) : (v) / (max))

union ply_datum
ply_cast_normalized(enum ply_type desiredType, enum ply_type dataType, union ply_datum datum)
{
	union ply_datum castDatum;
	switch (desiredType) {
	case PLY_TYPE_INT_:    PLY_STORE_CAST_VALUE(int32_t,  castDatum.i, dataType, datum); break;
	case PLY_TYPE_UINT_:   PLY_STORE_CAST_VALUE(uint32_t, castDatum.u, dataType, datum); break;

	case PLY_TYPE_FLOAT32:
		switch (dataType) {
		case PLY_TYPE_INT8:    castDatum.f = PLY_NORMALIZE((float)datum.i, INT8_MIN,  INT8_MAX);  break;
		case PLY_TYPE_INT16:   castDatum.f = PLY_NORMALIZE((float)datum.i, INT16_MIN, INT16_MAX); break;
		case PLY_TYPE_INT32:   castDatum.f = PLY_NORMALIZE((float)datum.i, INT32_MIN, INT32_MAX); break;

		case PLY_TYPE_UINT8:   castDatum.f = (float)datum.u / UINT8_MAX;  break;
		case PLY_TYPE_UINT16:  castDatum.f = (float)datum.u / UINT16_MAX; break;
		case PLY_TYPE_UINT32:  castDatum.f = (float)datum.u / UINT32_MAX; break;

		case PLY_TYPE_FLOAT32: castDatum.f = (float)datum.f; break;
		case PLY_TYPE_FLOAT64: castDatum.f = (float)datum.d; break;
		}
		break;
	
	case PLY_TYPE_FLOAT64:
		switch (dataType) {
		case PLY_TYPE_INT8:    castDatum.d = PLY_NORMALIZE((double)datum.i, INT8_MIN,  INT8_MAX);  break;
		case PLY_TYPE_INT16:   castDatum.d = PLY_NORMALIZE((double)datum.i, INT16_MIN, INT16_MAX); break;
		case PLY_TYPE_INT32:   castDatum.d = PLY_NORMALIZE((double)datum.i, INT32_MIN, INT32_MAX); break;

		case PLY_TYPE_UINT8:   castDatum.d = (double)datum.u / UINT8_MAX;  break;
		case PLY_TYPE_UINT16:  castDatum.d = (double)datum.u / UINT16_MAX; break;
		case PLY_TYPE_UINT32:  castDatum.d = (double)datum.u / UINT32_MAX; break;

		case PLY_TYPE_FLOAT32: castDatum.d = (double)datum.f; break;
		case PLY_TYPE_FLOAT64: castDatum.d = (double)datum.d; break;
		}
		break;
	}
	return castDatum;
}

// FIXME support #elems = 0 or #props = 0 or #tuples = 0

int
ply_parser_start_streaming(struct ply_parser *ply)
{
	ply->currentElement  = NULL;
	ply->currentTuple    = ULONG_MAX;
	ply->currentProperty = NULL;
	ply->currentItem     = 0;
	ply->offset          = 0;
	return 0;
}

static int
ply_advance(struct ply_parser *ply)
{
	if (!ply->currentProperty) {
		ply->currentElement  = ply->elements;
		ply->currentTuple    = 0;
		ply->currentProperty = ply->currentElement->properties;
		ply->currentItem     = 0;

		int r = ply->readFunc(ply->readData, ply->line + ply->lineLength, PLY_MAX_LINE - ply->lineLength);
		if (r < 0) return PLY_ERR_READ;
		ply->lineLength += (unsigned)r;

		return 0;
	}

	// Advance to the next (element, tuple, property)
	ply->currentProperty = ply->currentProperty->next;
	if (!ply->currentProperty) {
		// TODO make sure there aren't extraneous tokens at the end of the line

		ply->currentTuple++;
		if (ply->currentTuple >= ply->currentElement->numTuples) {
			ply->currentElement = ply->currentElement->next;
			if (!ply->currentElement) {
				ply->currentElement = ply->elements;
			}
			ply->currentTuple = 0;
		}
		ply->currentProperty = ply->currentElement->properties;

		ply->lineLength -= ply->offset + 1;
		memmove(ply->line, ply->line + ply->offset + 1, ply->lineLength);
		ply->offset = 0;

		int r = ply->readFunc(ply->readData, ply->line + ply->lineLength, PLY_MAX_LINE - ply->lineLength);
		if (r < 0) return PLY_ERR_READ;
		ply->lineLength += (unsigned)r;
	}
	return 0;
}

int
ply_stream_value(struct ply_parser *ply, union ply_datum *datum)
{
	ply_advance(ply);

	int r;
	if (ply->currentProperty->isList) {
		union ply_datum indexDatum;
		r = ply_read_datum_ascii(ply->line + ply->offset, ply->currentProperty->dataType, &indexDatum);
		if (r < 0) return r;
		ply->offset += r;

		unsigned listLength;
		switch (ply->currentProperty->indexType) {
		case PLY_TYPE_INT_:
			if (indexDatum.i < 0) return PLY_ERR_SYNTAX;
			listLength = (unsigned)indexDatum.i;
			break;
		case PLY_TYPE_UINT_: listLength = indexDatum.u; break;
		default: return PLY_ERR_SYNTAX;
		}
		datum->u = listLength;
		ply->currentItem = 0;
	} else {
		r = ply_read_datum_ascii(ply->line + ply->offset, ply->currentProperty->dataType, datum);
		if (r < 0) return r;
		ply->offset += r;
	}

	return 0;
}

int
ply_stream_list_item(struct ply_parser *ply, union ply_datum *datum)
{
	int r;
	r = ply_read_datum_ascii(ply->line + ply->offset, ply->currentProperty->dataType, datum);
	if (r < 0) return r;
	ply->offset += r;

	// Advance to the next list item
	ply->currentItem++;
	// TODO don't run over the end of the list

	return 0;
}

#undef PLY_STORE_CAST_VALUE
#undef PLY_NORMALIZE
#undef PLY_TYPE_INT_
#undef PLY_TYPE_UINT_
#undef PLY_TYPE_FLOAT_

#endif
