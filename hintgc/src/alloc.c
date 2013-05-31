/*
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991-1996 by Xerox Corporation.  All rights reserved.
 * Copyright (c) 1998 by Silicon Graphics.  All rights reserved.
 * Copyright (c) 1999-2004 Hewlett-Packard Development Company, L.P.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 *
 */

#include "private/gc_priv.h"

#include <stdio.h>
#if !defined(MACOS) && !defined(MSWINCE)
# include <signal.h>
# if !defined(__CC_ARM)
#   include <sys/types.h>
# endif
#endif

/*
 * Separate free lists are maintained for different sized objects
 * up to MAXOBJBYTES.
 * The call GC_allocobj(i,k) ensures that the freelist for
 * kind k objects of size i points to a non-empty
 * free list. It returns a pointer to the first entry on the free list.
 * In a single-threaded world, GC_allocobj may be called to allocate
 * an object of (small) size i as follows:
 *
 *            opp = &(GC_objfreelist[i]);
 *            if (*opp == 0) GC_allocobj(i, NORMAL);
 *            ptr = *opp;
 *            *opp = obj_link(ptr);
 *
 * Note that this is very fast if the free list is non-empty; it should
 * only involve the execution of 4 or 5 simple instructions.
 * All composite objects on freelists are cleared, except for
 * their first word.
 */

/*
 * The allocator uses GC_allochblk to allocate large chunks of objects.
 * These chunks all start on addresses which are multiples of
 * HBLKSZ.   Each allocated chunk has an associated header,
 * which can be located quickly based on the address of the chunk.
 * (See headers.c for details.)
 * This makes it possible to check quickly whether an
 * arbitrary address corresponds to an object administered by the
 * allocator.
 */

word GC_non_gc_bytes = 0;  /* Number of bytes not intended to be collected */

word GC_gc_no = 0;

#ifndef GC_DISABLE_INCREMENTAL
  GC_INNER int GC_incremental = 0;      /* By default, stop the world.  */
#endif

#ifdef THREADS
  int GC_parallel = FALSE;      /* By default, parallel GC is off.      */
#endif

#ifndef GC_FULL_FREQ
# define GC_FULL_FREQ 19   /* Every 20th collection is a full   */
                           /* collection, whether we need it    */
                           /* or not.                           */
#endif

int GC_full_freq = GC_FULL_FREQ;

STATIC GC_bool GC_need_full_gc = FALSE;
                           /* Need full GC do to heap growth.   */

#ifdef THREAD_LOCAL_ALLOC
  GC_INNER GC_bool GC_world_stopped = FALSE;
#endif

STATIC word GC_used_heap_size_after_full = 0;

/* GC_copyright symbol is externally visible. */
char * const GC_copyright[] =
{"Copyright 1988,1989 Hans-J. Boehm and Alan J. Demers ",
"Copyright (c) 1991-1995 by Xerox Corporation.  All rights reserved. ",
"Copyright (c) 1996-1998 by Silicon Graphics.  All rights reserved. ",
"Copyright (c) 1999-2009 by Hewlett-Packard Company.  All rights reserved. ",
"THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY",
" EXPRESSED OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.",
"See source code for details." };

/* Version macros are now defined in gc_version.h, which is included by */
/* gc.h, which is included by gc_priv.h.                                */
#ifndef GC_NO_VERSION_VAR
  const unsigned GC_version = ((GC_VERSION_MAJOR << 16) |
                        (GC_VERSION_MINOR << 8) | GC_TMP_ALPHA_VERSION);
#endif

GC_API unsigned GC_CALL GC_get_version(void)
{
  return (GC_VERSION_MAJOR << 16) | (GC_VERSION_MINOR << 8) |
          GC_TMP_ALPHA_VERSION;
}

GC_INLINE word GC_max(word x, word y)
{
    return(x > y? x : y);
}

GC_INLINE word GC_min(word x, word y)
{
    return(x < y? x : y);
}


/* some more variables */

#ifdef GC_DONT_EXPAND
  GC_bool GC_dont_expand = TRUE;
#else
  GC_bool GC_dont_expand = FALSE;
#endif

#ifndef GC_FREE_SPACE_DIVISOR
# define GC_FREE_SPACE_DIVISOR 3 /* must be > 0 */
#endif

word GC_free_space_divisor = GC_FREE_SPACE_DIVISOR;

GC_INNER int GC_CALLBACK GC_never_stop_func(void)
{
  return(0);
}

#ifndef GC_TIME_LIMIT
# define GC_TIME_LIMIT 50  /* We try to keep pause times from exceeding  */
                           /* this by much. In milliseconds.             */
#endif

unsigned long GC_time_limit = GC_TIME_LIMIT;

#ifndef NO_CLOCK
  STATIC CLOCK_TYPE GC_start_time = 0;
                                /* Time at which we stopped world.      */
                                /* used only in GC_timeout_stop_func.   */
#endif

STATIC int GC_n_attempts = 0;   /* Number of attempts at finishing      */
                                /* collection within GC_time_limit.     */

STATIC GC_stop_func GC_default_stop_func = GC_never_stop_func;
                                /* accessed holding the lock.           */

GC_API void GC_CALL GC_set_stop_func(GC_stop_func stop_func)
{
  DCL_LOCK_STATE;
  GC_ASSERT(stop_func != 0);
  LOCK();
  GC_default_stop_func = stop_func;
  UNLOCK();
}

GC_API GC_stop_func GC_CALL GC_get_stop_func(void)
{
  GC_stop_func stop_func;
  DCL_LOCK_STATE;
  LOCK();
  stop_func = GC_default_stop_func;
  UNLOCK();
  return stop_func;
}

#if defined(GC_DISABLE_INCREMENTAL) || defined(NO_CLOCK)
# define GC_timeout_stop_func GC_default_stop_func
#else
  STATIC int GC_CALLBACK GC_timeout_stop_func (void)
  {
    CLOCK_TYPE current_time;
    static unsigned count = 0;
    unsigned long time_diff;

    if ((*GC_default_stop_func)())
      return(1);

    if ((count++ & 3) != 0) return(0);
    GET_TIME(current_time);
    time_diff = MS_TIME_DIFF(current_time,GC_start_time);
    if (time_diff >= GC_time_limit) {
        if (GC_print_stats) {
          GC_log_printf(
                "Abandoning stopped marking after %lu msecs (attempt %d)\n",
                time_diff, GC_n_attempts);
        }
        return(1);
    }
    return(0);
  }
#endif /* !GC_DISABLE_INCREMENTAL */

#ifdef THREADS
  GC_INNER word GC_total_stacksize = 0; /* updated on every push_all_stacks */
#endif

/* Return the minimum number of words that must be allocated between    */
/* collections to amortize the collection cost.                         */
static word min_bytes_allocd(void)
{
#   ifdef STACK_GROWS_UP
      word stack_size = GC_approx_sp() - GC_stackbottom;
            /* GC_stackbottom is used only for a single-threaded case.  */
#   else
      word stack_size = GC_stackbottom - GC_approx_sp();
#   endif

    word total_root_size;       /* includes double stack size,  */
                                /* since the stack is expensive */
                                /* to scan.                     */
    word scan_size;             /* Estimate of memory to be scanned     */
                                /* during normal GC.                    */

#   ifdef THREADS
      if (GC_need_to_lock) {
        /* We are multi-threaded... */
        stack_size = GC_total_stacksize;
        /* For now, we just use the value computed during the latest GC. */
#       ifdef DEBUG_THREADS
          GC_log_printf("Total stacks size: %lu\n",
                        (unsigned long)stack_size);
#       endif
      }
#   endif

    total_root_size = 2 * stack_size + GC_root_size;
    scan_size = 2 * GC_composite_in_use + GC_atomic_in_use / 4
                + total_root_size;
    if (GC_incremental) {
        return scan_size / (2 * GC_free_space_divisor);
    } else {
        return scan_size / GC_free_space_divisor;
    }
}

/* Return the number of bytes allocated, adjusted for explicit storage  */
/* management, etc..  This number is used in deciding when to trigger   */
/* collections.                                                         */
STATIC word GC_adj_bytes_allocd(void)
{
    signed_word result;
    signed_word expl_managed = (signed_word)GC_non_gc_bytes
                                - (signed_word)GC_non_gc_bytes_at_gc;

    /* Don't count what was explicitly freed, or newly allocated for    */
    /* explicit management.  Note that deallocating an explicitly       */
    /* managed object should not alter result, assuming the client      */
    /* is playing by the rules.                                         */
#if ENABLE_HINTGC_INTEGRATION
    // We played with adding the amount reclaimed since the last collection
    // to this heuristic.  We didn't try tracking the amount expected
    // to be recovered (i.e. amount hinted.)
    // Oddly, this causes the heap expansion to trigger.  Not sure why..
    // Also, give credit for any space we've reclaimed since the 
    // last collection.  Without this, pending free collections don't
    // influence the decision heuristic at all.  (Incremental has a
    // different fall back, but might benefit from this too. Untested.)
    // Okay, with the max bits, it mostly works, but it makes the gctest
    // result non-deterministic.  Sometimes the heap is under limit, 
    // sometimes it isn't.
#endif
    result = (signed_word)GC_bytes_allocd
             + (signed_word)GC_bytes_dropped
             - (signed_word)GC_bytes_freed
             + (signed_word)GC_finalizer_bytes_freed
             - expl_managed;
      //- GC_max(0, GC_bytes_found);
    if (result > (signed_word)GC_bytes_allocd) {
        result = GC_bytes_allocd;
        /* probably client bug or unfortunate scheduling */
    }
    result += GC_bytes_finalized;
        /* We count objects enqueued for finalization as though they    */
        /* had been reallocated this round. Finalization is user        */
        /* visible progress.  And if we don't count this, we have       */
        /* stability problems for programs that finalize all objects.   */
    if (result < (signed_word)(GC_bytes_allocd >> 3)) {
        /* Always count at least 1/8 of the allocations.  We don't want */
        /* to collect too infrequently, since that would inhibit        */
        /* coalescing of free storage blocks.                           */
        /* This also makes us partially robust against client bugs.     */
        return(GC_bytes_allocd >> 3);
    } else {
        return(result);
    }
}


/* Clear up a few frames worth of garbage left at the top of the stack. */
/* This is used to prevent us from accidentally treating garbage left   */
/* on the stack by other parts of the collector as roots.  This         */
/* differs from the code in misc.c, which actually tries to keep the    */
/* stack clear of long-lived, client-generated garbage.                 */
STATIC void GC_clear_a_few_frames(void)
{
#   ifndef CLEAR_NWORDS
#     define CLEAR_NWORDS 64
#   endif
    volatile word frames[CLEAR_NWORDS];
    BZERO((word *)frames, CLEAR_NWORDS * sizeof(word));
}

/* Heap size at which we need a collection to avoid expanding past      */
/* limits used by blacklisting.                                         */
STATIC word GC_collect_at_heapsize = (word)(-1);

/* Have we allocated enough to amortize a collection? */
GC_INNER GC_bool GC_should_collect(void)
{
    static word last_min_bytes_allocd;
    static word last_gc_no;
    if (last_gc_no != GC_gc_no) {
      last_gc_no = GC_gc_no;
      last_min_bytes_allocd = min_bytes_allocd();
    }
    const GC_bool rval = (GC_adj_bytes_allocd() >= last_min_bytes_allocd
                          || GC_heapsize >= GC_collect_at_heapsize);
    if( rval ) { 
    }
    return rval;
}

