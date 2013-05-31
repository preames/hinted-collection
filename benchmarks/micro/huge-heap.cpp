
#include "gc.h"
#include "gc_mark.h"

#include <papi.h>

#include <malloc.h>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <algorithm>
#include <cmath>
namespace {

  struct node {
    node* next;
    int* values[5];
    void init(bool b) {
      int i;
      for(i = 0; i < 5; i++) {
        if(b) {
          values[i] = (int*)malloc(sizeof(int));
          assert( values[i] );
        }
        else {
          values[i] = NULL;
        }
      }
    }
    void destruct() {
      int i;
      for(i = 0; i < 5; i++) {
        free(values[i]);
        values[i] = NULL;
      }
    }
  };

  node* create_ll() {
    node* cur = (node*)malloc(sizeof(node));
    assert( cur );
    node* rval = cur;
    int i;
    for(i = 0; i < 50; i++) {
      cur->next = (node*)malloc(sizeof(node));
      assert( cur->next );
      cur = cur->next;
      cur->init(false);
      
    }
    cur->init(true);
    cur->next = NULL;
    return rval;
  }

  void destruct_ll(node* node) {
    assert( node );
    if( node->next ) {
      destruct_ll(node->next);
      free(node->next);
      node->next = NULL;
    }
    node->destruct();
  }
      

  struct large_object {
    node* items[500];

    void init() {
      int i;
      for(i = 0; i < 500; i++) {
        items[i] = create_ll();
      }
    }
    void destruct() {
      int i;
      for(i = 0; i < 500; i++) {
        destruct_ll(items[i]);
        free(items[i]);
        items[i] = NULL;
      }
    }
  };
  
  const int g_items_size = 20480*4;
  large_object* g_items[g_items_size];  

  int alloc_size() {
    const char* s = getenv("max_alloc_number");
    if(!s) return 2048;
    int rval = atoi(s);
    if( rval < 0 || rval > g_items_size/4 ) {
      printf("bad max alloc number\n");
      abort();
    }
    return rval;
  }

int foo(float p){
    return floor(100/p);
  }
  
  int foo2(int p) {
    switch(p) {
    case 100: return 1;
    case 90: return -90;
    case 75: return -75;
    case 50: return 2;
    case 25: return 4;
    case 10: return 10;
    case 5: return 20;
    case 1: return 100;
    case 0: return 0;
    };
    assert(false);
  }

  int dealloc_percentage() {
    const char* s = getenv("dealloc_percentage");
    if(!s) return 50;
    int rval = atoi(s);
    if( rval < 0 || rval > 100 ) {
      printf("bad max dealloc_precentage\n");
      abort();
    }
    return rval;
  }

#if 0
  int num_dealloc() {
    const char* s = getenv("dealloc_frequency");
    if(!s) return foo2(dealloc_percentage());
    int rval = atoi(s);
    if( rval < 0 || rval > 100000 ) {
      printf("bad max dealloc_frequency\n");
      abort();
    }
    return rval;
  }
#endif

  
  void allocate_large_heap() {
    GC_disable();
    int i;
    //TODO: walking off the end of the fricking array N > 2048
    for(i = 0; i < g_items_size; i++) {
      g_items[i] = NULL;
    }
    const int N = alloc_size();
    printf("Alloc Number: %d\n", N);
    assert( 4*N <= g_items_size );
    //NOTE: It is KEY that we skip 3 elements
    // this avoids a false sharing issue which is cricitical for
    // fairly benchmarking the parallel baseline collector.  The
    // hinted collector does not care.
    for(i = 0; i < N; i++) {
      g_items[4*i] = (large_object*) malloc( sizeof(large_object) );
      assert( g_items[4*i] );
      g_items[4*i]->init();
    }
    GC_enable();
  }

  

  void deallocate_some() {
    int i;
    const int perc = dealloc_percentage();
    const int mod = foo2(perc);
    printf("Dealloc freq: %d\n", mod);
    if( !mod ) return;
    if( mod > 0 ) {
      const int N = alloc_size();
      for(i = 0; i < N; i++) {
        if( i % mod == 0 ) {
          g_items[4*i]->destruct();
          free( g_items[4*i] );
          g_items[4*i] = NULL;
        }
      }
    } else {
      switch(mod) {
      case -90: {
        const int N = alloc_size();
        int c = 0;
        for(i = 0; i < N; i++) {
          if( i % 10 == 0 ) {
            c = 0;
          }
          if( c < 9 ) {
            g_items[4*i]->destruct();
            free( g_items[4*i] );
            g_items[4*i] = NULL;
          }
          c++;
        }
        break;
      }
      case -75: {
        const int N = alloc_size();
        int c = 0;
        for(i = 0; i < N; i++) {
          if( i % 4 == 0 ) {
            c = 0;
          }
          if( c < 3 ) {
            g_items[4*i]->destruct();
            free( g_items[4*i] );
            g_items[4*i] = NULL;
          }
          c++;
        }
        break;
      }
      default: assert(false);
      };
    }
  }

