/* tom_ply.h: Zero-Allocation PLY parser
 * version: 1.0
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
 *
 * WHY USE THE PLY FORMAT?
 *
 * PLY is a ubiquitous and well-established format for 3D geometry.
 * It is more versatile than OBJ, and the way it implements indexing
 * maps more directly to the way graphics hardware works.
 * PLY is entirely descriptive unlike glTF,
 * which dictates the way you need to lay out your buffers and render pipelines.
 *
 * HOW FAST IS THIS IMPLEMENTATION?
 *
 * Hard to say. PLY as a format does not lend itself well to fast load times.
 * If your application requires fast disk loads, I recommend you design your
 * own in-house mesh representation. You should prefer fixed, well-defined layouts,
 * and a structure-of-arrays approach.
 * The PLY file format allows too much variation to be processed in bulk effectively.
 *
 * HOW MUCH MEMORY DOES THIS IMPLEMENTATION USE?
 * 
 * Simple, as much as you give it, and none more.
 * Though practically, you should give it at least a buffer of two kilobytes or so.
 * A nice consequence of this design is that you do not need to call
 * a function to release PLY resources when you are done;
 * You can simply repurpose the memory that you gave it.
 *
 */

#ifndef _TOM_PLY_H_
#define _TOM_PLY_H_

#include <stdint.h>

#define PLY_MIN_BUFFER_CAPACITY 1024
#define PLY_MAX_NAME 32

/* Unless otherwise noted, all functions that return an int either return a non-negative value on success,
 * or a negative value to signal an error. The following are the error codes returned by the PLY parser itself.
 * User-supplied callbacks may return other error codes.
 */
#define PLY_ERR_SPACE    -110
#define PLY_ERR_LIMIT    -120
#define PLY_ERR_READ     -130
#define PLY_ERR_SYNTAX   -140
#define PLY_ERR_INTERNAL -150

/* A callback function to read a chunk of data from the input.
 * readData is user-specified pointer that can be used to store some context (e.g. a FILE pointer).
 * The data is to be stored in the provided buffer.
 * Up to max bytes may be read.
 * The function either returns the number of bytes read, or a negative value indicating an error
 * (any negative value can be chosen).
 */
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

/* A scalar value, i.e. the value of a property that isn't a list.
 */
union ply_scalar {
	int32_t  i; // Any signed int type: int8, int16, int32
	uint32_t u; // Any unsigned int type: uint8, uint16, uint32
	float    f;
	double   d;
};

enum ply_format {
	PLY_FORMAT_UNKNOWN = 0,
	PLY_FORMAT_ASCII,
	PLY_FORMAT_BINARY_LITTLE_ENDIAN,
	PLY_FORMAT_BINARY_BIG_ENDIAN,
};

typedef const struct ply_property *PLY_PROPERTY;
typedef const struct ply_element  *PLY_ELEMENT;

// Internal struct. Use the opaque-ish type PLY_PROPERTY and
// the provided accessor functions instead of using this struct directly.
struct ply_property {
	struct ply_property  *next;
	char                  name[PLY_MAX_NAME];
	enum ply_type         lengthType;
	enum ply_type         scalarType;
	unsigned char         isList;
};

// Internal struct. Use the opaque-ish type PLY_PROPERTY and
// the provided accessor functions instead of using this struct directly.
struct ply_element {
	struct ply_element   *next;
	char                  name[PLY_MAX_NAME];
	unsigned long         numTuples;
	struct ply_property  *properties;
	struct ply_property  *lastProperty;
	unsigned long         numProperties;
};

struct ply_parser {
	// Information extracted from the header
	enum ply_format       format;
	struct ply_element   *elements;
	struct ply_element   *lastElement;
	unsigned long         numElements;

	// Some work memory provided by the application.
	// The lower half (below memoryBreak) is used as a read buffer,
	// while the upper half is used as a heap to store element and
	// property descriptions.
	char  *memory;
	size_t memorySize;
	size_t memoryBreak;
	size_t bufferFill;

	// Input reading callback provided by the application.
	ply_read_callback readFunc;
	void             *readData;