/* STATIC */ GC_start_callback_proc GC_start_call_back = 0;
                        /* Called at start of full collections.         */
                        /* Not called if 0.  Called with the allocation */
                        /* lock held.  Not used by GC itself.           */

GC_API void GC_CALL GC_set_start_callback(GC_start_callback_proc fn)
{
    DCL_LOCK_STATE;
    LOCK();
    GC_start_call_back = fn;
    UNLOCK();
}

GC_API GC_start_callback_proc GC_CALL GC_get_start_callback(void)
{
    GC_start_callback_proc fn;
    DCL_LOCK_STATE;
    LOCK();
    fn = GC_start_call_back;
    UNLOCK();
    return fn;
}

GC_INLINE void GC_notify_full_gc(void)
{
    if (GC_start_call_back != 0) {
        (*GC_start_call_back)();
    }
}

STATIC GC_bool GC_is_full_gc = FALSE;

STATIC GC_bool GC_stopped_mark(GC_stop_func stop_func);
STATIC GC_bool GC_hintgc_stopped_mark(GC_stop_func stop_func);
STATIC void GC_finish_collection(GC_bool reset_metadata);


#define ENABLE_HINTGC_INTEGRATION 0

static GC_bool has_pending_frees();
#if ENABLE_HINTGC_INTEGRATION
static GC_bool HINTGC_should_collect();
#endif
GC_INNER GC_bool HINTGC_collect_inner(GC_stop_func stop_func);
static void print_pf_heuristic_data(int fd);

static const char* stat_filename() {
  if (GETENV("pf_stats_file") != 0) {
    return GETENV("pf_stats_file");
  } else {
    return "hintgc-stats.csv";
  }
}

void GC_fd_printf(int fd, const char *format, ...);

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <string.h>

// LEGACY CODE -- DO NOT USE
GC_API void GC_CALL GC_pf_stat_enable(int b) {
  HINTGC_stat_enable(b);
}
GC_API void GC_CALL GC_pf_stat_gc_only(int b) {
  HINTGC_stat_gc_only(b);
}

GC_API void GC_pfcollect() { 
  HINTGC_collect();
}
// LEGACY CODE -- DO NOT USE

static GC_bool g_enable_pending_free_stats = 0;
static GC_bool g_enable_pending_free_stats_set = 0;
GC_INNER GC_bool enable_pending_free_stats() {
  if( g_enable_pending_free_stats_set) {
    return g_enable_pending_free_stats;
  }
  else {
    //enabled by default
    return 0 == GETENV("pf_stats_default_disable");
  }
}
GC_API void GC_CALL HINTGC_stat_enable(int b) {
  g_enable_pending_free_stats = b;
  g_enable_pending_free_stats_set = TRUE;
}


static GC_bool g_pf_stat_gc_only = 0;
static GC_bool g_pf_stat_gc_only_set = 0;
GC_API void GC_CALL HINTGC_stat_gc_only(int b) {
  g_pf_stat_gc_only = b;
  g_pf_stat_gc_only_set = TRUE;
}
int pf_stat_gc_only() {
  if( g_pf_stat_gc_only_set) {
    return g_pf_stat_gc_only;
  }
  else {
    //disabled by default
    return 0 != GETENV("pf_stats_gc_only");
  }
}


#include "private/hintgc_config.h"
#ifdef BUILD_WITH_PAPI
#include <papi.h>

// On my laptop, can't include L2, L3 due to permission issues??
extern STATIC int papi_events[];
extern STATIC const int papi_event_size;
extern STATIC long long papi_values[30];
#endif //build with papi

/// 
GC_INNER GC_bool HINTGC_record_stats() {

  if( !enable_pending_free_stats() ) return;
  
  int fd = open(stat_filename(), O_CREAT|O_WRONLY|O_APPEND, 0666);
  if (fd < 0) {
    GC_err_printf("Failed to open %s as log file\n", stat_filename());
    GC_err_printf("  error: %s\n", strerror(errno));
    ABORT("failed to open file");
  }

  GC_printf("-----------------------------------\n");
  static int num = 0;
  num++;
  GC_fd_printf(fd, "%d", num);
  GC_printf("GC #%d\n", num);

  //TODO: log this to a running file for purposes of creating heuristics
  //TODO: capture reclaim..
  //for logging only
  unsigned long heap_size = (unsigned long)GC_get_heap_size();
  GC_fd_printf(fd, ", %lu", heap_size);
  GC_printf("heap size: %lu bytes\n", heap_size);
  GC_printf("theoretical best pending free: %lu ms\n",
            heap_size/12000000); //B/ms
  
  print_pf_heuristic_data(fd); //number of pending free blocks per total

#define REPORT_TIME_AROUND(cstr, stmt)                          \
  do {                                                          \
    long long papi_start, papi_end;                             \
    CLOCK_TYPE start_time;                                      \
    CLOCK_TYPE end_time;                                        \
    unsigned long time_diff;                                    \
    unsigned long papi_time_diff;                               \
    int i;                                                      \
    GET_TIME(start_time);                                       \
    papi_start = PAPI_get_real_usec();                          \
    stmt;                                                       \
    papi_end = PAPI_get_real_usec();                            \
    GET_TIME(end_time);                                         \
    time_diff = MS_TIME_DIFF(end_time, start_time);             \
    papi_time_diff = papi_end - papi_start;                     \
    papi_time_diff = papi_time_diff/1000;                       \
    GC_fd_printf(fd, ", %lu", papi_time_diff);                       \
    GC_printf("%s: %u ms/%u ms\n" ,                             \
              cstr, (unsigned) time_diff,                       \
              (unsigned) papi_time_diff);                       \
  } while(0);

  //Note: the bytes_found count is reset to zero by both pending free and
  // full gc.  Might need to tweak this for integration, but this works 
  // for now. (note that this can be negative if the free lists were marked
  // to begin with (bad))

  // Note: to get an accurate timing, we need to do the full collection without
  // reclaiming, then switch.  Otherwise the second one run sees an unfairly
  // reduced heap size.

  //NOTE: start_reclaim ends up doing most of the freeing work here, can't get
  // both timing and amount reclaimed from one run.  FIXME
  GC_fd_printf(fd, ", %d", pf_stat_gc_only());
  if( pf_stat_gc_only() ) {
    REPORT_TIME_AROUND( "full garbage collect", 
                        GC_try_to_collect_inner(GC_never_stop_func); );
    GC_fd_printf(fd, ", %ld", GC_bytes_found);
    GC_reclaim_all(GC_never_stop_func, FALSE);
    GC_printf("Reclaimed: %ld bytes in heap of size %lu bytes (%f%%)\n",
              (long)GC_bytes_found, (unsigned long)GC_heapsize,
              100*(float)GC_bytes_found/(unsigned long)GC_heapsize);
  } else {
    // Note: We want the pending free execution numbers regardless, makes for
    // useful and interesting charts.
    REPORT_TIME_AROUND( "hintgc collect", 
                        HINTGC_collect_inner(GC_never_stop_func); );
    GC_reclaim_all(GC_never_stop_func, FALSE);
    GC_fd_printf(fd, ", %ld", GC_bytes_found);
    GC_printf("Reclaimed: %ld bytes in heap of size %lu bytes (%f%%)\n",
              (long)GC_bytes_found, (unsigned long)GC_heapsize,
              100*(float)GC_bytes_found/(unsigned long)GC_heapsize);
  }
  GC_bool rval;
  REPORT_TIME_AROUND( "followup garbage collect", 
                      rval = GC_try_to_collect_inner(GC_never_stop_func); );
  GC_reclaim_all(GC_never_stop_func, FALSE);
  GC_fd_printf(fd, ", %ld", GC_bytes_found);
  GC_printf("Reclaimed: %ld bytes in heap of size %lu bytes (%f\%%)\n",
                (long)GC_bytes_found, (unsigned long)GC_heapsize,
                100*(float)GC_bytes_found/(unsigned long)GC_heapsize);
#undef REPORT_TIME_AROUND


  GC_fd_printf(fd, "\n");
  if (close(fd) < 0) {
    GC_err_printf("Failed to close %s as log file\n", stat_filename());
    GC_err_printf("  error: %s\n", strerror(errno));
    ABORT("failed to close file");
  }

  return rval;
}

/*
 * Called from a context where GC is NOT in progress.
 * Initiate a garbage collection if appropriate.
 * Choose judiciously
 * between partial, full, and stop-world collections.
 */
STATIC void GC_maybe_gc(void)
{
    static int n_partial_gcs = 0;
    
    GC_ASSERT(I_HOLD_LOCK());
    ASSERT_CANCEL_DISABLED();
    if (GC_should_collect()) {
        if (!GC_incremental) {

#if ENABLE_HINTGC_INTEGRATION == 0
         HINTGC_record_stats();
#else
          // Okay, now we want to clean up any garbage we have been
          // told is probably free.  We only fall through to the full
          // GC if we have to.  Since the hint flags are cleared, this
          // will only run once.
          if( HINTGC_should_collect() ) {
            HINTGC_collect_inner(GC_never_stop_func);
            //don't run the full collection unless we haven't freed enough space.

            //If we still exceed the heap threshold, continue to the full case
            if (!GC_need_full_gc) return;
          }
 
          /* FIXME: If possible, GC_default_stop_func should be used here */
          GC_try_to_collect_inner(GC_never_stop_func);
#endif
          n_partial_gcs = 0;
          return;
        } else {
          GC_ASSERT(FALSE); //invalid HINTGC configuration
#         ifdef PARALLEL_MARK
            if (GC_parallel)
              GC_wait_for_reclaim();
#         endif
          if (GC_need_full_gc || n_partial_gcs >= GC_full_freq) {
            if (GC_print_stats) {
              GC_log_printf(
                  "***>Full mark for collection %lu after %ld allocd bytes\n",
                  (unsigned long)GC_gc_no + 1, (long)GC_bytes_allocd);
            }
            GC_promote_black_lists();
            (void)GC_reclaim_all((GC_stop_func)0, TRUE);
            GC_notify_full_gc();
            GC_clear_marks();
            n_partial_gcs = 0;
            GC_is_full_gc = TRUE;
          } else {
            n_partial_gcs++;
          }
        }
        /* We try to mark with the world stopped.       */
        /* If we run out of time, this turns into       */
        /* incremental marking.                 */
#       ifndef NO_CLOCK
          if (GC_time_limit != GC_TIME_UNLIMITED) { GET_TIME(GC_start_time); }
#       endif
        /* FIXME: If possible, GC_default_stop_func should be   */
        /* used instead of GC_never_stop_func here.             */
        if (GC_stopped_mark(GC_time_limit == GC_TIME_UNLIMITED?
                            GC_never_stop_func : GC_timeout_stop_func)) {
#           ifdef SAVE_CALL_CHAIN
                GC_save_callers(GC_last_stack);
#           endif
            GC_finish_collection(TRUE);
        } else {
            if (!GC_is_full_gc) {
                /* Count this as the first attempt */
                GC_n_attempts++;
            }
        }
    }
}

static void pending_free_check(struct hblk *h, word fn)
{
  hdr * hhdr = HDR(h);
  GC_bool* result = (GC_bool*)(fn);

  *result |= hhdr->hb_flags & HAS_PENDING_FREE;
}

