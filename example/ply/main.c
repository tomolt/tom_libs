#include <stdlib.h>
#include <stdio.h>

#define PLY_IMPLEMENTATION
#include <tom_ply.h>

int
main(int argc, const char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "usage: printply <file>\n");
		return 1;
	}

	struct ply_parser ply;
	ply.workSize = 16 * 1024 * 1024;
	ply.workArea = calloc(1, ply.workSize);
	ply.workBreak = ply.workSize;
	ply.workBreak &= ~(size_t)0xF;

	int s = ply_load_file(&ply, argv[1]);
	if (s < 0) {
		fprintf(stderr, "can't load ply file\n");
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