	// State of the streaming API
	PLY_ELEMENT   currentElement;
	unsigned long currentTuple;
	PLY_PROPERTY  currentProperty;
	unsigned long currentItem;
	unsigned long bufferOffset;
};

/* Turn a PLY return code into an error string (thread-safe).
 * Obviously, this function does not handle user-defined error codes
 * that may be returned by user-supplied callback functions.
 */
const char *ply_strerror(int status);

/* The following is a series of fairly self-explanatory helper- and accessor-functions.
 * You should prefer using these instead of peeking into the provided structs.
 */

PLY_PROPERTY ply_element_get_property(PLY_ELEMENT element, unsigned long propertyIndex);
PLY_PROPERTY ply_element_get_property_by_name(PLY_ELEMENT element, const char *name);
PLY_ELEMENT  ply_parser_get_element(struct ply_parser *ply, unsigned long elementIndex);
PLY_ELEMENT  ply_parser_get_element_by_name(const struct ply_parser *ply, const char *name);
static inline const char *
ply_property_get_name(PLY_PROPERTY property) { return property->name; }
static inline enum ply_type
ply_property_is_list(PLY_PROPERTY property) { return property->isList; }
static inline enum ply_type
ply_property_get_length_type(PLY_PROPERTY property) { return property->lengthType; }
static inline enum ply_type
ply_property_get_scalar_type(PLY_PROPERTY property) { return property->scalarType; }
static inline const char *
ply_element_get_name(PLY_ELEMENT element) { return element->name; }
static inline unsigned long
ply_element_get_tuple_count(PLY_ELEMENT element) { return element->numTuples; }
static inline unsigned long
ply_element_get_property_count(PLY_ELEMENT element) { return element->numProperties; }
static inline unsigned long
ply_parser_get_element_count(struct ply_parser *ply) { return ply->numElements; }
static inline enum ply_format
ply_parser_get_format(struct ply_parser *ply) { return ply->format; }

/* Set the read callback that the PLY parser will use to read more input.
 * This function must be called before the parser can be used to do anything useful.
 */
void ply_parser_set_input(struct ply_parser *ply, ply_read_callback readFunc, void *readData);

/* Set the memory area that the PLY parser will use for input buffering and to store
 * some information about the file structure.
 * This function must be called before the parser can be used to do anything useful.
 */
void ply_parser_set_memory(struct ply_parser *ply, void *memory, size_t memorySize);

/* Reset the internal structures of the PLY parser.
 * Then, process the header information of the PLY file.
 * Must be called after a read callback and a memory pointer have been supplied.
 * Can be called on a struct ply_parser that has already previously been used
 * (but the input has to be reset by the user).
 * After this function has successfully completed, you can inspect the file structure
 * of elements and properties using the accessor functions.
 */
int  ply_process_header(struct ply_parser *ply);

/* Preferred, stream-based parsing functionality.
 * 
 * ply_start_streaming() prepares the PLY parser for streaming.
 * This function may only be called after ply_process_header().
 * After this function has been called, it (or ply_process_with_callbacks()) should not be called
 * on this struct ply_parser anymore, until ply_process_header() has been called again.
 */
int  ply_start_streaming(struct ply_parser *ply);

/* Advance to the next value in the file, and store it in *value.
 * If the property being read is a list, then the length of the list is stored in *value.
 * Once you have read the length of the list,
 * you need to call ply_stream_list_item() once for each item in the list,
 * before reading the next value via ply_stream_value().
 */
int  ply_stream_value(struct ply_parser *ply, union ply_scalar *value);

/* Advance to the next list item and read it.
 * This function should only be called when the stream is inside of a list.
 * It should only be called once for each item in the list.
 */
int  ply_stream_list_item(struct ply_parser *ply, union ply_scalar *value);

/* A set of user-specified callbacks that can be used as a SAX-like parser interface.
 * Each callback can return a negative value to indicate an error to stop parsing early.
 * It is OK for any these callbacks to be set to NULL if they are not needed.
 */
struct ply_handler {
	int (*startElement)(void *userdata, PLY_ELEMENT element);
	int (*endElement)(void *userdata);
	int (*startTuple)(void *userdata, unsigned long tupleIndex);
	int (*endTuple)(void *userdata);
	int (*onScalarValue)(void *userdata, PLY_PROPERTY property, union ply_scalar value);
	int (*startList)(void *userdata, PLY_PROPERTY property, uint32_t length);
	int (*endList)(void *userdata);
	int (*onListItem)(void *userdata, union ply_scalar value);
};