static GC_bool has_pending_frees() {
  return GC_bytes_pending_free != 0;
}
static GC_bool has_pending_frees_scan() {
  GC_bool result = FALSE;
  GC_apply_to_all_blocks( pending_free_check, (word)&result );
  return result;
}

struct heuristic_data {
  unsigned eligible_blocks;
  unsigned with_pending;
};

static void pending_free_heuristic_check(struct hblk *h, word fn)
{
  hdr * hhdr = HDR(h);
  struct heuristic_data* result = (struct heuristic_data*)(fn);

  result->eligible_blocks++;
  if( hhdr->hb_flags & HAS_PENDING_FREE ) {
    result->with_pending++;
  }
}
#if ENABLE_HINTGC_INTEGRATION
static GC_bool HINTGC_should_collect() {
  if (GETENV("GC_DISABLE_PENDING_FREE") != 0) {
    return FALSE;
  }
  if (GETENV("GC_FORCE_PENDING_FREE") != 0) {
    return TRUE;
  }
  if( GC_need_full_gc ) {
    return FALSE;
  }

#if 1
  struct heuristic_data data;
  data.eligible_blocks = 0;
  data.with_pending = 0;
  GC_apply_to_all_blocks( pending_free_heuristic_check, (word)&data );

  GC_bool decision = TRUE;
  if( data.with_pending == 0 ) {
    decision = FALSE; //no pending frees set
  }

  //Heuristic: Are there at least 3 times as many blocks without
  // frees as there are blocks with frees?  This is intended to ensure
  // that we get a useful priming of the mark phase.
  // TODO: Randomly choosen param, need to explore heuristic space
  if( !(data.eligible_blocks > 4*data.with_pending) ) {
    decision = FALSE; //no pending frees set
  }
#if 1
  GC_printf("Heuristic Data: %d of %d -> %s\n", 
            data.with_pending, data.eligible_blocks,
            decision ? "yes" : "no");
#endif
  return decision;

  //would like a way to know the number of live blocks cheaply...
#else
  return has_pending_frees();
#endif
}
#endif

static void print_pf_heuristic_data(int fd) {
  struct heuristic_data data;
  data.eligible_blocks = 0;
  data.with_pending = 0;
  GC_apply_to_all_blocks( pending_free_heuristic_check, (word)&data );

  GC_fd_printf(fd, ", %d, %d, %u", 
               data.with_pending, data.eligible_blocks, GC_bytes_pending_free);
  GC_printf("Heuristic Data: %d of %d (%f%%), %u bytes 'freed'\n", 
            data.with_pending, data.eligible_blocks, 100.0*(float)data.with_pending/data.eligible_blocks, GC_bytes_pending_free);
}


static void clear_pending_free_in_block(struct hblk *h, word fn)
{
  hdr * hhdr = HDR(h);
  hhdr->hb_flags &= ~HAS_PENDING_FREE; 
}

#include "private/gc_pmark.h"


STATIC void GC_mark_from_stack_until_empty();
STATIC void GC_DFS_until_empty();

static int items_in_mark_stack() {
  return GC_mark_stack_top-GC_mark_stack + 1;
}

/// Push any mark objects that appear in a pending free block
/// NOTE: The mark stack may be too small when this is called.  
/// That's handled externally, but this function can't assume it isn't.
/// This is part of Phase3
static void push_marked_objects_in_pending_free_block(struct hblk *h, word fn)
{
  hdr * hhdr = HDR(h);

  // handled by the apply wrapper function
  GC_ASSERT( !HBLK_IS_FREE(hhdr) );
  GC_ASSERT( !IS_FORWARDING_ADDR_OR_NIL(hhdr) );

  // Only push objects from blocks which are pending frees
  if( !(hhdr->hb_flags & HAS_PENDING_FREE) ) {
    return;
  }
  GC_ASSERT( hhdr -> hb_obj_kind != UNCOLLECTABLE );

  // Optimization: If there's no pointers in the block, 
  // there is nothing to mark.   Enabling this shaves off about 3% 
#if 0 // NOT LEGAL: Freelists (which are pointers) can be threaded through PTRFREE objects (EVIL!)
  if( hhdr->hb_obj_kind == PTRFREE /* aka atomic */ ) {
    return;
  }
#endif

  //TODO: As an optimization, could clear the pending free flags here
  // this would speed up the collection (by not inspecting edges), but might
  // interfer with instrumentation.  Also might prevent a rescan if needed to
  // recover from a mark_stack_too_small.  (Due to last, can't).

  // Use the well optimized version provided by the standard collector
  // Note: This might overflow the stack and force a rescan.  In practice,
  // it doesn't, but it is handled by the wrapping code regardless.
  GC_push_marked(h, hhdr);

  // This last step isn't neccessary, but we'd like to avoid the
  // rescan if we can.  As a result, try to prevent the mark stack from
  // overflowing.  We don't want to do this on every page scan though, or
  // we loose some nice locality.
  // NOTE: This case prevents the need for the overflow logic on the large
  // mellinnium heap runs.
  // NOTE: Using a BFS step would NOT be correct here, unless we always
  // retried.  That would be bad performance.
  if( GC_mark_stack_top > GC_mark_stack + (GC_mark_stack_size/2) ) {
    GC_DFS_until_empty(); //may overflow, that's okay (ensures progress)
  }
}


//in mark.c
GC_INNER void alloc_mark_stack(size_t);

//This is defined in mark.c.  It's basically a flag for whether
// the heap is partially marked already.
extern STATIC GC_bool GC_objects_are_marked;
static GC_bool preames_print_stats = FALSE;

// This does some sanity checking - in at least one case
// the proper roots did NOT get pushed.
GC_INNER void push_roots_with_check() {
  //is the stack within the expected range?
  int sanity_check;
  
  GC_clear_a_few_frames();
  GC_noop(0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0);

  //the stack guess is sane right?
  GC_ASSERT(GC_approx_sp() HOTTER_THAN (ptr_t)&sanity_check);

  const int items_before = items_in_mark_stack();
  GC_push_roots(TRUE, GC_approx_sp());
  GC_ASSERT( !GC_mark_stack_too_small );
  const int roots_pushed = items_in_mark_stack() - items_before;
  //GC_printf("# roots sets pushed: %d\n",  roots_pushed);

  //GC_DFS_until_empty();

  // Mark from the mark stack to ensure each of the items pointed to by
  // a root are marked.  Also okay to just leave this one they're on
  // the queue since we'll come back to this later
  //  GC_mark_from_stack_until_empty();
  //GC_ASSERT( GC_mark_stack_top == GC_mark_stack-1 );
}

GC_INNER void safe_print_counters(const char* name) {
#ifdef BUILD_WITH_PAPI
  // Ok - This check for initialized is an evil, evil hack.  The issue is that
  // the PAPI initialization code (called from inside GC_init) calls malloc.
  // This doesn't recurse into GC_init, but does hit this code path.  The 
  // read counters call checks to see if PAPI has been inited and runs the 
  // init code.  If we don't die from an recursive mess, the original code
  // in GC_init dies with an assertion about an internal init function being
  // called twice.  This is the best work around to avoid the entire mess
  // I found, but it's undeniably a hack and may break at some point in the
  // future.
  if( PAPI_is_initialized() ) {
    int status = PAPI_read_counters(papi_values, papi_event_size);
    GC_ASSERT( PAPI_OK == status );
    int i = 0;
    GC_printf("%s\n", name);
    for(i = 0; i < papi_event_size; i++) {                  
      GC_printf("--%lld\n", papi_values[i]);                  
    }
  }  
#endif
}
GC_INNER void safe_clear_counters() {
#ifdef BUILD_WITH_PAPI
  if( PAPI_is_initialized() ) {
    int status = PAPI_read_counters(papi_values, papi_event_size);
    GC_ASSERT( PAPI_OK == status );
  }  
#endif
}

// In mark.c, runs a bunch of barriers between threads to measure
// the overhead.
STATIC void do_sync_time_experiment();


STATIC void hintgc_do_phase1_serial();
STATIC void hintgc_do_phase2_serial();

STATIC void hintgc_do_phase1_parallel();
STATIC void hintgc_do_phase2_parallel();

extern STATIC GC_bool g_reclaim_hinted;

/// The inner function for performing a stop-the-world hinted collection.
/// Depending on the build mode, this will either be parallel or serial
/// Note: The term "pending free" is used throughout rather than "hint".
/// they're exactly the same thing.  
/// Note: High level description of the algorithm can be found in the
/// ISMM 2013 paper.  
/// Assumes the following:
///  - cancel disabled
///  - dont_gc already checked (i.e. this is unconditional)
///  - any incremental/concurrent GC completed.
GC_INNER GC_bool HINTGC_collect_inner(GC_stop_func stop_func)
{
#define PARAM_ENABLE_PROFITABILITY_CHECK 1
#if PARAM_ENABLE_PROFITABILITY_CHECK
  //This might seem slightly trivial, but this case occurs regularly
  // in the spec benchmark suite.  By skipping the collection
  // entirely (when we know we can reclaim _nothing_), we reduce the
  // average of bzip (for example) sharply.
  if( !has_pending_frees() ) {
    return TRUE;
  }
#endif

  //TODO: as a related matter, implementing either the exactness
  // check, or the clear hint flag on full marking optimization would
  // also have attacked this common case nicely as well.

  safe_clear_counters();

  //check the assumptions
  ASSERT_CANCEL_DISABLED();
  if (GC_dont_gc || (*stop_func)()) return FALSE;
  GC_ASSERT( !GC_dont_gc );
  GC_ASSERT( !(GC_incremental && GC_collection_in_progress()) );

#   ifndef SMALL_CONFIG
  CLOCK_TYPE start_time = 0; /* initialized to prevent warning. */
  CLOCK_TYPE current_time;
#   endif



  // Design Choice - at what granularity should potentially dead data be
  // recorded?
  // - block level - cheap to record, but potentially expensive to check
  // - granula - a possible middle ground
  // - per object - either requires flag bits per object or a free list
  //   the first is a space hog, the second a time hit in this critical code.
  // Note: A (potentially better) design is described in the ISMM paper.  
  // If you're going to reimplement this, please go read that.

  /* 
     An alternate algorithm (never implemented) would be to run many
     passes over the heapmarking one step at a time. Given the assumptions
     of the hinted collection, you'd expect to make only one pass.  
     Unfortunately, this would make inaccurate frees extremely expensive.  
     Probably not a good idea.
  */

    // NOTE: A pending free GC is NOT a full GC.  While the full GC
    // was intended for incremental GC, it's also helpful for us in
    // deciding whether to additionally run a full GC or not. TODO
    //GC_notify_full_gc();
#   ifndef SMALL_CONFIG
    if (preames_print_stats) {
        GET_TIME(start_time);
      }
#   endif
    GC_promote_black_lists();
    /* Make sure all blocks have been reclaimed, so sweep routines      */
    /* don't see cleared mark bits.                                     */
    /* If we're guaranteed to finish, then this is unnecessary.         */
    /* In the find_leak case, we have to finish to guarantee that       */
    /* previously unmarked objects are not reported as leaks.           */
#       ifdef PARALLEL_MARK
          if (GC_parallel)
            GC_wait_for_reclaim();
#       endif
        if ((GC_find_leak || stop_func != GC_never_stop_func)
            && !GC_reclaim_all(stop_func, FALSE)) {
            /* Aborted.  So far everything is still consistent. */
            return(FALSE);
        }
    GC_invalidate_mark_state();  /* Flush mark stack.   */
    GC_clear_marks();
#   ifdef SAVE_CALL_CHAIN
    GC_save_callers(GC_last_stack);
#   endif
    //See comment above
    //GC_is_full_gc = TRUE;

    // All code for hinted collection moved inside 
    // GC_hintgc_stopped_mark

    /*scope*/  { 
      long long papi_start, papi_end;                               
      unsigned long papi_time_diff;                                 
      papi_start = PAPI_get_real_usec();
      
      if (!GC_hintgc_stopped_mark(stop_func)) {
        if (!GC_incremental) {
          /* We're partially done and have no way to complete or use      */
          /* current work.  Reestablish invariants as cheaply as          */
          /* possible.                                                    */
          GC_invalidate_mark_state();
          GC_unpromote_black_lists();
          
          GC_printf("stopped mark failed!\n");
          GC_apply_to_all_blocks( clear_pending_free_in_block, 0 );
          
          GC_ASSERT( !has_pending_frees_scan() );
        } /* else we claim the world is already still consistent.  We'll  */
        /* finish incrementally.                                        */
        return(FALSE);
      }
      
      papi_end = PAPI_get_real_usec();                              
      papi_time_diff = papi_end - papi_start;                       
      papi_time_diff = papi_time_diff/1000;  
      
      //right now, about 13% (unparallized)
      // where's the other 1/3rd?
      
      GC_printf("HintGC Stopped PAPI Time %lu ms\n",
                    papi_time_diff);
    }
    

    g_reclaim_hinted = TRUE;
    GC_finish_collection(FALSE);
    g_reclaim_hinted = FALSE;
#   ifndef SMALL_CONFIG
      if (preames_print_stats) {
        GET_TIME(current_time);
        unsigned long pause = MS_TIME_DIFF(current_time,start_time);
        GC_log_printf("Pending Free collection took %lu msecs\n",
                      MS_TIME_DIFF(current_time,start_time));
        static unsigned long max_pause = 0;
        if( pause > max_pause ) {
          max_pause = pause;
          
        }
        GC_log_printf(" longest pause to date: %lu\n", max_pause);
        //        if( MS_TIME_DIFF(current_time,start_time) )
      }
#   endif

    // Make sure the pending free flags don't persist since
    // they were either wrong, or we already cleared out the relavant parts
    // Note: An explicit call is not needed - this is done inside
    // GC_start_reclaim since it has to visit each anyways
    //GC_apply_to_all_blocks( clear_pending_free_in_block, 0 );

    GC_ASSERT( !has_pending_frees_scan() );

    safe_print_counters(" hinted end");
    return(TRUE);
}
GC_API void GC_CALL GC_force_deallocate(void* p);

