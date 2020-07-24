// (c) Copyright 2020, Sean Connelly (@velipso), sean.cm
// MIT License
// Project Home: https://github.com/velipso/nightmare

#ifndef NIGHTMARE__H
#define NIGHTMARE__H

#include <stdint.h>

typedef void *(*nm_malloc_f)(size_t sz);
typedef void *(*nm_realloc_f)(void *ptr, size_t sz);
typedef void (*nm_free_f)(void *ptr);

// all memory management is through these functions, which default to stdlib's malloc/realloc/free
// overwrite these global variables with your own functions if desired
extern nm_malloc_f  nm_malloc;
extern nm_realloc_f nm_realloc;
extern nm_free_f    nm_free;

#endif // NIGHTMARE__H
