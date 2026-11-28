#define DH_IMPLEMENT_HERE 1
#include "dh_cuts.h"

#define PATHUTIL_IMPLEMENTATION 1
#include "../../tom_pathutil.h"

#include <stdlib.h>
#include <string.h>

static void check_normpath(const char *path, const char *expected)
{
	size_t limit = strlen(path) + 1;
	char *buffer = malloc(limit);
	memcpy(buffer, path, limit);

	pathutil_normpath(buffer);

	dh_assert(memchr(buffer, 0, limit) != NULL);
	dh_assertsq(buffer, expected);

	free(buffer);
}

static void test_normpath(void)
{
	dh_push("pathutil_normpath()");

	// these cases should remain unchanged
	check_normpath("/", "/");
	check_normpath(".", ".");
	check_normpath("..", "..");
	check_normpath("foo", "foo");
	check_normpath("/bar", "/bar");
	check_normpath(".gitignore", ".gitignore");
	check_normpath("/dir/image.jpeg", "/dir/image.jpeg");

	// extra slash removal
	check_normpath("///", "/");
	check_normpath("foo/", "foo");
	check_normpath("/foo/", "/foo");
	check_normpath("//foo", "/foo");
	check_normpath("foo//", "foo");
	check_normpath("foo/bar//baz/", "foo/bar/baz");
	check_normpath("./", ".");

	// single dot
	check_normpath("./.", ".");
	check_normpath("/.", "/");
	check_normpath(".//././", ".");
	check_normpath("foo/./bar/./baz", "foo/bar/baz");
	check_normpath("./../.", "..");

	// double dot
	check_normpath("../..", "../..");
	check_normpath("/..", "/");
	check_normpath("foo/..", ".");
	check_normpath("xyz/foo/../bar/../", "xyz");
	check_normpath("xyz/foo/../bar/../../", ".");
	check_normpath("../sister/my_folder/..", "../sister");
	check_normpath("/../../foo/bar/baz/..", "/foo/bar");
	
	dh_pop();
}

int main()
{
	dh_init(stderr);

	dh_branch( test_normpath(); );
	
	return dh_summarize();
}