#define ENABLE_SANITY_CHECK 1

GC_API void HINTGC_collect() {
    IF_CANCEL(int cancel_state;)
    DCL_LOCK_STATE;

    if (GC_dont_gc) return;

    /*scope*/ {
      //After flushing the roots, this had damn well better be marked!
      //IMPORTANT: only valid while the mark bits are still set
      // NOTE: The sanity check is only valid when not doing parallel
      // marking.  Otherwise, we deadlock here due to re-entry.
#if ENABLE_SANITY_CHECK
      // MUST be outside the lock region
      void* sanity_check = GC_malloc(2);
#endif

      // Can be called from multiple threads at once, must force 
      // a STW event
      LOCK();
      DISABLE_CANCEL(cancel_state);
      ENTER_GC();
      /* Minimize junk left in my registers */
      GC_noop(0,0,0,0,0,0);
#if 1 
      HINTGC_record_stats();
#endif
      HINTGC_collect_inner(GC_never_stop_func);
      
      EXIT_GC();
#   ifdef USE_MUNMAP
      GC_unmap_threshold = old_unmap_threshold; /* restore */
#   endif
      RESTORE_CANCEL(cancel_state);
      UNLOCK(); 
      
#if ENABLE_SANITY_CHECK
      ptr_t head = sanity_check;
      FIXUP_POINTER(head);
      GC_ASSERT( GC_is_marked(head) ); //sanity check
      GC_force_deallocate(sanity_check);
#endif
    }
}
/*
 * Stop the world garbage collection.  Assumes lock held. If stop_func is
 * not GC_never_stop_func then abort if stop_func returns TRUE.
 * Return TRUE if we successfully completed the collection.
 */
GC_INNER GC_bool GC_try_to_collect_inner(GC_stop_func stop_func)
{
  safe_clear_counters();

#   ifndef SMALL_CONFIG
      CLOCK_TYPE start_time = 0; /* initialized to prevent warning. */
      CLOCK_TYPE current_time;
#   endif
    ASSERT_CANCEL_DISABLED();
    if (GC_dont_gc || (*stop_func)()) return FALSE;
    if (GC_incremental && GC_collection_in_progress()) {
      if (preames_print_stats) {
        GC_log_printf(
            "GC_try_to_collect_inner: finishing collection in progress\n");
      }
      /* Just finish collection already in progress.    */
        while(GC_collection_in_progress()) {
            if ((*stop_func)()) return(FALSE);
            GC_collect_a_little_inner(1);
        }
    }

    GC_notify_full_gc();
#   ifndef SMALL_CONFIG
    if (preames_print_stats) {
        GET_TIME(start_time);
        //GC_log_printf("Initiating full world-stop collection!\n");
      }
#   endif
    GC_promote_black_lists();
    /* Make sure all blocks have been reclaimed, so sweep routines      */
    /* don't see cleared mark bits.                                     */
    /* If we're guaranteed to finish, then this is unnecessary.         */
    /* In the find_leak case, we have to finish to guarantee that       */
    /* previously unmarked objects are not reported as leaks.           */
#       ifdef PARALLEL_MARK
          if (GC_parallel)
            GC_wait_for_reclaim();
#       endif
        if ((GC_find_leak || stop_func != GC_never_stop_func)
            && !GC_reclaim_all(stop_func, FALSE)) {
            /* Aborted.  So far everything is still consistent. */
            return(FALSE);
        }
    GC_invalidate_mark_state();  /* Flush mark stack.   */
    GC_clear_marks();
#   ifdef SAVE_CALL_CHAIN
        GC_save_callers(GC_last_stack);
#   endif
    GC_is_full_gc = TRUE;
    /*scope*/  { 
      long long papi_start, papi_end;                               
      unsigned long papi_time_diff;                                 
      papi_start = PAPI_get_real_usec();
      
      if (!GC_stopped_mark(stop_func)) {
        if (!GC_incremental) {
          /* We're partially done and have no way to complete or use      */
          /* current work.  Reestablish invariants as cheaply as          */
          /* possible.                                                    */
          GC_invalidate_mark_state();
          GC_unpromote_black_lists();
        } /* else we claim the world is already still consistent.  We'll  */
        /* finish incrementally.                                        */
        return(FALSE);
      }
      
      papi_end = PAPI_get_real_usec();                              
      papi_time_diff = papi_end - papi_start;                       
      papi_time_diff = papi_time_diff/1000;  
      
      //right now, about 13% (unparallized)
      // where's the other 1/3rd?
      
      GC_printf("Standard Stopped PAPI Time %lu ms\n",
                    papi_time_diff);
    }
    
    GC_finish_collection(TRUE);
#   ifndef SMALL_CONFIG
      if (preames_print_stats) {
        GET_TIME(current_time);
        //        if( MS_TIME_DIFF(current_time,start_time) )
        unsigned long pause = MS_TIME_DIFF(current_time,start_time);
        GC_log_printf("Complete collection took %lu msecs\n",
                      pause);
        static unsigned long max_pause = 0;
        if( pause > max_pause ) {
          max_pause = pause;
        }
        GC_log_printf(" longest pause to date: %lu\n", max_pause);

      }
#   endif


    // remove the tracking for future collections
    // Note: An explicit call is not needed - this is done inside
    // GC_start_reclaim since it has to visit each anyways
    
    GC_apply_to_all_blocks( clear_pending_free_in_block, 0 );

    safe_print_counters("gc end");
    

    return(TRUE);
}

/*
 * Perform n units of garbage collection work.  A unit is intended to touch
 * roughly GC_RATE pages.  Every once in a while, we do more than that.
 * This needs to be a fairly large number with our current incremental
 * GC strategy, since otherwise we allocate too much during GC, and the
 * cleanup gets expensive.
 */
#ifndef GC_RATE
# define GC_RATE 10
#endif
#ifndef MAX_PRIOR_ATTEMPTS
# define MAX_PRIOR_ATTEMPTS 1
#endif
        /* Maximum number of prior attempts at world stop marking       */
        /* A value of 1 means that we finish the second time, no matter */
        /* how long it takes.  Doesn't count the initial root scan      */
        /* for a full GC.                                               */

STATIC int GC_deficit = 0;/* The number of extra calls to GC_mark_some  */
                          /* that we have made.                         */

/// The job of this routine is to do some form of GC and free up memory
/// If not incremental, this may be an entire GC.
GC_INNER void GC_collect_a_little_inner(int n)
{

    int i;
    IF_CANCEL(int cancel_state;)

    if (GC_dont_gc) return;
    DISABLE_CANCEL(cancel_state);
    if (GC_incremental && GC_collection_in_progress()) {
        for (i = GC_deficit; i < GC_RATE*n; i++) {
            if (GC_mark_some((ptr_t)0)) {
                /* Need to finish a collection */
#               ifdef SAVE_CALL_CHAIN
                    GC_save_callers(GC_last_stack);
#               endif
#               ifdef PARALLEL_MARK
                    if (GC_parallel)
                      GC_wait_for_reclaim();
#               endif
                if (GC_n_attempts < MAX_PRIOR_ATTEMPTS
                    && GC_time_limit != GC_TIME_UNLIMITED) {
#                 ifndef NO_CLOCK
                    GET_TIME(GC_start_time);
#                 endif
                  if (!GC_stopped_mark(GC_timeout_stop_func)) {
                    GC_n_attempts++;
                    break;
                  }
                } else {
                  /* FIXME: If possible, GC_default_stop_func should be */
                  /* used here.                                         */
                  (void)GC_stopped_mark(GC_never_stop_func);
                }
                GC_finish_collection(TRUE);
                break;
            }
        }
        if (GC_deficit > 0) GC_deficit -= GC_RATE*n;
        if (GC_deficit < 0) GC_deficit = 0;
    } else {
        GC_maybe_gc();
    }
    RESTORE_CANCEL(cancel_state);
}

GC_INNER void (*GC_check_heap)(void) = 0;
GC_INNER void (*GC_print_all_smashed)(void) = 0;

GC_API int GC_CALL GC_collect_a_little(void)
{
    int result;
    DCL_LOCK_STATE;

    LOCK();
    GC_collect_a_little_inner(1);
    result = (int)GC_collection_in_progress();
    UNLOCK();
    if (!result && GC_debugging_started) GC_print_all_smashed();
    return(result);
}

#ifndef SMALL_CONFIG
  /* Variables for world-stop average delay time statistic computation. */
  /* "divisor" is incremented every world-stop and halved when reached  */
  /* its maximum (or upon "total_time" oveflow).                        */
  static unsigned world_stopped_total_time = 0;
  static unsigned world_stopped_total_divisor = 0;
# ifndef MAX_TOTAL_TIME_DIVISOR
    /* We shall not use big values here (so "outdated" delay time       */
    /* values would have less impact on "average" delay time value than */
    /* newer ones).                                                     */