/* Alternative, SAX-style parsing function.
 *
 * This function may only be called after ply_process_header().
 * After this function has been called, it (or ply_start_streaming()) should not be called
 * on this struct ply_parser anymore, until ply_process_header() has been called again.
 */
int  ply_process_with_callbacks(struct ply_parser *ply, const struct ply_handler *handler, void *userdata);

/* Sometimes, PLY files provide the properties that you need in different types than the ones you expect.
 * For these situations, you can use ply_cast() to convert a value from its current type to any other.
 * This function does not check for any under- or overflows that may occur.
 * When floating-point values are cast to integer values, they are rounded towards zero.
 */
union ply_scalar ply_cast(enum ply_type desiredType, enum ply_type scalarType, union ply_scalar value);

/* This function is identical to ply_cast(), except that when casting to a floating-point type,
 * signed integer types are normalized to a range between -1.0 and 1.0,
 * and unsigned integer types are normalized to a range between 0.0 and 1.0.
 */
union ply_scalar ply_cast_normalized(enum ply_type desiredType, enum ply_type scalarType, union ply_scalar value);

#endif

#ifdef PLY_IMPLEMENTATION

#include <stdlib.h> // strtof(), strtod(), strtol(), strtoul()
#include <string.h> // memcmp(), strcmp(), memmove(), memset(), strncpy()

/* These macros are short-hands for case-lists inside switch statements.
 * They are really ugly, but at least they play well with auto-formatting ...
 */
#define PLY_TYPE_INT_      PLY_TYPE_INT8:  case PLY_TYPE_INT16:  case PLY_TYPE_INT32
#define PLY_TYPE_UINT_     PLY_TYPE_UINT8: case PLY_TYPE_UINT16: case PLY_TYPE_UINT32
#define PLY_TYPE_FLOAT_    PLY_TYPE_FLOAT32: case PLY_TYPE_FLOAT64

// Must be kept in the same order as the ply_type enum.
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
ply_parser_set_input(struct ply_parser *ply, ply_read_callback readFunc, void *readData)
{
	ply->readFunc = readFunc;
	ply->readData = readData;

	ply->bufferFill = 0;
}

void
ply_parser_set_memory(struct ply_parser *ply, void *memory, size_t memorySize)
{
	ply->memory       = memory;
	ply->memorySize   = memorySize;

	ply->memoryBreak  = ply->memorySize;
	ply->memoryBreak &= ~(size_t)0xF;
}

/* Reserve some space from the top of the memory pool.
 * The returned pointers are 16-byte aligned.
 * The space is cleared to zero.
 */
static void *
ply_reserve(struct ply_parser *ply, size_t size)
{
	if (ply->memoryBreak < PLY_MIN_BUFFER_CAPACITY + size) return NULL;
	ply->memoryBreak -= size;
	ply->memoryBreak &= ~(size_t)0xF;
	void *pointer = ply->memory + ply->memoryBreak;
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

	if (ply->elements) {
		ply->lastElement->next = element;
	} else {
		ply->elements = element;
	}
	ply->lastElement = element;
	ply->numElements++;

	return 0;
}

