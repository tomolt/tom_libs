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

const char *
to_c_type(enum ply_type type)
{
	switch (type) {
	case PLY_TYPE_INT8:    return "int8_t";
	case PLY_TYPE_INT16:   return "int16_t";
	case PLY_TYPE_INT32:   return "int32_t";
	case PLY_TYPE_UINT8:   return "uint8_t";
	case PLY_TYPE_UINT16:  return "uint16_t";
	case PLY_TYPE_UINT32:  return "uint32_t";
	case PLY_TYPE_FLOAT32: return "float";
	case PLY_TYPE_FLOAT64: return "double";
	default:               return "";
	}
}

void
print_c_value(enum ply_type type, union ply_scalar value)
{
	switch (type) {
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
}

struct handler_data {
	const char *basename;
	enum ply_type itemType;
};

int
start_element(void *userdata, PLY_ELEMENT element)
{
	struct handler_data *data = userdata;
	const char *name = ply_element_get_name(element);
	printf("static const struct %s_%s %s_%s[] = {\n",
		data->basename, name, data->basename, name);
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
	print_c_value(ply_property_get_scalar_type(property), value);
	return 0;
}

int
start_list(void *userdata, PLY_PROPERTY property, uint32_t length)
{
	struct handler_data *data = userdata;
	data->itemType = ply_property_get_scalar_type(property);
	printf("%u, (%s[]){ ", length, to_c_type(data->itemType));
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
	struct handler_data *data = userdata;
	print_c_value(data->itemType, value);
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

	// Extract the base name of the file, in a hacky way ...
	char basename[100];
	strncpy(basename, argv[1], sizeof(basename) - 1);
	basename[sizeof(basename) - 1] = 0;
	char *split;
	while ((split = strchr(basename, '/'))) {
		memmove(basename, split + 1, sizeof(basename) - (split + 1 - basename));
	}
	if ((split = strchr(basename, '.'))) {
		*split = 0;
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
		"#ifndef %s_PLY_H\n"
		"#define %s_PLY_H\n\n"
		"#include <stdint.h>\n\n",
		basename, basename);

	unsigned long numElements = ply_parser_get_element_count(&ply);
	for (unsigned long e = 0; e < numElements; e++) {
		PLY_ELEMENT element = ply_parser_get_element(&ply, e);
		printf("struct %s_%s {\n", basename, ply_element_get_name(element));

		for (unsigned long p = 0; p < ply_element_get_property_count(element); p++) {
			PLY_PROPERTY property = ply_element_get_property(element, p);
			const char *scalarType = to_c_type(ply_property_get_scalar_type(property));
			const char *name = ply_property_get_name(property);
			if (ply_property_is_list(property)) {
				const char *lengthType = to_c_type(ply_property_get_length_type(property));
				printf("\t%s n_%s;\n", lengthType, name);
				printf("\t%s *p_%s;\n", scalarType, name);
			} else {
				printf("\t%s p_%s;\n", scalarType, name);
			}
			property = property->next;
		}

		printf("};\n\n");
	}

	struct handler_data data;
	data.basename = basename;
	s = ply_process_with_callbacks(&ply, &my_handler, &data);
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