#   define MAX_TOTAL_TIME_DIVISOR 1000
# endif
#endif

/*
 * Assumes lock is held.  We stop the world and mark from all roots.
 * If stop_func() ever returns TRUE, we may fail and return FALSE.
 * Increment GC_gc_no if we succeed.
 */
STATIC GC_bool GC_stopped_mark(GC_stop_func stop_func)
{
    unsigned i;
#   ifndef SMALL_CONFIG
      CLOCK_TYPE start_time = 0; /* initialized to prevent warning. */
      CLOCK_TYPE current_time;
#   endif

#   if !defined(REDIRECT_MALLOC) && (defined(MSWIN32) || defined(MSWINCE))
        GC_add_current_malloc_heap();
#   endif
#   if defined(REGISTER_LIBRARIES_EARLY)
        GC_cond_register_dynamic_libraries();
#   endif

#   ifndef SMALL_CONFIG
      if (GC_print_stats)
        GET_TIME(start_time);
#   endif

    STOP_WORLD();
#   ifdef THREAD_LOCAL_ALLOC
      GC_world_stopped = TRUE;
#   endif
    if (GC_print_stats) {
        /* Output blank line for convenience here */
        GC_log_printf(
              "\n--> Marking for collection %lu after %lu allocated bytes\n",
              (unsigned long)GC_gc_no + 1, (unsigned long) GC_bytes_allocd);
    }
#   ifdef MAKE_BACK_GRAPH
      if (GC_print_back_height) {
        GC_build_back_graph();
      }
#   endif

    /* Mark from all roots.  */
        /* Minimize junk left in my registers and on the stack */
            GC_clear_a_few_frames();
            GC_noop(0,0,0,0,0,0);
        GC_initiate_gc();
        for (i = 0;;i++) {
          if ((*stop_func)()) {
            if (GC_print_stats) {
              GC_log_printf("Abandoned stopped marking after %u iterations\n",
                            i);
            }
            GC_deficit = i;     /* Give the mutator a chance.   */
#           ifdef THREAD_LOCAL_ALLOC
              GC_world_stopped = FALSE;
#           endif
            START_WORLD();
            return(FALSE);
          }
          
          if (GC_mark_some(GC_approx_sp())) break;
        }

    GC_gc_no++;
    if (GC_print_stats) {
      GC_log_printf(
             "Collection %lu reclaimed %ld bytes ---> heapsize = %lu bytes\n",
             (unsigned long)(GC_gc_no - 1), (long)GC_bytes_found,
             (unsigned long)GC_heapsize);
    }

    /* Check all debugged objects for consistency */
        if (GC_debugging_started) {
            (*GC_check_heap)();
        }

#   ifdef THREAD_LOCAL_ALLOC
      GC_world_stopped = FALSE;
#   endif
    START_WORLD();
#   ifndef SMALL_CONFIG
      if (GC_print_stats) {
        unsigned long time_diff;
        unsigned total_time, divisor;
        GET_TIME(current_time);
        time_diff = MS_TIME_DIFF(current_time,start_time);

        /* Compute new world-stop delay total time */
        total_time = world_stopped_total_time;
        divisor = world_stopped_total_divisor;
        if ((int)total_time < 0 || divisor >= MAX_TOTAL_TIME_DIVISOR) {
          /* Halve values if overflow occurs */
          total_time >>= 1;
          divisor >>= 1;
        }
        total_time += time_diff < (((unsigned)-1) >> 1) ?
                        (unsigned)time_diff : ((unsigned)-1) >> 1;
        /* Update old world_stopped_total_time and its divisor */
        world_stopped_total_time = total_time;
        world_stopped_total_divisor = ++divisor;

        GC_ASSERT(divisor != 0);
        GC_log_printf(
                "World-stopped marking took %lu msecs (%u in average)\n",
                time_diff, total_time / divisor);
      }
#   endif
    return(TRUE);
}

/** This function is the hinted collection version of GC_stopped_mark.  It
    is responsible for stopping the world and ensuring the full hinted 
    collection algorithm runs to completion. 
    Note: Most of the code is copied unchanged.  Only the section
    around mark_some is modified.
*/
STATIC GC_bool GC_hintgc_stopped_mark(GC_stop_func stop_func)
{
    unsigned i;
#   ifndef SMALL_CONFIG
      CLOCK_TYPE start_time = 0; /* initialized to prevent warning. */
      CLOCK_TYPE current_time;
#   endif

#   if !defined(REDIRECT_MALLOC) && (defined(MSWIN32) || defined(MSWINCE))
        GC_add_current_malloc_heap();
#   endif
#   if defined(REGISTER_LIBRARIES_EARLY)
        GC_cond_register_dynamic_libraries();
#   endif

#   ifndef SMALL_CONFIG
      if (GC_print_stats)
        GET_TIME(start_time);
#   endif

    STOP_WORLD();
#   ifdef THREAD_LOCAL_ALLOC
      GC_world_stopped = TRUE;
#   endif
    if (GC_print_stats) {
        /* Output blank line for convenience here */
        GC_log_printf(
              "\n--> Marking for collection %lu after %lu allocated bytes\n",
              (unsigned long)GC_gc_no + 1, (unsigned long) GC_bytes_allocd);
    }
#   ifdef MAKE_BACK_GRAPH
      if (GC_print_back_height) {
        GC_build_back_graph();
      }
#   endif

      /* Mark from all roots.  */
      /* Minimize junk left in my registers and on the stack */
      GC_clear_a_few_frames();
      GC_noop(0,0,0,0,0,0);
      GC_initiate_gc();

      //BEGIN HINTGC EDITS ----------------------------------------
      /* Pseudo code for this algorithm is described in ISMM 2013.  You 
         really want to go read that first. */


      const GC_bool phase_stats = FALSE;

      // Note: An alternate design would be to mark _only_ the items in
      // the pending free blocks (i.e. skip the mark of all non-pending
      // free items) and modify the reclaim code to account for this.
      // From a quick limit test, this doesn't sound like a particularly
      // good idea - since the mass mark is really cheap - , but it 
      // might be worth considering further.


    /* scope */ {

      CLOCK_TYPE linear_start_time = 0; /* initialized to prevent warning. */
      CLOCK_TYPE linear_current_time;
      GET_TIME(linear_start_time);

      // sanity check - empty mark stack
      GC_ASSERT( GC_mark_stack_top == GC_mark_stack-1 );
      GC_ASSERT( !GC_mark_stack_too_small );
      GC_ASSERT( !GC_objects_are_marked);

      GC_ASSERT( !GC_mark_stack_too_small );

      // This is a test routine to figure out how long a noop
      // BSP step takes in the parallel version.  Uncomment to
      // use.
      //do_sync_time_experiment();

      // Start by marking any object reachable from a block
      // without a pending free.  This should get most everything
      // in the heap marked.  The expectation is that few blocks
      // have pending frees.
      if( phase_stats ) safe_print_counters("before phase 1");

      int use_parallel_impl = FALSE;
#ifdef PARALLEL_MARK
      //don't trigger in the single threaded "parallel" case
      use_parallel_impl = GC_parallel;
#endif

      if( !use_parallel_impl ) {
        // In the ISMM 2013 version, this is the code path that
        // was used to collect numbers.  (Without all the parallel
        // code additions obviously and without a fair amount of
        // refactoring internally.)   This code path has only
        // been lightly tested recently.  As a result, it's 
        // much more likely there was a bug introduced than in the
        // parallel code path.
        //Phase 1
        hintgc_do_phase1_serial();
        if( phase_stats ) safe_print_counters("phase 1");
        //Phase 2
        hintgc_do_phase2_serial();
        if( phase_stats ) safe_print_counters("phase 2 - scan");
      } else {
#ifndef PARALLEL_MARK
        GC_ASSERT(FALSE); //Illegal configuration!
#else //HINTGC_PARALLEL
        //Phase 1
        /*scope*/  { 
          long long papi_start, papi_end;                               
          unsigned long papi_time_diff;                                 
          papi_start = PAPI_get_real_usec();
          
          hintgc_do_phase1_parallel();
          
          papi_end = PAPI_get_real_usec();                              
          papi_time_diff = papi_end - papi_start;                       
          papi_time_diff = papi_time_diff/1000;  
          
          //right now, about 13% (unparallized)
          // where's the other 1/3rd?
          
          GC_printf("Phase 1 PAPI Time %lu ms\n",
                    papi_time_diff);
        }
        if( phase_stats ) safe_print_counters("phase 1");
        //Phase 2
        hintgc_do_phase2_parallel();
        if( phase_stats ) safe_print_counters("phase 2 - scan");
#endif
      }


      GC_ASSERT( !GC_mark_stack_too_small );
      // Flush the mark stack since the last batch may not have finished
      // NOTE: This one perf critical for the serial code path.  Why?
      // Using a DFS here would be slow since it's not adapted to
      // only work within the pending free blocks.  However, we're
      // also throwing away a lot of work by not pushing this content..
      GC_mark_from_stack_until_empty();
      if( phase_stats ) safe_print_counters("phase 2 - finish");

      //At this point, all the objects in blocks without pending frees
      // have been marked along with everything that they've pointed to.

      //Phase 3 ------------------------------------------------------

      // Next, make sure we mark all the roots.  Note that this exposes
      // a bug when run under a do_blocking, with_gc_active.  Given that
      // no benchmark I care uses these routines, I simply disabled that
      // portion of the tests.

      // Since we've already marked all objects not in pending free blocks,
      // we're essentially doing a filter add here.  (The existing code won't
      // push anything that's already been marked.  Completely disabling this as
      // a perf limit test - while being functionally wrong - makes less than 
      // 1/2% difference; it's in the noise.
      push_roots_with_check();
      //GC_printf("# roots pushed: %d\n", items_in_mark_stack() );

      //The following is a hacky way of getting the Boehm collector's mark
      // routine to avoid restarting the entire scan.
      //      GC_objects_are_marked = FALSE;  (Is this still needed? Yes
      //      see below.  I didn't move this in case there's a reason I
      //      don't remember to have them seperated.)
      GC_mark_state = MS_ROOTS_PUSHED;
      GC_ASSERT( !GC_mark_stack_too_small );

      if( phase_stats ) safe_print_counters("roots");
      
      /* At this point, the following invariants should hold:
         1) Every object outside a pending free block is marked.
         2) Every object in a pending free block directly reachable from
            an object outside a pending free block is marked (or pushed).
         3) Every object in a pending free block directly reachable from 
            a root is marked (or pushed.)
      */

      // Now we need to seed a standard DFS traversal starting with
      // a combination of the normal roots (since some may point to
      // objects in the pending free blocks) and any marked objects in the
      // pending free blocks.  The resulting DFS is responsible for marking
      // any objects in the pending free blocks which are reachable.

      // TODO: This might allow work to fall through to the general
      // collector handling - which doesn't have the fast overflow
      // handling!  Make sure the marking is completely done.
      // This would be a performance improvement, but is functional
      // correct as is.  This might help for example libquantum though.

      // TODO: Currently, the overflow handling is inherently serial
      // we take no advantage of the parallel execution.  It would be
      // worthwhile to integrate this with the GC_mark_some loop below
      // to get parallel marking during each overflow.  The fact we 
      // currently need both sets of code is unfortunate.   
      // NOTE: We would also need to integrate uncollectable handling here
      // if we were do redo this.

      /*scope*/  { 
        long long papi_start, papi_end;                               
        unsigned long papi_time_diff;                                 
        papi_start = PAPI_get_real_usec();
        
        GC_apply_to_all_blocks( push_marked_objects_in_pending_free_block, 0);
        while( GC_mark_stack_too_small ) {
          // Overflow handling -- The good news is we only need to 
          // rescan the pending free blocks and nothing else.
          // Note: This case is not optional.  It gets hit for some 
          // of the larger heap sizes (microbenchmarks) on the millinnium
          // cluster runs and one of the clang compile jobs (during
          // clang self host)
          // Steps: 
          // 1) do any useful work we can (ensures progress)
          // 2) increase the mark stack size
          // 3) try again
          GC_DFS_until_empty();
          alloc_mark_stack(2*GC_mark_stack_size);
          GC_apply_to_all_blocks( push_marked_objects_in_pending_free_block, 0);
        }
        GC_ASSERT( !GC_mark_stack_too_small );
      
        
        papi_end = PAPI_get_real_usec();                              
        papi_time_diff = papi_end - papi_start;                       
        papi_time_diff = papi_time_diff/1000;  
        
        GC_printf("Phase 3 PAPI Time %lu ms\n",
                      papi_time_diff);
      }

      if( phase_stats ) safe_print_counters("phase 3");
      
      // This next step is a hack.  It exploits the invariant assumptions
      // used by mark_some to trip us directly into the mark uncollectable
      // step, followed by the push roots, and general collect.  Without
      // it, we get routed into the handling for recovering from a partial
      // mark.  
      // NOTE: This is not sound if we've already overflowed!  We shouldn't
      // have, but that would be nasty case to debug.
      GC_ASSERT( !GC_mark_stack_too_small );
      GC_objects_are_marked = FALSE;
      // See the assignment of GC_mark_state above.

      GET_TIME(linear_current_time);

      if (preames_print_stats) {
        unsigned long pause = MS_TIME_DIFF(linear_current_time,
                                           linear_start_time);
        static unsigned long max_pause = 0;
        if( pause > max_pause ) {
          max_pause = pause;
          
        }
        if(pause) {
          GC_log_printf("Marking for pending free took %lu msecs\n",
                        pause);
          GC_log_printf(" longest pause to date: %lu\n", max_pause);
        }

          
      }
    }

    // Alternate design possibilities
    // 1) There was previously some other code here which tried to walk the
    // raw heap sections.  This might have an advantage w/re locality
    // but the implementation I had was incorrect.  If I want to revisit
    // go back and look at GC_dump_regions in allchblk.c.
    // 2) Rather than scanning unhinted objects in phase 3, we could record
    // crossing edges in phase 2 and only rescan those objects.  I have
    // not explored this possibility.  Might be worth using a hybrid scheme
    // with a small fixed size buffer for crossing edges.  Would need to
    // check emperical data to decide.
    // 3) An alternate design would be to not mark objects outside pending free
    // and only push them.  Then modify the reclaim code to ignore blocks
    // without pending free.  (Compilications with new frees)  Given the
    // time to mark, this shouldn't matter.



      //END HINTGC EDITS ----------------------------------------
      //EXCEPT TIMING

      /*scope*/  { 
        long long papi_start, papi_end;                               
        unsigned long papi_time_diff;                                 
        papi_start = PAPI_get_real_usec();
        
        
        for (i = 0;;i++) {
          if ((*stop_func)()) {
            if (GC_print_stats) {
              GC_log_printf("Abandoned stopped marking after %u iterations\n",
                            i);
            }
            GC_deficit = i;     /* Give the mutator a chance.   */
#           ifdef THREAD_LOCAL_ALLOC
            GC_world_stopped = FALSE;
#           endif
            START_WORLD();
            return(FALSE);
          }
          
          if (GC_mark_some(GC_approx_sp())) break;
        }
        
        
        papi_end = PAPI_get_real_usec();                              
        papi_time_diff = papi_end - papi_start;                       
        papi_time_diff = papi_time_diff/1000;  
        
        GC_printf("Mark Some (fallback) Time %lu ms\n",
                  papi_time_diff);
      }

    GC_gc_no++;
    if (GC_print_stats) {
      GC_log_printf(
             "Collection %lu reclaimed %ld bytes ---> heapsize = %lu bytes\n",
             (unsigned long)(GC_gc_no - 1), (long)GC_bytes_found,
             (unsigned long)GC_heapsize);
    }

    /* Check all debugged objects for consistency */
        if (GC_debugging_started) {
            (*GC_check_heap)();
        }

#   ifdef THREAD_LOCAL_ALLOC
      GC_world_stopped = FALSE;
#   endif
    START_WORLD();
#   ifndef SMALL_CONFIG
      if (GC_print_stats) {
        unsigned long time_diff;
        unsigned total_time, divisor;
        GET_TIME(current_time);
        time_diff = MS_TIME_DIFF(current_time,start_time);

        /* Compute new world-stop delay total time */
        total_time = world_stopped_total_time;
        divisor = world_stopped_total_divisor;
        if ((int)total_time < 0 || divisor >= MAX_TOTAL_TIME_DIVISOR) {
          /* Halve values if overflow occurs */
          total_time >>= 1;
          divisor >>= 1;
        }
        total_time += time_diff < (((unsigned)-1) >> 1) ?
                        (unsigned)time_diff : ((unsigned)-1) >> 1;
        /* Update old world_stopped_total_time and its divisor */
        world_stopped_total_time = total_time;
        world_stopped_total_divisor = ++divisor;

        GC_ASSERT(divisor != 0);
        GC_log_printf(
                "World-stopped marking took %lu msecs (%u in average)\n",
                time_diff, total_time / divisor);
      }
#   endif
    return(TRUE);
}

