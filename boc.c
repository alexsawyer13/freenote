#include "boc.h"

int boc_main(boc *b)
{
    boc_add_exec("note");

	boc_add_include("include");

	boc_add_src("vendor/glad.c");
    boc_add_src("src/freenote.c");
    boc_add_src("src/clib.c");

	boc_add_lib_dir("lib");

	boc_add_lib("glfw3");
	boc_add_lib("m");
	boc_add_lib("GL");
//	boc_add_lib("X11");
//	boc_add_lib("pthread");
//	boc_add_lib("Xrandr");
//	boc_add_lib("Xi");
    return 0;
}
