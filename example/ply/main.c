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

bool
start_element(void *userdata, PLY_ELEMENT element)
{
	(void)userdata;
	const char *name = ply_element_get_name(element);
	printf("static const struct e_%s e_%s[] = {\n", name, name);
	return true;
}

bool
end_element(void *userdata)
{
	(void)userdata;
	printf("};\n\n");
	return true;
}

bool
start_tuple(void *userdata, unsigned long tupleIndex)
{
	(void)userdata;
	(void)tupleIndex;
	printf("\t{ ");
	return true;
}

bool
end_tuple(void *userdata)
{
	(void)userdata;
	printf("},\n");
	return true;
}

bool
on_datum(void *userdata, PLY_PROPERTY property, union ply_datum datum)
{
	(void)userdata;
	switch (ply_property_get_data_type(property)) {
	case PLY_TYPE_INT8:
	case PLY_TYPE_INT16:
	case PLY_TYPE_INT32:
		printf("%"PRId32", ", datum.i);
		break;

	case PLY_TYPE_UINT8:
	case PLY_TYPE_UINT16:
	case PLY_TYPE_UINT32:
		printf("%"PRIu32", ", datum.u);
		break;

	case PLY_TYPE_FLOAT32:
		printf("%f, ", datum.f);
		break;

	case PLY_TYPE_FLOAT64:
		printf("%lf, ", datum.d);
		break;
	}
	return true;
}

bool
start_list(void *userdata, PLY_PROPERTY property, uint32_t length)
{
	(void)userdata;
	itemType = ply_property_get_data_type(property);
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
	return true;
}

bool
end_list(void *userdata)
{
	(void)userdata;
	printf("}, ");
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
		printf("%"PRId32", ", datum.i);
		break;

	case PLY_TYPE_UINT8:
	case PLY_TYPE_UINT16:
	case PLY_TYPE_UINT32:
		printf("%"PRIu32", ", datum.u);
		break;

	case PLY_TYPE_FLOAT32:
		printf("%f, ", datum.f);
		break;

	case PLY_TYPE_FLOAT64:
		printf("%lf, ", datum.d);
		break;
	}
	return true;
}

const struct ply_handler my_handler = {
	.startElement = start_element,
	.endElement   = end_element,
	.startTuple   = start_tuple,
	.endTuple     = end_tuple,
	.onDatum      = on_datum,
	.startList    = start_list,
	.endList      = end_list,
	.onListItem   = on_list_item,
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

	struct ply_parser ply = { 0 };

	ply_parser_reset(&ply);

	size_t workSize = 16 * 1024 * 1024;
	void *workArea = calloc(1, workSize);

	ply_parser_set_work_area(&ply, workArea, workSize);
	ply_parser_set_input(&ply, file_read_callback, plyFile);
	ply_parser_set_handler(&ply, &my_handler);

	int s = ply_parse_header(&ply);
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
			switch (ply_property_get_data_type(property)) {
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

#if 1
	ply_parser_start_streaming(&ply);
	for (unsigned long e = 0; e < ply_parser_get_element_count(&ply); e++) {
		PLY_ELEMENT element = ply_parser_get_element(&ply, e);
		my_handler.startElement(NULL, element);
		for (unsigned long t = 0; t < ply_element_get_tuple_count(element); t++) {
			my_handler.startTuple(NULL, t);
			PLY_PROPERTY property = element->properties;
			while (property) {
				union ply_datum datum;
				s = ply_advance(&ply);
				s = ply_stream_value(&ply, &datum);
				if (ply_property_is_list(property)) {
					my_handler.startList(NULL, property, datum.u);
					union ply_datum item;
					for (unsigned long i = 0; i < datum.u; i++) {
						s = ply_stream_value(&ply, &item);
						my_handler.onListItem(NULL, item);
					}
					my_handler.endList(NULL);
				} else {
					my_handler.onDatum(NULL, property, datum);
				}
				property = property->next;
			}
			my_handler.endTuple(NULL);
		}
		my_handler.endElement(NULL);
	}
#else
	s = ply_parse_contents(&ply);
	if (s < 0) {
		fclose(plyFile);
		free(workArea);
		fprintf(stderr, "can't parse ply contents\n");
		return 1;
	}
#endif

	printf("#endif\n");

	fclose(plyFile);
	free(workArea);
	return 0;
}
