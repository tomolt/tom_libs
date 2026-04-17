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

static enum ply_type itemType;

int
start_element(void *userdata, PLY_ELEMENT element)
{
	(void)userdata;
	const char *name = ply_element_get_name(element);
	printf("static const struct e_%s e_%s[] = {\n", name, name);
	return 0;
}

int
end_element(void *userdata)
{
	(void)userdata;
	printf("};\n\n");
	return 0;
}

int
start_tuple(void *userdata, unsigned long tupleIndex)
{
	(void)userdata;
	(void)tupleIndex;
	printf("\t{ ");
	return 0;
}

int
end_tuple(void *userdata)
{
	(void)userdata;
	printf("},\n");
	return 0;
}

int
on_scalar_value(void *userdata, PLY_PROPERTY property, union ply_scalar value)
{
	(void)userdata;
	switch (ply_property_get_scalar_type(property)) {
	case PLY_TYPE_INT8:
	case PLY_TYPE_INT16:
	case PLY_TYPE_INT32:
		printf("%"PRId32", ", value.i);
		break;

	case PLY_TYPE_UINT8:
	case PLY_TYPE_UINT16:
	case PLY_TYPE_UINT32:
		printf("%"PRIu32", ", value.u);
		break;

	case PLY_TYPE_FLOAT32:
		printf("%f, ", value.f);
		break;

	case PLY_TYPE_FLOAT64:
		printf("%lf, ", value.d);
		break;
	}
	return 0;
}

int
start_list(void *userdata, PLY_PROPERTY property, uint32_t length)
{
	(void)userdata;
	itemType = ply_property_get_scalar_type(property);
	const char *typeName = "";
	switch (itemType) {
		case PLY_TYPE_INT8:    typeName = "int8_t";   break;
		case PLY_TYPE_INT16:   typeName = "int16_t";  break;
		case PLY_TYPE_INT32:   typeName = "int32_t";  break;
		case PLY_TYPE_UINT8:   typeName = "uint8_t";  break;
		case PLY_TYPE_UINT16:  typeName = "uint16_t"; break;
		case PLY_TYPE_UINT32:  typeName = "uint32_t"; break;
		case PLY_TYPE_FLOAT32: typeName = "float";    break;
		case PLY_TYPE_FLOAT64: typeName = "double";   break;
	}
	printf("%u, (%s[]){ ", length, typeName);
	return 0;
}

int
end_list(void *userdata)
{
	(void)userdata;
	printf("}, ");
	return 0;
}

int
on_list_item(void *userdata, union ply_scalar value)
{
	(void)userdata;
	switch (itemType) {
	case PLY_TYPE_INT8:
	case PLY_TYPE_INT16:
	case PLY_TYPE_INT32:
		printf("%"PRId32", ", value.i);
		break;

	case PLY_TYPE_UINT8:
	case PLY_TYPE_UINT16:
	case PLY_TYPE_UINT32:
		printf("%"PRIu32", ", value.u);
		break;

	case PLY_TYPE_FLOAT32:
		printf("%f, ", value.f);
		break;

	case PLY_TYPE_FLOAT64:
		printf("%lf, ", value.d);
		break;
	}
	return 0;
}

const struct ply_handler my_handler = {
	.startElement  = start_element,
	.endElement    = end_element,
	.startTuple    = start_tuple,
	.endTuple      = end_tuple,
	.onScalarValue = on_scalar_value,
	.startList     = start_list,
	.endList       = end_list,
	.onListItem    = on_list_item,
};

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

	struct ply_parser ply;

	size_t workSize = 16 * 1024 * 1024;
	void *workArea = calloc(1, workSize);

	ply_parser_set_memory(&ply, workArea, workSize);
	ply_parser_set_input(&ply, file_read_callback, plyFile);

	int s = ply_process_header(&ply);
	if (s < 0) {
		fclose(plyFile);
		free(workArea);
		fprintf(stderr, "can't parse ply header\n");
		return 1;
	}

	printf(
		"#ifndef PLY_CONTENTS_H\n"
		"#define PLY_CONTENTS_H\n\n"
		"#include <stdint.h>\n\n");

	unsigned long numElements = ply_parser_get_element_count(&ply);
	for (unsigned long e = 0; e < numElements; e++) {
		PLY_ELEMENT element = ply_parser_get_element(&ply, e);
		printf("struct e_%s {\n", ply_element_get_name(element));

		// TODO
		PLY_PROPERTY property = element->properties;
		while (property) {
			const char *typeName = "";
			switch (ply_property_get_scalar_type(property)) {
			case PLY_TYPE_INT8:    typeName = "int8_t";   break;
			case PLY_TYPE_INT16:   typeName = "int16_t";  break;
			case PLY_TYPE_INT32:   typeName = "int32_t";  break;
			case PLY_TYPE_UINT8:   typeName = "uint8_t";  break;
			case PLY_TYPE_UINT16:  typeName = "uint16_t"; break;
			case PLY_TYPE_UINT32:  typeName = "uint32_t"; break;
			case PLY_TYPE_FLOAT32: typeName = "float";    break;
			case PLY_TYPE_FLOAT64: typeName = "double";   break;
			}
			const char *name = ply_property_get_name(property);
			if (ply_property_is_list(property)) {
				printf("\t%s n_%s;\n", "int", name); // TODO
				printf("\t%s *p_%s;\n", typeName, name);
			} else {
				printf("\t%s p_%s;\n", typeName, name);
			}
			property = property->next;
		}

		printf("};\n\n");
	}

	s = ply_process_with_callbacks(&ply, &my_handler, NULL);
	if (s < 0) {
		fclose(plyFile);
		free(workArea);
		fprintf(stderr, "can't parse ply contents\n");
		return 1;
	}

	printf("#endif\n");

	fclose(plyFile);
	free(workArea);
	return 0;
}