/* Set all mark bits for the free list whose first entry is q   */
GC_INNER void GC_set_fl_marks(ptr_t q)
{
   struct hblk *h, *last_h;
   hdr *hhdr;
   IF_PER_OBJ(size_t sz;)
   unsigned bit_no;

   if (q != NULL) {
     h = HBLKPTR(q);
     last_h = h;
     hhdr = HDR(h);
     IF_PER_OBJ(sz = hhdr->hb_sz;)

     for (;;) {
        bit_no = MARK_BIT_NO((ptr_t)q - (ptr_t)h, sz);
        if (!mark_bit_from_hdr(hhdr, bit_no)) {
          set_mark_bit_from_hdr(hhdr, bit_no);
          ++hhdr -> hb_n_marks;
        }

        q = obj_link(q);
        if (q == NULL)
          break;

        h = HBLKPTR(q);
        if (h != last_h) {
          last_h = h;
          hhdr = HDR(h);
          IF_PER_OBJ(sz = hhdr->hb_sz;)
        }
     }
   }
}

#if defined(GC_ASSERTIONS) && defined(THREADS) && defined(THREAD_LOCAL_ALLOC)
  /* Check that all mark bits for the free list whose first entry is    */
  /* (*pfreelist) are set.  Check skipped if points to a special value. */
  void GC_check_fl_marks(void **pfreelist)
  {
#   ifdef AO_HAVE_load_acquire_read
      AO_t *list = (AO_t *)AO_load_acquire_read((AO_t *)pfreelist);
                /* Atomic operations are used because the world is running. */
      AO_t *prev;
      AO_t *p;

      if ((word)list <= HBLKSIZE) return;

      prev = (AO_t *)pfreelist;
      for (p = list; p != NULL;) {
        AO_t *next;

        if (!GC_is_marked((ptr_t)p)) {
          hdr *hhdr;
          hhdr = HDR(HBLKPTR(p));
          GC_err_printf("Unmarked object %p on list %p\n",
                        (void *)p, (void *)list);
          GC_print_type(p);
          GC_print_block_descr_inner( HBLKPTR(p) );
          puts("free list");
          GC_print_free_list( hhdr->hb_obj_kind, 
                              BYTES_TO_GRANULES(hhdr->hb_sz));
          //GC_print_obj();
          //          GC_print_block_list();
          ABORT("Unmarked local free list entry");
        }

        /* While traversing the free-list, it re-reads the pointer to   */
        /* the current node before accepting its next pointer and       */
        /* bails out if the latter has changed.  That way, it won't     */
        /* try to follow the pointer which might be been modified       */
        /* after the object was returned to the client.  It might       */
        /* perform the mark-check on the just allocated object but      */
        /* that should be harmless.                                     */
        next = (AO_t *)AO_load_acquire_read(p);
        if (AO_load(prev) != (AO_t)p)
          break;
        prev = p;
        p = next;
      }
#   else
      /* FIXME: Not implemented (just skipped). */
      (void)pfreelist;
#   endif
  }
#endif /* GC_ASSERTIONS && THREAD_LOCAL_ALLOC */

/* Clear all mark bits for the free list whose first entry is q */
/* Decrement GC_bytes_found by number of bytes on free list.    */
STATIC void GC_clear_fl_marks(ptr_t q)
{
   struct hblk *h, *last_h;
   hdr *hhdr;
   size_t sz;
   unsigned bit_no;

   if (q != NULL) {
     h = HBLKPTR(q);
     last_h = h;
     hhdr = HDR(h);
     sz = hhdr->hb_sz;  /* Normally set only once. */

     for (;;) {
        bit_no = MARK_BIT_NO((ptr_t)q - (ptr_t)h, sz);
        if (mark_bit_from_hdr(hhdr, bit_no)) {
          size_t n_marks = hhdr -> hb_n_marks - 1;
          clear_mark_bit_from_hdr(hhdr, bit_no);
#         ifdef PARALLEL_MARK
            /* Appr. count, don't decrement to zero! */
            if (0 != n_marks || !GC_parallel) {
              hhdr -> hb_n_marks = n_marks;
            }
#         else
            hhdr -> hb_n_marks = n_marks;
#         endif
        }
        GC_bytes_found -= sz;

        q = obj_link(q);
        if (q == NULL)
          break;

        h = HBLKPTR(q);
        if (h != last_h) {
          last_h = h;
          hhdr = HDR(h);
          sz = hhdr->hb_sz;
        }
     }
   }
}

#if defined(GC_ASSERTIONS) && defined(THREADS) && defined(THREAD_LOCAL_ALLOC)
  void GC_check_tls(void);
#endif