  int count_nonnull() {
    int rval = 0;
    int i;
    for(i = 0; i < g_items_size; i++) {
      if( g_items[i] ) {
        rval++;
      }
    }
    return rval;
  }
}

int helper() {
  allocate_large_heap();
  deallocate_some();
  GC_clear_stack(NULL);
  return 0;
}

int gc_run() {
  const char* s = getenv("gc_run");
  if(!s) return 0;
  int rval = atoi(s);
  if( rval < 0 || rval > 1 ) {
    printf("bad max gc_run\n");
    abort();
  }
  return rval;
}

namespace {
  // On my laptop, can't include L2, L3 due to permission issues??
  int papi_events[] = {PAPI_L1_TCM, PAPI_L1_LDM, PAPI_L1_STM};//, PAPI_L2_TCM, PAPI_L3_TCM};
  //int papi_events[] = {PAPI_L1_TCM, PAPI_L1_LDM, PAPI_L1_STM, PAPI_L2_TCM, PAPI_L3_TCM};
  const int papi_event_size = sizeof(papi_events)/sizeof(int);

  long long papi_values[papi_event_size];
}

const char* papi_error_string(int code) {
  switch(code) {
  case PAPI_EINVAL:
    return "PAPI_EINVAL";
  case PAPI_EISRUN:
    return "PAPI_EISRUN";
  case PAPI_ESYS:
    return "PAPI_ESYS";
  case PAPI_ENOMEM:
    return "PAPI_ENOMEM";
  case PAPI_ECNFLCT:
    return "PAPI_ECNFLCT";
  case PAPI_ENOEVNT:
    return "PAPI_ENOEVNT";
  default:
    return "UNKNOWN ERROR CODE";
  };
}

int main() {
#if 0
  int status = PAPI_start_counters(papi_events, papi_event_size);
  if( PAPI_OK != status ) {
    puts(papi_error_string(status) );
    printf("%d\n", status);
    assert( false );
  }

  assert( PAPI_OK == PAPI_read_counters(papi_values, papi_event_size) );
 #endif

  const int N = alloc_size();
  printf("Alloc Number: %d\n", N);
  //  const int dealloc = num_dealloc();
  //printf("Dealloc freq: %d\n", dealloc);  
  int gc = gc_run();
  printf("GC Run: %d\n", gc);

  helper();
  int rval = count_nonnull();

  printf("Done alloc/dealloc!\n");
  printf("----- heap size is %lu bytes\n",
         (unsigned long)GC_get_heap_size());
  printf("----- theoretical best pending free (in ms): %lu\n",
         (unsigned long)GC_get_heap_size()/12000000); //B/ms
#ifdef ENABLE_PF
  GC_pf_stat_gc_only(gc);
  GC_pf_stat_enable(1);
  //assert( PAPI_OK == PAPI_read_counters(papi_values, papi_event_size) );

  GC_pfcollect();

  //GC_gcollect();
  
#else
  // wrap GC_collect since it doesn't have instrumentation
  // when we build against the classic unmodified BDW
  /*scope*/ {

    long long papi_start, papi_end;                               
    unsigned long papi_time_diff;                                 
    papi_start = PAPI_get_real_usec();
    
    GC_gcollect();
    
    papi_end = PAPI_get_real_usec();                              
    papi_time_diff = papi_end - papi_start;                       
    papi_time_diff = papi_time_diff/1000;  
    
    printf("GC (Outer Call) Time %lu ms\n",
           papi_time_diff);
  }
#endif
  //  for(int i = 0; i < papi_event_size; i++) {
  //  printf("%lld\n", papi_values[i]);
  //}
//exit(1);
  printf("----- heap size is %lu bytes\n",
         (unsigned long)GC_get_heap_size());

  printf("passed\n");
  return 0;
}