static int
ply_parse_property(struct ply_parser *ply, char **tokenState)
{
	if (!ply->lastElement) return PLY_ERR_SYNTAX;

	struct ply_property *property = ply_reserve(ply, sizeof *property);

	char *token;
	token = ply_next_token(tokenState, ' ');
	if (!token) return PLY_ERR_SYNTAX;

	if (!strcmp(token, "list")) {
		property->isList = 1;

		token = ply_next_token(tokenState, ' ');
		if (!token) return PLY_ERR_SYNTAX;

		int s = ply_parse_type(token, &property->lengthType);
		if (s < 0) return s;
		if (property->lengthType == PLY_TYPE_FLOAT32 ||
			property->lengthType == PLY_TYPE_FLOAT64) {
			return PLY_ERR_SYNTAX;
		}

		token = ply_next_token(tokenState, ' ');
		if (!token) return PLY_ERR_SYNTAX;
	}

	int s = ply_parse_type(token, &property->scalarType);
	if (s < 0) return s;

	token = ply_next_token(tokenState, ' ');
	if (!token) return PLY_ERR_SYNTAX;

	strncpy(property->name, token, PLY_MAX_NAME - 1);

	struct ply_element *element = ply->lastElement;
	if (element->properties) {
		element->lastProperty->next = property;
	} else {
		element->properties = property;
	}
	element->lastProperty = property;
	element->numProperties++;

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
ply_process_header(struct ply_parser *ply)
{
	// Reset our data structures
	ply->format         = PLY_FORMAT_UNKNOWN;
	ply->elements       = NULL;
	ply->lastElement    = NULL;
	ply->numElements    = 0;
	ply->bufferFill     = 0;

	// Release any memory that may have been sub-allocated from the memory area
	ply->memoryBreak  = ply->memorySize;
	ply->memoryBreak &= ~(size_t)0xF;

	int r = ply->readFunc(ply->readData, ply->memory, PLY_MIN_BUFFER_CAPACITY);
	if (r < 0) return PLY_ERR_READ;
	ply->bufferFill = r;

	if (ply->bufferFill < 4 || !!memcmp(ply->memory, "ply\n", 4)) {
		return PLY_ERR_SYNTAX;
	}
	ply->bufferFill -= 4;
	memmove(ply->memory, ply->memory + 4, ply->bufferFill);

	for (;;) {
		int r = ply->readFunc(ply->readData,
			ply->memory + ply->bufferFill, PLY_MIN_BUFFER_CAPACITY - ply->bufferFill);
		if (r < 0) return PLY_ERR_READ;
		ply->bufferFill += r;

		char *nl = strchr(ply->memory, '\n');
		if (!nl) return PLY_ERR_SYNTAX;
		*nl = 0;

		int s = ply_parse_header_line(ply, ply->memory);
		if (s < 0) return s;

		ply->bufferFill -= nl + 1 - ply->memory;
		memmove(ply->memory, nl + 1, ply->bufferFill);

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
ply_read_scalar_ascii(const char *str, enum ply_type type, union ply_scalar *value)
{
	char *end;
	switch (type) {
	case PLY_TYPE_INT8:
		value->i = strtol(str, &end, 10);
		if (value->i < INT8_MIN) return PLY_ERR_SYNTAX;
		if (value->i > INT8_MAX) return PLY_ERR_SYNTAX;
		break;

	case PLY_TYPE_UINT8:
		value->u = strtoul(str, &end, 10);
		if (value->u > UINT8_MAX) return PLY_ERR_SYNTAX;
		break;

	case PLY_TYPE_INT16:
		value->i = strtol(str, &end, 10);
		if (value->i < INT16_MIN) return PLY_ERR_SYNTAX;
		if (value->i > INT16_MAX) return PLY_ERR_SYNTAX;
		break;

	case PLY_TYPE_UINT16:
		value->u = strtoul(str, &end, 10);
		if (value->u > UINT16_MAX) return PLY_ERR_SYNTAX;
		break;

	case PLY_TYPE_INT32:
		value->i = strtol(str, &end, 10);
		if (value->i < INT32_MIN) return PLY_ERR_SYNTAX;
		if (value->i > INT32_MAX) return PLY_ERR_SYNTAX;
		break;

	case PLY_TYPE_UINT32:
		value->u = strtoul(str, &end, 10);
		if (value->u > UINT32_MAX) return PLY_ERR_SYNTAX;
		break;
	
	case PLY_TYPE_FLOAT32:
		value->f = strtof(str, &end);
		break;

	case PLY_TYPE_FLOAT64:
		value->d = strtod(str, &end);
		break;
	
	default:
		return PLY_ERR_INTERNAL;
	}
	if (!end || end == str) return PLY_ERR_SYNTAX;
	while (*end == ' ') end++;
	return end - str;
}

static int
ply_read_scalar_le(const unsigned char *raw, enum ply_type type, union ply_scalar *value)
{
	uint64_t q;
	switch (type) {
	case PLY_TYPE_INT8:
		value->i  = (int32_t)raw[0];
		return 1;

	case PLY_TYPE_UINT8:
		value->u  = (uint32_t)raw[0];
		return 1;

	case PLY_TYPE_INT16:
		value->i  = (int32_t)raw[0] << 0;
		value->i |= (int32_t)raw[1] << 8;
		return 2;

	case PLY_TYPE_UINT16:
		value->u  = (uint32_t)raw[0] << 0;
		value->u |= (uint32_t)raw[1] << 8;
		return 2;

	case PLY_TYPE_INT32:
	case PLY_TYPE_UINT32:
	case PLY_TYPE_FLOAT32:
		value->u  = (uint32_t)raw[0] <<  0;
		value->u |= (uint32_t)raw[1] <<  8;
		value->u |= (uint32_t)raw[2] << 16;
		value->u |= (uint32_t)raw[3] << 24;
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
		value->d = (double)q;
		return 8;
	
	default:
		return PLY_ERR_INTERNAL;
	}
}

static int
ply_read_scalar_be(const unsigned char *raw, enum ply_type type, union ply_scalar *value)
{
	uint64_t q;
	switch (type) {
	case PLY_TYPE_INT8:
		value->i  = (int32_t)raw[0];
		return 1;

	case PLY_TYPE_UINT8:
		value->u  = (uint32_t)raw[0];
		return 1;

	case PLY_TYPE_INT16:
		value->i  = (int32_t)raw[0] << 8;
		value->i |= (int32_t)raw[1] << 0;
		return 2;

	case PLY_TYPE_UINT16:
		value->u  = (uint32_t)raw[0] << 8;
		value->u |= (uint32_t)raw[1] << 0;
		return 2;

	case PLY_TYPE_INT32:
	case PLY_TYPE_UINT32:
	case PLY_TYPE_FLOAT32:
		value->u  = (uint32_t)raw[0] << 24;
		value->u |= (uint32_t)raw[1] << 16;
		value->u |= (uint32_t)raw[2] <<  8;
		value->u |= (uint32_t)raw[3] <<  0;
		return 4;
	
	case PLY_TYPE_FLOAT64:
		q  = (uint64_t)raw[0] << 56;
		q |= (uint64_t)raw[1] << 48;
		q |= (uint64_t)raw[2] << 40;
		q |= (uint64_t)raw[3] << 32;
		q |= (uint64_t)raw[4] << 24;
		q |= (uint64_t)raw[5] << 16;
		q |= (uint64_t)raw[6] <<  8;
		q |= (uint64_t)raw[7] <<  0;
		value->d = (double)q;
		return 8;
	
	default:
		return PLY_ERR_INTERNAL;
	}
}

static int
ply_read_scalar(enum ply_format format, const char *raw, enum ply_type type, union ply_scalar *value)
{
	switch (format) {
	case PLY_FORMAT_ASCII:
		return ply_read_scalar_ascii(raw, type, value);
	
	case PLY_FORMAT_BINARY_LITTLE_ENDIAN:
		return ply_read_scalar_le((const unsigned char *)raw, type, value);
	
	case PLY_FORMAT_BINARY_BIG_ENDIAN:
		return ply_read_scalar_be((const unsigned char *)raw, type, value);
	
	default:
		return PLY_ERR_INTERNAL;
	}
}

// FIXME support #elems = 0 or #props = 0 or #tuples = 0

int
ply_start_streaming(struct ply_parser *ply)
{
	ply->currentElement  = NULL;
	ply->currentTuple    = (unsigned long)-1;
	ply->currentProperty = NULL;
	ply->currentItem     = 0;
	ply->bufferOffset    = 0;
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

		int r = ply->readFunc(ply->readData, ply->memory + ply->bufferFill, ply->memoryBreak - ply->bufferFill);
		if (r < 0) return PLY_ERR_READ;
		ply->bufferFill += r;

		return 0;
	}

	// Advance to the next (element, tuple, property)
	ply->currentProperty = ply->currentProperty->next;
	if (!ply->currentProperty) {
		ply->currentTuple++;
		if (ply->currentTuple >= ply->currentElement->numTuples) {
			ply->currentElement = ply->currentElement->next;
			if (!ply->currentElement) {
				ply->currentElement = ply->elements;
			}
			ply->currentTuple = 0;
		}
		ply->currentProperty = ply->currentElement->properties;

		if (ply->format == PLY_FORMAT_ASCII) {
			if (ply->memory[ply->bufferOffset] != '\n') {
				return PLY_ERR_SYNTAX;
			}
			ply->bufferOffset++;
		}
		
		// TODO more flexibly refill the buffer only when needed. Both faster and allows for larger tuples.

		ply->bufferFill -= ply->bufferOffset;
		memmove(ply->memory, ply->memory + ply->bufferOffset, ply->bufferFill);
		ply->bufferOffset = 0;

		int r = ply->readFunc(ply->readData, ply->memory + ply->bufferFill, ply->memoryBreak - ply->bufferFill);
		if (r < 0) return PLY_ERR_READ;
		ply->bufferFill += r;
	}
	return 0;
}

int
ply_stream_value(struct ply_parser *ply, union ply_scalar *value)
{
	int r = ply_advance(ply);
	if (r < 0) return r;

	if (ply->currentProperty->isList) {
		union ply_scalar lengthValue;
		r = ply_read_scalar(ply->format, ply->memory + ply->bufferOffset,
			ply->currentProperty->lengthType, &lengthValue);
		if (r < 0) return r;
		ply->bufferOffset += r;

		unsigned listLength;
		switch (ply->currentProperty->lengthType) {
		case PLY_TYPE_INT_:
			if (lengthValue.i < 0) return PLY_ERR_SYNTAX;
			listLength = (unsigned)lengthValue.i;
			break;
		case PLY_TYPE_UINT_: listLength = lengthValue.u; break;
		default: return PLY_ERR_SYNTAX;
		}
		value->u = listLength;
		ply->currentItem = 0;
	} else {
		r = ply_read_scalar(ply->format, ply->memory + ply->bufferOffset,
			ply->currentProperty->scalarType, value);
		if (r < 0) return r;
		ply->bufferOffset += r;
	}

	return 0;
}

int
ply_stream_list_item(struct ply_parser *ply, union ply_scalar *value)
{
	int r;
	r = ply_read_scalar(ply->format, ply->memory + ply->bufferOffset,
		ply->currentProperty->scalarType, value);
	if (r < 0) return r;
	ply->bufferOffset += r;

	// Advance to the next list item
	ply->currentItem++;
	// TODO don't run over the end of the list

	return 0;
}

int
ply_process_with_callbacks(struct ply_parser *ply, const struct ply_handler *handler, void *userdata)
{
	int r = ply_start_streaming(ply);
	if (r < 0) return r;

	PLY_ELEMENT element = ply->elements;
	while (element) {
		if (handler->startElement) {
			r = handler->startElement(userdata, element);
			if (r < 0) return r;
		}
		
		for (unsigned long t = 0; t < element->numTuples; t++) {
			if (handler->startTuple) {
				r = handler->startTuple(userdata, t);
				if (r < 0) return r;
			}

			PLY_PROPERTY property = element->properties;
			while (property) {
				union ply_scalar value;
				r = ply_stream_value(ply, &value);
				if (r < 0) return r;

				if (property->isList) {
					if (handler->startList) {
						r = handler->startList(userdata, property, value.u);
						if (r < 0) return r;
					}

					union ply_scalar item;
					for (unsigned long i = 0; i < value.u; i++) {
						r = ply_stream_list_item(ply, &item);
						if (r < 0) return r;

						if (handler->onListItem) {
							r = handler->onListItem(userdata, item);
							if (r < 0) return r;
						}
					}
					
					if (handler->endList) {
						r = handler->endList(userdata);
						if (r < 0) return r;
					}
				} else {
					if (handler->onScalarValue) {
						r = handler->onScalarValue(userdata, property, value);
						if (r < 0) return r;
					}
				}

				property = property->next;
			}

			if (handler->endTuple) {
				r = handler->endTuple(userdata);
				if (r < 0) return r;
			}
		}

		if (handler->endElement) {
			r = handler->endElement(userdata);
			if (r < 0) return r;
		}

		element = element->next;
	}
	return 0;
}

#define PLY_STORE_CAST_VALUE(destType, dest, scalarType, value)\
	switch (scalarType) {\
		case PLY_TYPE_INT_:    dest = (destType)value.i; break;\
		case PLY_TYPE_UINT_:   dest = (destType)value.u; break;\
		case PLY_TYPE_FLOAT32: dest = (destType)value.f; break;\
		case PLY_TYPE_FLOAT64: dest = (destType)value.d; break;\
	}

union ply_scalar
ply_cast(enum ply_type desiredType, enum ply_type scalarType, union ply_scalar value)
{
	union ply_scalar castValue;
	switch (desiredType) {
	case PLY_TYPE_INT_:    PLY_STORE_CAST_VALUE(int32_t,  castValue.i, scalarType, value); break;
	case PLY_TYPE_UINT_:   PLY_STORE_CAST_VALUE(uint32_t, castValue.u, scalarType, value); break;
	case PLY_TYPE_FLOAT32: PLY_STORE_CAST_VALUE(float,    castValue.f, scalarType, value); break;
	case PLY_TYPE_FLOAT64: PLY_STORE_CAST_VALUE(double,   castValue.d, scalarType, value); break;
	}
	return castValue;
}

#define PLY_NORMALIZE(v, min, max) ((v) < 0 ? -((v) / (min)) : (v) / (max))

union ply_scalar
ply_cast_normalized(enum ply_type desiredType, enum ply_type scalarType, union ply_scalar value)
{
	union ply_scalar castValue;
	switch (desiredType) {
	case PLY_TYPE_INT_:    PLY_STORE_CAST_VALUE(int32_t,  castValue.i, scalarType, value); break;
	case PLY_TYPE_UINT_:   PLY_STORE_CAST_VALUE(uint32_t, castValue.u, scalarType, value); break;

	case PLY_TYPE_FLOAT32:
		switch (scalarType) {
		case PLY_TYPE_INT8:    castValue.f = PLY_NORMALIZE((float)value.i, INT8_MIN,  INT8_MAX);  break;
		case PLY_TYPE_INT16:   castValue.f = PLY_NORMALIZE((float)value.i, INT16_MIN, INT16_MAX); break;
		case PLY_TYPE_INT32:   castValue.f = PLY_NORMALIZE((float)value.i, INT32_MIN, INT32_MAX); break;

		case PLY_TYPE_UINT8:   castValue.f = (float)value.u / UINT8_MAX;  break;
		case PLY_TYPE_UINT16:  castValue.f = (float)value.u / UINT16_MAX; break;
		case PLY_TYPE_UINT32:  castValue.f = (float)value.u / UINT32_MAX; break;

		case PLY_TYPE_FLOAT32: castValue.f = (float)value.f; break;
		case PLY_TYPE_FLOAT64: castValue.f = (float)value.d; break;
		}
		break;
	
	case PLY_TYPE_FLOAT64:
		switch (scalarType) {
		case PLY_TYPE_INT8:    castValue.d = PLY_NORMALIZE((double)value.i, INT8_MIN,  INT8_MAX);  break;
		case PLY_TYPE_INT16:   castValue.d = PLY_NORMALIZE((double)value.i, INT16_MIN, INT16_MAX); break;
		case PLY_TYPE_INT32:   castValue.d = PLY_NORMALIZE((double)value.i, INT32_MIN, INT32_MAX); break;

		case PLY_TYPE_UINT8:   castValue.d = (double)value.u / UINT8_MAX;  break;
		case PLY_TYPE_UINT16:  castValue.d = (double)value.u / UINT16_MAX; break;
		case PLY_TYPE_UINT32:  castValue.d = (double)value.u / UINT32_MAX; break;

		case PLY_TYPE_FLOAT32: castValue.d = (double)value.f; break;
		case PLY_TYPE_FLOAT64: castValue.d = (double)value.d; break;
		}
		break;
	}
	return castValue;
}

#undef PLY_STORE_CAST_VALUE
#undef PLY_NORMALIZE
#undef PLY_TYPE_INT_
#undef PLY_TYPE_UINT_
#undef PLY_TYPE_FLOAT_

#endif