/* Finish up a collection.  Assumes mark bits are consistent, lock is   */
/* held, but the world is otherwise running.                            */
// Reset_metadata controls whether the data used by the GC_should_collect 
//  heuristic is reset or not.  Useful if this is a partial collection 
//  (aka pending free) which shouldn't prevent a full collection from
//  running if one is still needed.
STATIC void GC_finish_collection(GC_bool reset_metadata)
{
#   ifndef SMALL_CONFIG
      CLOCK_TYPE start_time = 0; /* initialized to prevent warning. */
      CLOCK_TYPE finalize_time = 0;
      CLOCK_TYPE done_time;
#   endif

#   if defined(GC_ASSERTIONS) && defined(THREADS) \
       && defined(THREAD_LOCAL_ALLOC) && !defined(DBG_HDRS_ALL)
        /* Check that we marked some of our own data.           */
        /* FIXME: Add more checks.                              */
      GC_check_tls();
#   endif

#   ifndef SMALL_CONFIG
      if (GC_print_stats)
        GET_TIME(start_time);
#   endif

      //needed for tracking of how much pending free cleans up
      // probably want two variables for integration purposes
    if( TRUE || reset_metadata ) {
      GC_bytes_found = 0;
      GC_bytes_pending_free = 0;
      GC_pending_free_high = 0;
      GC_pending_free_low = ONES;
    }
#   if defined(LINUX) && defined(__ELF__) && !defined(SMALL_CONFIG)
        if (GETENV("GC_PRINT_ADDRESS_MAP") != 0) {
          GC_print_address_map();
        }
#   endif
    COND_DUMP;
    if (GC_find_leak) {
      /* Mark all objects on the free list.  All objects should be      */
      /* marked when we're done.                                        */
      word size;        /* current object size  */
      unsigned kind;
      ptr_t q;

      for (kind = 0; kind < GC_n_kinds; kind++) {
        for (size = 1; size <= MAXOBJGRANULES; size++) {
          q = GC_obj_kinds[kind].ok_freelist[size];
          if (q != 0) GC_set_fl_marks(q);
        }
      }
      GC_start_reclaim(TRUE);
        /* The above just checks; it doesn't really reclaim anything.   */
    }

    GC_finalize();
#   ifdef STUBBORN_ALLOC
      GC_clean_changing_list();
#   endif

#   ifndef SMALL_CONFIG
      if (GC_print_stats)
        GET_TIME(finalize_time);
#   endif

    if (GC_print_back_height) {
#     ifdef MAKE_BACK_GRAPH
        GC_traverse_back_graph();
#     elif !defined(SMALL_CONFIG)
        GC_err_printf("Back height not available: "
                      "Rebuild collector with -DMAKE_BACK_GRAPH\n");
#     endif
    }

    /* Clear free list mark bits, in case they got accidentally marked   */
    /* (or GC_find_leak is set and they were intentionally marked).      */
    /* Also subtract memory remaining from GC_bytes_found count.         */
    /* Note that composite objects on free list are cleared.             */
    /* Thus accidentally marking a free list is not a problem;  only     */
    /* objects on the list itself will be marked, and that's fixed here. */
    {
      word size;        /* current object size          */
      ptr_t q;          /* pointer to current object    */
      unsigned kind;

      for (kind = 0; kind < GC_n_kinds; kind++) {
        for (size = 1; size <= MAXOBJGRANULES; size++) {
          q = GC_obj_kinds[kind].ok_freelist[size];
          if (q != 0) GC_clear_fl_marks(q);
        }
      }
    }

    if (GC_print_stats == VERBOSE)
        GC_log_printf("Bytes recovered before sweep - f.l. count = %ld\n",
                      (long)GC_bytes_found);

    /* Reconstruct free lists to contain everything not marked */
    GC_start_reclaim(FALSE);
    if (GC_print_stats) {
      GC_log_printf("Heap contains %lu pointer-containing "
                    "+ %lu pointer-free reachable bytes\n",
                    (unsigned long)GC_composite_in_use,
                    (unsigned long)GC_atomic_in_use);
    }
    if (GC_is_full_gc) {
        GC_used_heap_size_after_full = USED_HEAP_SIZE;
        GC_need_full_gc = FALSE;
    } else {
        GC_need_full_gc = USED_HEAP_SIZE - GC_used_heap_size_after_full
                            > min_bytes_allocd();
    }

    if (GC_print_stats == VERBOSE) {
#     ifdef USE_MUNMAP
        GC_log_printf("Immediately reclaimed %ld bytes in heap"
                      " of size %lu bytes (%lu unmapped)\n",
                      (long)GC_bytes_found, (unsigned long)GC_heapsize,
                      (unsigned long)GC_unmapped_bytes);
#     else
        GC_log_printf(
                "Immediately reclaimed %ld bytes in heap of size %lu bytes\n",
                (long)GC_bytes_found, (unsigned long)GC_heapsize);
#     endif
    }

    /* Reset or increment counters for next cycle */
    if( reset_metadata ) {
      GC_n_attempts = 0;
      GC_is_full_gc = FALSE;
      GC_bytes_allocd_before_gc += GC_bytes_allocd;
      GC_non_gc_bytes_at_gc = GC_non_gc_bytes;
      GC_bytes_allocd = 0;
      GC_bytes_dropped = 0;
      GC_bytes_freed = 0;
      GC_finalizer_bytes_freed = 0;
    }
#   ifdef USE_MUNMAP
      GC_unmap_old();
#   endif

#   ifndef SMALL_CONFIG
      if (GC_print_stats) {
        GET_TIME(done_time);

        /* A convenient place to output finalization statistics. */
        GC_print_finalization_stats();

        GC_log_printf("Finalize plus initiate sweep took %lu + %lu msecs\n",
                      MS_TIME_DIFF(finalize_time,start_time),
                      MS_TIME_DIFF(done_time,finalize_time));
      }
#   endif
}

/* If stop_func == 0 then GC_default_stop_func is used instead.         */
STATIC GC_bool GC_try_to_collect_general(GC_stop_func stop_func,
                                         GC_bool force_unmap)
{
    GC_bool result;
#   ifdef USE_MUNMAP
      int old_unmap_threshold;
#   endif
    IF_CANCEL(int cancel_state;)
    DCL_LOCK_STATE;

    if (!GC_is_initialized) GC_init();
    if (GC_debugging_started) GC_print_all_smashed();
    GC_INVOKE_FINALIZERS();
    LOCK();
    DISABLE_CANCEL(cancel_state);
#   ifdef USE_MUNMAP
      old_unmap_threshold = GC_unmap_threshold;
      if (force_unmap ||
          (GC_force_unmap_on_gcollect && old_unmap_threshold > 0))
        GC_unmap_threshold = 1; /* unmap as much as possible */
#   endif
    ENTER_GC();
    /* Minimize junk left in my registers */
    GC_noop(0,0,0,0,0,0);
    // This is called from GG_gcollect - want a full stop the world
    // collection, not simply a pending free
    result = GC_try_to_collect_inner(stop_func != 0 ? stop_func :
                                     GC_default_stop_func);
    EXIT_GC();
#   ifdef USE_MUNMAP
      GC_unmap_threshold = old_unmap_threshold; /* restore */
#   endif
    RESTORE_CANCEL(cancel_state);
    UNLOCK();
    if (result) {
        if (GC_debugging_started) GC_print_all_smashed();
        GC_INVOKE_FINALIZERS();
    }
    return(result);
}

/* Externally callable routines to invoke full, stop-the-world collection. */
GC_API int GC_CALL GC_try_to_collect(GC_stop_func stop_func)
{
    GC_ASSERT(stop_func != 0);
    return (int)GC_try_to_collect_general(stop_func, FALSE);
}

GC_API void GC_CALL GC_gcollect(void)
{

    /* 0 is passed as stop_func to get GC_default_stop_func value       */
    /* while holding the allocation lock (to prevent data races).       */
    (void)GC_try_to_collect_general(0, FALSE);
    if (GC_have_errors) GC_print_all_errors();
}

GC_API void GC_CALL GC_gcollect_and_unmap(void)
{
    (void)GC_try_to_collect_general(GC_never_stop_func, TRUE);
}

GC_INNER word GC_n_heap_sects = 0;
                        /* Number of sections currently in heap. */

#ifdef USE_PROC_FOR_LIBRARIES
  GC_INNER word GC_n_memory = 0;
                        /* Number of GET_MEM allocated memory sections. */
#endif

#ifdef USE_PROC_FOR_LIBRARIES
  /* Add HBLKSIZE aligned, GET_MEM-generated block to GC_our_memory. */
  /* Defined to do nothing if USE_PROC_FOR_LIBRARIES not set.       */
  GC_INNER void GC_add_to_our_memory(ptr_t p, size_t bytes)
  {
    if (0 == p) return;
    if (GC_n_memory >= MAX_HEAP_SECTS) {
      GC_printf("%d\n", GC_n_memory);
      ABORT("Too many GC-allocated memory sections: Increase MAX_HEAP_SECTS");
    }
    GC_our_memory[GC_n_memory].hs_start = p;
    GC_our_memory[GC_n_memory].hs_bytes = bytes;
    GC_n_memory++;
  }
#endif

/*
 * Use the chunk of memory starting at p of size bytes as part of the heap.
 * Assumes p is HBLKSIZE aligned, and bytes is a multiple of HBLKSIZE.
 */
GC_INNER void GC_add_to_heap(struct hblk *p, size_t bytes)
{
    hdr * phdr;
    word endp;

    if (GC_n_heap_sects >= MAX_HEAP_SECTS) {
        ABORT("Too many heap sections: Increase MAXHINCR or MAX_HEAP_SECTS");
    }
    while ((word)p <= HBLKSIZE) {
        /* Can't handle memory near address zero. */
        ++p;
        bytes -= HBLKSIZE;
        if (0 == bytes) return;
    }
    endp = (word)p + bytes;
    if (endp <= (word)p) {
        /* Address wrapped. */
        bytes -= HBLKSIZE;
        if (0 == bytes) return;
        endp -= HBLKSIZE;
    }
    //preames - the above looks like it should be a while loop
    // not changing it in case I'm wrong
    GC_ASSERT( !(endp <= (word)p) );

    phdr = GC_install_header(p);
    if (0 == phdr) {
        /* This is extremely unlikely. Can't add it.  This will         */
        /* almost certainly result in a 0 return from the allocator,    */
        /* which is entirely appropriate.                               */
        return;
    }
    GC_ASSERT(endp > (word)p && endp == (word)p + bytes);
    GC_heap_sects[GC_n_heap_sects].hs_start = (ptr_t)p;
    GC_heap_sects[GC_n_heap_sects].hs_bytes = bytes;
    GC_n_heap_sects++;
    phdr -> hb_sz = bytes;
    phdr -> hb_flags = 0;
    GC_freehblk(p);
    GC_heapsize += bytes;
    if ((ptr_t)p <= (ptr_t)GC_least_plausible_heap_addr
        || GC_least_plausible_heap_addr == 0) {
        GC_least_plausible_heap_addr = (void *)((ptr_t)p - sizeof(word));
                /* Making it a little smaller than necessary prevents   */
                /* us from getting a false hit from the variable        */
                /* itself.  There's some unintentional reflection       */
                /* here.                                                */
    }
    //(ptr_t)p + bytes == endp here
    if ((ptr_t)p + bytes >= (ptr_t)GC_greatest_plausible_heap_addr) {
        GC_greatest_plausible_heap_addr = (void *)endp;
    }
    
    GC_ASSERT( p > GC_least_plausible_heap_addr &&
               endp <= GC_greatest_plausible_heap_addr );
}

#if !defined(NO_DEBUGGING)
  void GC_print_heap_sects(void)
  {
    unsigned i;

    GC_printf("Total heap size: %lu\n", (unsigned long)GC_heapsize);
    for (i = 0; i < GC_n_heap_sects; i++) {
      ptr_t start = GC_heap_sects[i].hs_start;
      size_t len = GC_heap_sects[i].hs_bytes;
      struct hblk *h;
      unsigned nbl = 0;

      for (h = (struct hblk *)start; h < (struct hblk *)(start + len); h++) {
        if (GC_is_black_listed(h, HBLKSIZE)) nbl++;
      }
      GC_printf("Section %d from %p to %p %lu/%lu blacklisted\n",
                i, start, start + len,
                (unsigned long)nbl, (unsigned long)(len/HBLKSIZE));
    }
  }
#endif

