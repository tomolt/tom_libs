#include <stdlib.h>
#include <stdio.h>

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
	ply.workSize = 16 * 1024 * 1024;
	ply.workArea = calloc(1, ply.workSize);
	ply.workBreak = ply.workSize;
	ply.workBreak &= ~(size_t)0xF;
	ply.userdata = plyFile;
	ply.read_cb = file_read_callback;

	int s = ply_parse_header(&ply);
	if (s < 0) {
		fprintf(stderr, "can't parse ply file\n");
		return 1;
	}

	struct ply_element *element = ply.elements;
	while (element) {
		printf("ELEMENT %s\n", element->name);
		struct ply_property *property = element->properties;
		while (property) {
			printf("PROPERTY %s\n", property->name);
			property = property->next;
		}
		element = element->next;
	}

	free(ply.workArea);
	return 0;
}
