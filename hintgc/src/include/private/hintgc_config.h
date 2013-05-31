
#ifndef __HINTGC_CONFIG_H_
#define __HINTGC_CONFIG_H_

/* Build with either or both of these libraries - varies by machine */

// DBGUTILS is a separate library which provides useful debugging macros
// This is not manditory, but if you're doing active development, it
// will likely help to have this installed
// You can find it at https://www.github.com:preames/dbgutils
//#define BUILD_WITH_DBGUTILS

// PAPI is the Performance API instrumentation toolkit.  We use this
// to collected information about the cache behavior etc.  
// NOTE: In any multiple-thread build, do NOT enable PAPI.  PAPI has
// these nice per thread data structures which can allocated via malloc.
// This causes a deadlock.  PAPI results can only be used in a single
// threaded build
// NOTE: Despite the above, we always link against PAPI.  Even in multiple
// thread builds, we use their timer functions.
//#define BUILD_WITH_PAPI
#endif
