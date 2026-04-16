#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#define PLY_IMPLEMENTATION
#include <tom_ply.h>

int
file_read_callback(void *file, void *buffer, unsigned max)
{
	size_t got = fread(buffer, 1, max, file);
	if (!got && ferror(file)) {
		return -1;
	}
	return (int)got;
}

static unsigned tupleCounter;
static enum ply_type itemType;

bool
start_element(void *userdata, const char *name)
{
	(void)userdata;
	printf("ELEMENT %s\n", name);
	tupleCounter = 0;
	return true;
}

bool
start_tuple(void *userdata)
{
	(void)userdata;
	printf("  TUPLE %u\n", tupleCounter);
	tupleCounter++;
	return true;
}

bool
start_list(void *userdata, unsigned length, enum ply_type type)
{
	(void)userdata;
	itemType = type;
	char typeChar;
	switch (itemType) {
	case PLY_TYPE_INT8:
	case PLY_TYPE_INT16:
	case PLY_TYPE_INT32:
		typeChar = 'I';
		break;

	case PLY_TYPE_UINT8:
	case PLY_TYPE_UINT16:
	case PLY_TYPE_UINT32:
		typeChar = 'U';
		break;

	case PLY_TYPE_FLOAT32:
		typeChar = 'F';
		break;

	case PLY_TYPE_FLOAT64:
		typeChar = 'D';
		break;
	}
	printf("    L %u of %c [ ", length, typeChar);
	return true;
}

bool
end_list(void *userdata)
{
	(void)userdata;
	printf("]\n");
	return true;
}

bool
on_list_item(void *userdata, union ply_datum datum)
{
	(void)userdata;
	switch (itemType) {
	case PLY_TYPE_INT8:
	case PLY_TYPE_INT16:
	case PLY_TYPE_INT32:
		printf("%"PRId32" ", datum.i);
		break;

	case PLY_TYPE_UINT8:
	case PLY_TYPE_UINT16:
	case PLY_TYPE_UINT32:
		printf("%"PRIu32" ", datum.u);
		break;

	case PLY_TYPE_FLOAT32:
		printf("%f ", datum.f);
		break;

	case PLY_TYPE_FLOAT64:
		printf("%lf ", datum.d);
		break;
	}
	return true;
}

bool
on_datum(void *userdata, enum ply_type type, union ply_datum datum)
{
	(void)userdata;
	switch (type) {
	case PLY_TYPE_INT8:
	case PLY_TYPE_INT16:
	case PLY_TYPE_INT32:
		printf("    I %"PRId32"\n", datum.i);
		break;

	case PLY_TYPE_UINT8:
	case PLY_TYPE_UINT16:
	case PLY_TYPE_UINT32:
		printf("    U %"PRIu32"\n", datum.u);
		break;

	case PLY_TYPE_FLOAT32:
		printf("    F %f\n", datum.f);
		break;

	case PLY_TYPE_FLOAT64:
		printf("    D %lf\n", datum.d);
		break;
	}
	return true;
}

int
main(int argc, const char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: printply <file>\n");
		return 1;
	}

	FILE *plyFile = fopen(argv[1], "rb");
	if (!plyFile) {
		fprintf(stderr, "can't open ply file\n");
		return 1;
	}

	struct ply_parser ply = { 0 };
	ply.workSize = 16 * 1024 * 1024;
	ply.workArea = calloc(1, ply.workSize);
	ply.workBreak = ply.workSize;
	ply.workBreak &= ~(size_t)0xF;
	ply.readData = plyFile;
	ply.readFunc = file_read_callback;

	int s = ply_parse_header(&ply);
	if (s < 0) {
		fclose(plyFile);
		fprintf(stderr, "can't parse ply header\n");
		return 1;
	}

	printf("HEADER\n======\n");
	struct ply_element *element = ply.elements;
	while (element) {
		printf("ELEMENT %s\n", element->name);
		struct ply_property *property = element->properties;
		while (property) {
			printf("  PROPERTY %s\n", property->name);
			property = property->next;
		}
		element = element->next;
	}

	printf("\nCONTENTS\n========\n");
	ply.handler.startElement = start_element;
	ply.handler.startTuple = start_tuple;
	ply.handler.startList = start_list;
	ply.handler.endList = end_list;
	ply.handler.onListItem = on_list_item;
	ply.handler.onDatum = on_datum;
	s = ply_parse_contents(&ply);
	if (s < 0) {
		fclose(plyFile);
		fprintf(stderr, "can't parse ply contents\n");
		return 1;
	}

	fclose(plyFile);
	free(ply.workArea);
	return 0;
}
