// (c) Copyright 2017, Sean Connelly (@voidqk), http://syntheti.cc
// MIT License
// Project Home: https://github.com/voidqk/nightmare

// sink bindings for nightmare

#ifndef SINK_NIGHTMARE__H
#define SINK_NIGHTMARE__H

#include "sink.h"
#include "nightmare.h"

void sink_nightmare_scr(sink_scr scr);
bool sink_nightmare_ctx(sink_ctx ctx, nm_ctx nctx); // returns false for out of memory

#endif // SINK_NIGHTMARE__H
