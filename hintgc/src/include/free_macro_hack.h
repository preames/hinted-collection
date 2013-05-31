

#ifndef __FREE_MACRO_HACK_H__
#define __FREE_MACRO_HACK_H__

#include <stdlib.h>
#include "gc.h"


#ifdef free
#error "free(p) previously defined -- error!"
#endif

/// This only works when __ptr is a lvalue (not an rvalue)
/// This works for fairly few real usage of ptrs. 
///  i.e. free(ptr = malloc(...) does not work
#define free(__ptr) do { GC_free(__ptr); __ptr = NULL; } while(0);

#endif
