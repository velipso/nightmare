// (c) Copyright 2020, Sean Connelly (@velipso), sean.cm
// MIT License
// Project Home: https://github.com/velipso/nightmare

#include "nightmare.h"

#include <stdlib.h>

sink_malloc_f  sink_malloc  = malloc;
sink_realloc_f sink_realloc = realloc;
sink_free_f    sink_free    = free;