void * GC_least_plausible_heap_addr = (void *)ONES;
void * GC_greatest_plausible_heap_addr = 0;

GC_API void GC_CALL GC_set_max_heap_size(GC_word n)
{
    GC_max_heapsize = n;
}

GC_word GC_max_retries = 0;

/*
 * this explicitly increases the size of the heap.  It is used
 * internally, but may also be invoked from GC_expand_hp by the user.
 * The argument is in units of HBLKSIZE.
 * Tiny values of n are rounded up.
 * Returns FALSE on failure.
 */
GC_INNER GC_bool GC_expand_hp_inner(word n)
{
    word bytes;
    struct hblk * space;
    word expansion_slop;        /* Number of bytes by which we expect the */
                                /* heap to expand soon.                   */

    if (n < MINHINCR) n = MINHINCR;
    bytes = n * HBLKSIZE;
    /* Make sure bytes is a multiple of GC_page_size */
      {
        word mask = GC_page_size - 1;
        bytes += mask;
        bytes &= ~mask;
      }

    if (GC_max_heapsize != 0 && GC_heapsize + bytes > GC_max_heapsize) {
        /* Exceeded self-imposed limit */
        return(FALSE);
    }
    space = GET_MEM(bytes);
    GC_add_to_our_memory((ptr_t)space, bytes);
    if (space == 0) {
        if (GC_print_stats) {
            GC_log_printf("Failed to expand heap by %ld bytes\n",
                          (unsigned long)bytes);
        }
        return(FALSE);
    }
    if (GC_print_stats) {
      GC_log_printf("Increasing heap size by %lu after %lu allocated bytes\n",
                    (unsigned long)bytes, (unsigned long)GC_bytes_allocd);
    }
    /* Adjust heap limits generously for blacklisting to work better.   */
    /* GC_add_to_heap performs minimal adjustment needed for            */
    /* correctness.                                                     */
    expansion_slop = min_bytes_allocd() + 4*MAXHINCR*HBLKSIZE;
    if ((GC_last_heap_addr == 0 && !((word)space & SIGNB))
        || (GC_last_heap_addr != 0 && GC_last_heap_addr < (ptr_t)space)) {
        /* Assume the heap is growing up */
        word new_limit = (word)space + bytes + expansion_slop;
        if (new_limit > (word)space) {
          GC_greatest_plausible_heap_addr =
            (void *)GC_max((word)GC_greatest_plausible_heap_addr,
                           (word)new_limit);
        }
    } else {
        /* Heap is growing down */
        word new_limit = (word)space - expansion_slop;
        if (new_limit < (word)space) {
          GC_least_plausible_heap_addr =
            (void *)GC_min((word)GC_least_plausible_heap_addr,
                           (word)space - expansion_slop);
        }
    }

    //preames - In the case where the above overflows, no useful
    // update is done, however the invariant is reinstalled inside
    // add_to_heap.  Could do slightly better, but not sure it 
    // matters
    //GC_ASSERT( space > GC_least_plausible_heap_addr &&
    //           space <= GC_greatest_plausible_heap_addr );


    GC_prev_heap_addr = GC_last_heap_addr;
    GC_last_heap_addr = (ptr_t)space;
    GC_add_to_heap(space, bytes);
    /* Force GC before we are likely to allocate past expansion_slop */
      GC_collect_at_heapsize =
         GC_heapsize + expansion_slop - 2*MAXHINCR*HBLKSIZE;
      if (GC_collect_at_heapsize < GC_heapsize /* wrapped */)
         GC_collect_at_heapsize = (word)(-1);
    return(TRUE);
}

/* Really returns a bool, but it's externally visible, so that's clumsy. */
/* Arguments is in bytes.  Includes GC_init() call.                      */
GC_API int GC_CALL GC_expand_hp(size_t bytes)
{
    int result;
    DCL_LOCK_STATE;

    LOCK();
    if (!GC_is_initialized) GC_init();
    result = (int)GC_expand_hp_inner(divHBLKSZ((word)bytes));
    if (result) GC_requested_heapsize += bytes;
    UNLOCK();
    return(result);
}

GC_INNER unsigned GC_fail_count = 0;
                        /* How many consecutive GC/expansion failures?  */
                        /* Reset by GC_allochblk.                       */

/* Collect or expand heap in an attempt make the indicated number of    */
/* free blocks available.  Should be called until the blocks are        */
/* available (seting retry value to TRUE unless this is the first call  */
/* in a loop) or until it fails by returning FALSE.                     */
GC_INNER GC_bool GC_collect_or_expand(word needed_blocks,
                                      GC_bool ignore_off_page,
                                      GC_bool* retry)
{
    GC_bool gc_not_stopped = TRUE;
    word blocks_to_get;
    GC_stop_func stop_func; 
    IF_CANCEL(int cancel_state;)

    DISABLE_CANCEL(cancel_state);
    if (!GC_incremental && !GC_dont_gc &&
        ((GC_dont_expand && GC_bytes_allocd > 0) || 
         GC_should_collect() )) {

#if 1
      //rather than get fancy (and introduce more bugs), just run two GC
      HINTGC_record_stats();
#endif
      /* 
      //OKAY: There's a serious issue here.  If the pending free
      // collector runs, but can't make progress of the right block
      // size for the caller, then the full GC will not be run since
      // should collect will return false.  This triggers massive heap
      // growth.
      One approach might be to only do the pending free sweep if
      blocks with the right size and kind have pending free bits set.
      While this would help, would still need some kind of fall back for
      when that pending free is actually wrong.  Could invoke continue_reclaim
      and test results, but that seems hacky here.  (This might be a useful 
      optimization regardless.)
      
      THIS SEEMS LIKES THE BEST APPROACH. - IMPLEMENTED (partially)
      Another approach would to make the pending free check separate
      from the GC cycle.  (i.e. change finish_collection to not clear flags)
      This has the disadvantage that GC doesn't slow down any after a successful 
      free check.  In turn, could modify the should_collect call to subtract out
      the amount reclaimed from the current collection.  Since the reclaim is on
      the per size, per kind granularity, this would have the desired effect.
      - NOTE: The handling of the reclaim didn't have the expected effect.  Not sure
        entirely why.  That part is disabled currently.

      Note: The large alloc path isn't even trying to reclaim.  NO, the large
      block reclaim is done in reclaim_start.  (Mixing of layers, but okay...)
      */

      /* Try to do a full collection using 'default' stop_func (unless  */
      /* nothing has been allocated since the latest collection or heap */
      /* expansion is disabled).                                        */
      stop_func = 
        GC_bytes_allocd > 0 && (!GC_dont_expand || !*retry) ?
        GC_default_stop_func : GC_never_stop_func;

#if ENABLE_HINTGC_INTEGRATION
      // Okay, now we want to clean up any garbage we have been
      // told is probably free.  We only fall through to the full
      // GC if we have to.  Since the pending free is cleared, this
      // will only run once.
      if( GC_should_collect() && HINTGC_should_collect() ) {
        gc_not_stopped = HINTGC_collect_inner(stop_func);
        GC_ASSERT( !has_pending_frees_scan() );

        //puts("pending free ran");

        // Did we make enough progress not to run a full GC?  Need to
        // be below the space threshold, and either completed, or stopped
        // and about to retry with a different stop function.
        GC_bool made_progress = //!GC_need_full_gc &&
                                (gc_not_stopped == TRUE || !*retry);
        //GC_need_full_gc
        if( made_progress ) {
          RESTORE_CANCEL(cancel_state);
          // record the fact we should retry without the stop function
          *retry = TRUE;
          return(TRUE);
        }
      }
      //      puts("after pending free");
      //HINTGC_collect_inner(GC_never_stop_func);
#endif
      gc_not_stopped = GC_try_to_collect_inner( stop_func );
      if (gc_not_stopped == TRUE || !*retry) {
        /* Either the collection hasn't been aborted or this is the     */
        /* first attempt (in a loop).                                   */
        RESTORE_CANCEL(cancel_state);
        // record the fact we should retry without the stop function
        *retry = TRUE;
        return(TRUE);
      }
      *retry = FALSE;
    } else {
      //GC_printf("Not trying to collect\n");
    }

    blocks_to_get = GC_heapsize/(HBLKSIZE*GC_free_space_divisor)
                        + needed_blocks;
    if (blocks_to_get > MAXHINCR) {
      word slop;

      /* Get the minimum required to make it likely that we can satisfy */
      /* the current request in the presence of black-listing.          */
      /* This will probably be more than MAXHINCR.                      */
      if (ignore_off_page) {
        slop = 4;
      } else {
        slop = 2 * divHBLKSZ(BL_LIMIT);
        if (slop > needed_blocks) slop = needed_blocks;
      }
      if (needed_blocks + slop > MAXHINCR) {
        blocks_to_get = needed_blocks + slop;
      } else {
        blocks_to_get = MAXHINCR;
      }
    }

    //    GC_printf("Fall through to heap expansion: %d\n", blocks_to_get);



    if (!GC_expand_hp_inner(blocks_to_get)
        && !GC_expand_hp_inner(needed_blocks)) {
      if (gc_not_stopped == FALSE) {
        /* Don't increment GC_fail_count here (and no warning).     */
        GC_gcollect_inner();
        GC_ASSERT(GC_bytes_allocd == 0);
      } else if (GC_fail_count++ < GC_max_retries) {
        WARN("Out of Memory!  Trying to continue ...\n", 0);
        GC_gcollect_inner();
      } else {
#       if !defined(AMIGA) || !defined(GC_AMIGA_FASTALLOC)
          WARN("Out of Memory! Heap size: %" GC_PRIdPTR " MiB."
               " Returning NULL!\n", (GC_heapsize - GC_unmapped_bytes) >> 20);
#       endif
        RESTORE_CANCEL(cancel_state);
        return(FALSE);
      }
    } else if (GC_fail_count && GC_print_stats) {
      GC_log_printf("Memory available again...\n");
    }
    RESTORE_CANCEL(cancel_state);
    return(TRUE);
}

/*
 * Make sure the object free list for size gran (in granules) is not empty.
 * Return a pointer to the first object on the free list.
 * The object MUST BE REMOVED FROM THE FREE LIST BY THE CALLER.
 * Assumes we hold the allocator lock.
 */
GC_INNER ptr_t GC_allocobj(size_t gran, int kind)
{
    void ** flh = &(GC_obj_kinds[kind].ok_freelist[gran]);
    GC_bool tried_minor = FALSE;
    GC_bool retry = FALSE;

    if (gran == 0) return(0);

    while (*flh == 0) {
      ENTER_GC();
      /* Do our share of marking work */
        if(TRUE_INCREMENTAL) GC_collect_a_little_inner(1);
      /* Sweep blocks for objects of this size */
        GC_continue_reclaim(gran, kind);
      EXIT_GC();
      if (*flh == 0) {
        GC_new_hblk(gran, kind);
      }
      if (*flh == 0) {
        ENTER_GC();
        if (GC_incremental && GC_time_limit == GC_TIME_UNLIMITED
            && !tried_minor) {
          GC_collect_a_little_inner(1);
          tried_minor = TRUE;
        } else {
          if (!GC_collect_or_expand(1, FALSE, &retry)) {
            EXIT_GC();
            return(0);
          }
        }
        EXIT_GC();
      }
    }
    /* Successful allocation; reset failure count.      */
    GC_fail_count = 0;

    return(*flh);
}
