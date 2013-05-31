
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include "gc.h"
#include "gc_mark.h"

#define free(e) GC_free(e)


void fragement_size_class(int size) {
  GC_disable();

  void** arr = (void**)malloc( sizeof(void*) * 10000 );
  for(int i = 0; i < 10000; i++) {
    arr[i] = malloc(size);
  }
  
  for(int j = 0; j < 20; j++) {
    for(int i = 0; i < 10000; i++) {
      int pos = rand() % 10000;
      free(arr[pos]);
      arr[pos] = malloc(size);
    }
    GC_enable();
    //don't report here
    GC_pf_stat_enable(0);
    GC_gcollect();
    GC_pf_stat_enable(1);
    GC_disable();

  }
  for(int i = 0; i < 10000; i++) {
    free( arr[i] );
    arr[i] = NULL;
  }
  delete[] arr;
  arr = NULL;

  GC_enable();
  //don't report for these
  GC_pf_stat_enable(0);
  GC_gcollect();
  GC_gcollect();
  GC_pf_stat_enable(1);
}
  

struct oct_tree_node;
struct oct_tree_node {
  struct oct_tree_node* children[8];
};

oct_tree_node* test_generate_tree(const int n) {
  if( n == 0) {
    return NULL;
  }
  struct oct_tree_node* rval = new oct_tree_node();
  for(int i = 0; i < 8; i++) {
    rval->children[i] = test_generate_tree(n-1);
  }
  return rval; 
}

//a wide array of references to small objects
void* test_generate_wide(const int n) {
  void** arr = (void**)malloc( sizeof(void*)*n );
  for(int i = 0; i < n; i++) {
    arr[i] = malloc(sizeof(4));
  }
  return arr;
}

void free_wide(void** arr, const int n) {
  for(int i = 0; i < n; i++) {
    free(arr[i]);
  }
  free(arr);
}

struct fan_node {
  void* next;
};

void* test_generate_fan_in(const int n) {
  void** arr = (void**) malloc( sizeof(void*) * n );
  void* end = malloc(4);
  for(int i = 0; i < n; i++) {
    arr[i] = malloc(sizeof(fan_node));
    ((fan_node*)arr[i])->next = end;
  }
  return arr;
}
void free_fan_in(void** arr, const int n) {
  void* end = arr[0];
  for(int i = 0; i < n; i++) {
    free(arr[i]);
  }
  free(arr);
  free(end);
}




void* test_generate_single() {
  return malloc( 4*sizeof(void*) );
}


struct linked_list_node {
  linked_list_node* next;
  int value;
  linked_list_node(int v) {
    value = v;
  }
};

linked_list_node* test_generate_linked_list(int length = 10) __attribute__((__noinline__));
linked_list_node* test_generate_linked_list(int length) {
  asm ("");
  linked_list_node* cur = (linked_list_node*)malloc(sizeof(linked_list_node));
  assert( cur );
  cur->value = 0;
  linked_list_node* rval = cur;
  for(int i = 0; i < length; i++) {
    cur->next = (linked_list_node*)malloc(sizeof(linked_list_node));
    assert( cur->next );
    cur->next->value = i+1;
    cur = cur->next;
  }
  cur->next = NULL;
  asm ("");
  return rval;
}

void destruct_linked_list(linked_list_node* node) __attribute__((__noinline__));
void destruct_linked_list(linked_list_node* node) {
  asm ("");
  assert( false ); //walks off stack, fixme

  assert( node );
  if( node->next ) {
    destruct_linked_list(node->next);
    free(node->next);
    node->next = NULL;
  }
  asm ("");
}

//free, but do not destruct
void free_linked_list(linked_list_node* node) __attribute__((__noinline__));
void free_linked_list(linked_list_node* node) {
  asm ("");
  assert( node );
  while( NULL != node ) {
    linked_list_node* next = node->next;
    free(node);
    node = next;
  }
  asm ("");
}

void print_linked_list(linked_list_node* node) {
  assert( node );
  while( node ) {
    printf("%d, ", node->value);
    linked_list_node* next = node->next;
    node = next;
  }
}



void bestcase() {
  GC_disable();
  //a single sample root to that data
  linked_list_node* roots[256];
  int roots_length = 256;

  //Store in inverse order to avoid stride 1
  for(int i = 255; i >= 0; i--) {
    roots[i] = test_generate_linked_list();
  }
  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}


void selftest() {

	  
  //a single sample root to that data
  void* roots[5];
  memset(&roots[0], 0, sizeof(void*)*5);
  
  GC_disable();
  roots[0] = (void*)test_generate_single();
  GC_enable();
  
#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
  GC_reachable_here(roots[0]);
  
  GC_disable();
  roots[0] = (void*) test_generate_linked_list();
  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif

  GC_reachable_here(roots[0]);

  GC_disable();
  roots[0] = test_generate_tree(4);
  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
  GC_reachable_here(roots[0]);

  GC_disable();
  roots[0] = test_generate_tree(5);
  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
  GC_reachable_here(roots[0]);

  GC_disable();
  roots[0] = test_generate_tree(6);
  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
  GC_reachable_here(roots[0]);

  GC_disable();
  roots[0] = test_generate_wide(300);
  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
  GC_reachable_here(roots[0]);

}

void pretty_print_size(std::ostream& stream, uint64_t size) {
  if(0 != size % 1024 ) {
    stream << size << " bytes";
    return;
  }
  size /= 1024;
  if(0 != size % 1024 ) {
    stream << size << " KB";
    return;
  }
  size /= 1024;
  stream << size << " MB";
}

void microbench_prime() {
  //prime the system
  for(int i = 0; i < 5; i++) {
    printf("Priming system.  Ignore.\n");
    //fill in that space (with test data)
    //need to clear the mark bits each iteration
    GC_disable();
    void* root = test_generate_single();
    GC_enable();

#ifdef ENABLE_PF
    GC_pfcollect();
#else
    GC_gcollect();
#endif
    GC_reachable_here(root);
  }
}

#if 0
/** Show the increase in kernel launch time */
void microbench_launch() {
  uint32_t bufsizeinwords = 63*1024*1024;
  uint32_t bufsizeinbytes = bufsizeinwords*sizeof(uint32_t);
  uint32_t *mem = (uint32_t*)malloc(bufsizeinbytes);
  assert( mem );
  memset(mem, bufsizeinbytes, 0);

  // For alignment, need an offset of 3 elements at the beginning.
  // So the first element that is vector-aligned is the header.
  uint32_t *space = mem + 3;
	  
	  
  //a single sample root to that data
  uint32_t roots[5];
  roots[0] = (uint32_t)space; //address of our test data's first node
  int roots_length = 1;

  //prime the system
  for(int i = 0; i < 5; i++) {
    printf("Priming system.  Ignore.\n");
    //fill in that space (with test data)
    //need to clear the mark bits each iteration
    test_generate_single(space, bufsizeinbytes );

    //Make sure the data structures passed in are correct
    validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), 
                        (void*)space, (void*)((char*)space+bufsizeinbytes));
    launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), 
                                (void*)mem, (void*)((char*)mem+bufsizeinbytes));

  }

  std::ofstream stream("micro-launch-size.dat");
  stream << 
    "# THIS FILE IS MACHINE GENERATED.\n"
    "# Format:\n"
    "# column 1 - size of memory in bytes\n"
    "# column 2 - average overhead\n"
    "# column 3 - minimum overhead\n"
    "# column 4 - max overhead\n";

  //try power of two larger memory buffers to see what effect this has on the 
  // launch overhead
  for(uint64_t size = 64; size <= bufsizeinbytes; size*=2) {
    std::cout << "Testing space size ";
    pretty_print_size(std::cout, size);
    std::cout << std::endl;
    std::vector<float> overheads;
    for(int i = 0; i < 20; i++) {
      //fill in that space (with test data)
      //need to clear the mark bits each iteration
      test_generate_single(space, bufsizeinbytes );
      
      //Make sure the data structures passed in are correct
      validate_structure( (void*)&roots[0], (void*)(&roots[0]+roots_length), 
                          (void*)space, (void*)((char*)space+size));
      launch_checked_opencl_mark( (void*)&roots[0], (void*)(&roots[0]+roots_length), 
                                  (void*)mem, (void*)((char*)mem+size));
      float overhead = g_stat_manager.stats.back().gpu_queueTimeInMilliseconds - g_stat_manager.stats.back().gpu_executionTimeInMilliseconds;

      overheads.push_back( overhead );
    }
    float low = min(overheads);
    float high = max(overheads);
    float s = sum(overheads);
    stream << " " << size << " " << s / 20 << " " << low << " " << high << std::endl;
  }

  free( mem );
  mem = NULL;
  space = NULL;
}
#endif

#define DEBUGF printf

#if 0
void repeattest(int iterations) {
	DEBUGF("Running %d iterations of the GC test loop.\n", iterations);
	
	for(int i = 0; i < iterations; i++) {
	  if( iterations != 1 ) {
	    DEBUGF("\n");
	    DEBUGF("----------- Iteration %d --------\n", i);
	  }

	  // Allocate space for heap structure.
	  const int bufSize = 1024*1024; //in words
	
	  DEBUGF("Allocating buffer...\n");
	  uint32_t *space = (uint32_t*)malloc(bufSize*sizeof(uint32_t));
      assert( space );
	  memset(space, bufSize*sizeof(uint32_t), 0);
	  DEBUGF("Test ptr: %p\n", space);
	  //fill in that space (with test data)
	  unsigned offset = 0;
	  test_generate_tree(4, space, &offset);

	  // Generate test data.
	  uint32_t roots[5];
	  roots[0] = (uint32_t)space; //address of our test data's first node
	  int roots_length = 1;

	  DEBUGF("Tree elements: %d (offset: %d)\n", offset);

	  DEBUGF("%d, %d", sizeof(uint32_t), sizeof(uint32_t*));
	  DEBUGF("%p, %p", &roots[0], &roots[0]+roots_length);
	  launch_checked_opencl_mark(&roots[0], &roots[0]+roots_length, space, space+bufSize);

	  free(space);
	  space = NULL;
	}
}
#endif

//NOTE: It is crtically important that the root set be a global variable
// if they're local to each function, the complier will "nicely" perform
// dead-store-elimination in anything other than -g (and sometimes then.)
// In other words, this is neccesssary to get meaningful results.
//Note: In case that wasn't clear, that means _ALL_ of these tests rely
// on undefined behavior.  That's problematic at best.
void* g_roots[10000];

void stomp_roots() {
  memset(&(g_roots[0]), 0, 10000*sizeof(void*));
}

void all_roots_reachable() {
  for(int i = 0; i < 10000; i++) {
    GC_reachable_here(g_roots[i]);
  }
}
void mb_single_item() {
  GC_disable();
  void* root = test_generate_single();
  free(root);
  root = NULL;
  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif

  GC_reachable_here(root);
}

void mb_single_10kll_dead_hinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(10000);

  GC_clear_stack(NULL);
  //free it
  free_linked_list((linked_list_node*)g_roots[0]);
  GC_clear_stack(NULL);
  stomp_roots();
  GC_clear_stack(NULL);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}
void mb_single_10kll_dead_unhinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(10000);

  GC_clear_stack(NULL);
  stomp_roots();
  GC_clear_stack(NULL);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}

void mb_single_10kll_live_hinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(10000);

  //free it
  free_linked_list((linked_list_node*)g_roots[0]);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}

void mb_single_10kll_live_unhinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(10000);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif

}


void mb_single_1Mll_dead_hinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(1000000);

  GC_clear_stack(NULL);
  //free it
  free_linked_list((linked_list_node*)g_roots[0]);
  GC_clear_stack(NULL);
  stomp_roots();
  GC_clear_stack(NULL);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}
void mb_single_1Mll_dead_unhinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(1000000);

  GC_clear_stack(NULL);
  stomp_roots();
  GC_clear_stack(NULL);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}

void mb_single_1Mll_live_hinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(1000000);

  //free it
  free_linked_list((linked_list_node*)g_roots[0]);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}

void mb_single_1Mll_live_unhinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(1000000);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif

}

void mb_single_10Mll_live_unhinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(10000000);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif

}

void mb_single_100Mll_live_unhinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(100000000);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif

}

void mb_single_100Kll_live_unhinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(100000);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}



void mb_6x1Mll_partial_cleanup() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(1000000);
  g_roots[1] = (void*)test_generate_linked_list(1000000);
  g_roots[2] = (void*)test_generate_linked_list(1000000);
  g_roots[3] = (void*)test_generate_linked_list(1000000);
  g_roots[4] = (void*)test_generate_linked_list(1000000);
  g_roots[5] = (void*)test_generate_linked_list(1000000);

  GC_clear_stack(NULL);
  //free it
  free_linked_list((linked_list_node*)g_roots[0]);
  g_roots[0] = NULL;
  free_linked_list((linked_list_node*)g_roots[3]);
  g_roots[3] = NULL;
  GC_clear_stack(NULL);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}

/* Delete 10,000 elements off the end of an 1M element linked list
   Put some other garbage on the heap, just to make it slightly more real */
void mb_deep_turnover() {
  GC_disable();
  g_roots[0] = (void*)test_generate_linked_list(1000000);
#if 1
  g_roots[1] = (void*)test_generate_linked_list(100000);
  g_roots[2] = (void*)test_generate_linked_list(100000);
  g_roots[3] = (void*)test_generate_linked_list(100000);
  g_roots[4] = (void*)test_generate_linked_list(100000);
  g_roots[5] = (void*)test_generate_linked_list(100000);
#endif
  GC_clear_stack(NULL);

  //free something at the end
  linked_list_node* cur = (linked_list_node*)g_roots[0];
  while(cur) {
    if( cur->value == 990000 ) {
      //    if( !cur->next->next->next->next->next->next ) {
      free_linked_list(cur->next);
      cur->next = NULL;
    }
    cur = cur->next;
  }
  GC_clear_stack(NULL);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}

void mb_256_by_10kll() {
  GC_disable();
  //Store in inverse order to avoid stride 1
  for(int i = 255; i >= 0; i--) {
    g_roots[i] = test_generate_linked_list(10000);
  }

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
  for(int i = 0; i < 256; i++) {
    GC_reachable_here(g_roots[i]);
  }
}


//simply a large heap which we can not scan
void mb_2560_by_ll() {
  GC_disable();

  //Store in inverse order to avoid stride 1
  for(int i = 2560-1; i >= 0; i--) {
    g_roots[i] = test_generate_linked_list(1000);
  }

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif

  for(int i = 0; i < 2560; i++) {
    GC_reachable_here(g_roots[i]);
  }

}

void mb_unbalenced_live() {
  GC_disable();

  //Store in inverse order to avoid stride 1
  for(int i = 255; i >= 0; i--) {
    g_roots[i] = (void*)test_generate_linked_list(1000);
  }
  for(int i = 255; i >= 0; i--) {
    g_roots[i+256] = (void*)test_generate_tree(6);
  }
  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif

  for(int i = 0; i < 512; i++) {
    GC_reachable_here(g_roots[i]);
  }
}

void mb_unbalenced_part_dead() {
  GC_disable();

  //allocate and throw away part of that structure
  for(int i = 255; i >= 0; i--) {
    free_linked_list( test_generate_linked_list(1000) );
  }
  //Store in inverse order to avoid stride 1
  for(int i = 255; i >= 0; i--) {
    g_roots[i+256] = (void*)test_generate_tree(6);
  }
  GC_clear_stack(NULL);
  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif

  for(int i = 256; i < 512; i++) {
    GC_reachable_here(g_roots[i]);
  }
}

//todo: fix generate wide (duplicate)
//todo: explain comment at top of file

void mb_divergence() {
  GC_disable();
  g_roots[0] = test_generate_wide(1000);
  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
  GC_reachable_here(g_roots[0]);
}



void mb_vp_ll() {
  /* From VP pg 7.
     Concurrently 16 Cuda kernels allocate lists with 8192 elements
     each. One list is kept, the other 15 lists become collectable. 
     This is repeated 8 times.
     
     From inspection of the code, the 8 times are separate gcs.  We perform each
     micro benchmark multiple times, so we skip this.
  */

  GC_disable();
  //a single sample root to that data
  g_roots[0] = (void*)test_generate_linked_list(8192);

  for(int j = 0; j < 15; j++) {
    free_linked_list( test_generate_linked_list(8192) );
  }
  GC_enable();
  

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif

  GC_reachable_here(g_roots[0]);
}


void mb_vp_array() {
  GC_disable();
  /* From VP page 7, col 2:
     The bottom chart of Fig. 9 shows the results of repeatedly allo-
     cating large arrays of references to fresh objects. This benchmark
     has 1024 parallel kernels each of which creates arrays filled with
     1024 small objects. The arrays of only 64 kernels are kept. The
     other 960 arrays (and pointed to objects) become garbage.
  */

  for(int i = 0; i < 1024; i++) {
    if( i < 64 ) {
      g_roots[i] = test_generate_wide(1024);
    } else {
      void* t = test_generate_wide(1024);
      free_wide((void**)t, 1024);
    }
  }
  GC_enable();


#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif 

  for(int i = 0; i < 64; i++) {
    GC_reachable_here( g_roots[i] );
  }
}

void mb_fan_in_dead_hinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_fan_in(1000000);

  //free it
  free_fan_in( (void**)g_roots[0], 1000000);

  stomp_roots();

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}

void mb_fan_in_live_hinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_fan_in(1000000);

  //free it
  free_fan_in((void**)g_roots[0], 1000000);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}

void mb_fan_in_live_unhinted() {
  GC_disable();
  g_roots[0] = (void*)test_generate_fan_in(1000000);

  GC_enable();

#ifdef ENABLE_PF
  GC_pfcollect();
#else
  GC_gcollect();
#endif
}

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void run_bm(const char* fname, void (*bench_fun)()) {
  printf("%s\n", fname);
  //  g_stat_manager.stream.open(fname);

  //set the stats file that the instrumented writer is looking for

  const char* orig = getenv("pf_stats_file");
  setenv("pf_stats_file", fname, 1);


  GC_pf_stat_enable(0);
  //purge the test data
  stomp_roots();
  GC_gcollect();
  GC_gcollect_and_unmap();
  GC_pf_stat_enable(1);
  
#if 1
  // Note: This is wildly unfortanate, but to get the heap sizes to match
  // up with the previously reported microbenchmarks from the serial section
  // we need to heavily fragement the heap here.
  for(int i = 0; i < 20; i++) {
    fragement_size_class( sizeof(void*) );
    fragement_size_class( sizeof(linked_list_node) );
    fragement_size_class( sizeof(oct_tree_node) );
  }
  GC_gcollect();
#endif


  for(int i = 0; i < 20; i++) {
    printf("- iteration %d\n", i);


    // Okay, we're going to use tricks with fork/wait to ensure every
    // test run sees exactly the same environment.
    // Note: This turns out to be a major issue for multi-threaded programs.
    // fork does NOT respect threads in any way shape or form.  BDW's handling
    // assumes that fork will always be followed by exec (thus skipping thread
    // startup for markers.)  We want the marker threads to exist and be
    // respawned.  As a result, supporting this required a change to both
    // the configuration - to build with fork support enabled - and the 
    // implementation of the forking support - to restart threads.

    //interweave the two to get the lines appropriately
    // in the output file
#if 1
    pid_t pID = fork();
    if (pID == 0) { // child
      GC_pf_stat_gc_only(0);
      bench_fun();
      exit(0);
    } else if( pID < 0 ) { //fail
        assert(false);
    } else { //parent
      int rv;
      wait(&rv);
      assert( 0 == WEXITSTATUS(rv) );
    }

    pID = fork();
    if (pID == 0) { // child
      GC_pf_stat_gc_only(1);
      bench_fun();
      exit(0);
    } else if( pID < 0 ) { //fail
        assert(false);
    } else { //parent
      int rv;
      wait(&rv);
      assert( 0 == WEXITSTATUS(rv) );
    }
#else
    // As it happens, we can't fork in the parallel collector setting.  Ouch.
    // Now can we simply run it all in one process - the collector stores
    // state across runs - i.e. enabling heuristics, stats, possible corruption, etc..
    // This version has unstable and varying results.  Do not use if possible.
    GC_pf_stat_gc_only(0);
    bench_fun();
    GC_pf_stat_gc_only(1);
    bench_fun();
#endif
  }



  //restore it back to what it was
  setenv("pf_stats_file", orig, 1);


  //g_stat_manager.dump_to_file();
  //g_stat_manager.stats.clear();
}  

//#include <map>

//
void microbenchs_for_eval() {
  printf("Running micro benchmarks\n");

  //std::map<string, void (*bench_fun)()> available;

#define ENABLE_FOR_PAPER 1

  //clear anything done so far
  //g_stat_manager.stream.close();
  //g_stat_manager.stats.clear();

  //run_bm("microbench_single.csv", mb_single_item );

  // This batch illustrates basic properties
#if 0 //not being used
  //10k is just too short, no useful results
  run_bm("microbench_single_10kll_dead_hinted.csv", mb_single_10kll_dead_hinted );
  run_bm("microbench_single_10kll_dead_unhinted.csv", mb_single_10kll_dead_unhinted );
  run_bm("microbench_single_10kll_live_hinted.csv", mb_single_10kll_live_hinted );
  run_bm("microbench_single_10kll_live_unhinted.csv", mb_single_10kll_live_unhinted );
#endif

  // Why are the results of this so unstable?  High variance for parallel run.
  //run_bm("microbench_single_1Mll_dead_unhinted.csv", mb_single_1Mll_dead_unhinted );


#if ENABLE_FOR_PAPER
  run_bm("microbench_single_1Mll_dead_hinted.csv", mb_single_1Mll_dead_hinted );
  run_bm("microbench_single_1Mll_dead_unhinted.csv", mb_single_1Mll_dead_unhinted );
  run_bm("microbench_single_1Mll_live_hinted.csv", mb_single_1Mll_live_hinted );
  run_bm("microbench_single_1Mll_live_unhinted.csv", mb_single_1Mll_live_unhinted );
#endif

#if 0
  run_bm("microbench_single_100Kll_live_unhinted.csv", mb_single_100Kll_live_unhinted );
  run_bm("microbench_single_10Mll_live_unhinted.csv", mb_single_10Mll_live_unhinted );
  run_bm("microbench_single_100Mll_live_unhinted.csv", mb_single_100Mll_live_unhinted );
#endif
  
#if ENABLE_FOR_PAPER
  //1M fan in - really only useful for showing parallel limits
  run_bm("microbench_fan_in_dead_hinted.csv", mb_fan_in_dead_hinted );
  run_bm("microbench_fan_in_live_hinted.csv", mb_fan_in_live_hinted );
  run_bm("microbench_fan_in_live_unhinted.csv", mb_fan_in_live_unhinted );
#endif


  // These cases show where we win
#if ENABLE_FOR_PAPER
  //useful case - only some data structures cleaned up
  run_bm("microbench_6x1Mll_partial_cleanup.csv", mb_6x1Mll_partial_cleanup);
  //useful case - deep turnover
  run_bm("microbench_deep_turnover.csv", mb_deep_turnover);
#endif

#if ENABLE_FOR_PAPER
  //turn over of 32k 1000x with large heap (one iteration)
  run_bm("microbench_unbalenced_live.csv", mb_unbalenced_live );
  run_bm("microbench_unbalenced_part_dead.csv", mb_unbalenced_part_dead );
#endif

#if ENABLE_FOR_PAPER
  //simply examples of large heaps to be scanned,
  // but with different shapes
  run_bm("microbench_2560_by_100ll.csv", mb_2560_by_ll );
  run_bm("microbench_256_by_10kll.csv", mb_256_by_10kll );
#endif

  // Working, but somewhat pointless
#if 0 //ENABLE_FOR_PAPER
  run_bm("microbench_divergence.csv", mb_divergence );  
  /* These couple are aproximations of the VP paper */
  run_bm("microbench_vp_ll.csv", mb_vp_ll );
  run_bm("microbench_vp_array.csv", mb_vp_array );
#endif
  //force a flush
  //g_stat_manager.stream.open("stat.csv");
}





int main(int argc, char **argv) {

  //before anything else, run some sanity checks
  DEBUGF("Running self test sequence\n");
  //selftest();


  // WARNING: Make sure any micro benchmark you run
  // is _NOT_ the first kernel run.  Run it multiple 
  // times and ignore the first few executions.  
  //for(int i = 0; i < 20; i++) {
  //  bestcase();
  //}

  //prime the system
  //microbench_prime();

  GC_pf_stat_enable(1);
  //each test is run (PF, GC) 20x
  microbenchs_for_eval();





  //Did you remember to change the QUEUE_SIZE to 100?
  //printf("Running kernel launch micro benchmark\n");
  //microbench_launch();
}


/* The interesting cases:
   - single linked list (live, unhinted)
   - single linked list (live, hinted)
   - single linked list (dead, unhinted)
   - single linked list (dead, hinted)

   - interwoven linked list (one dead/hinted, one live)
     - how much does page level flags hurt us?

   - fanout/fanin case (live, unhinted)
   - fanout/fanin case (dead end node, hinted end node)
     - made case about high degree nodes
   - with fanout of 100000 don't get useful difference (due to caching..?)
   - 1M works


   - highlight cases where we do well:
     - turnover at end of list!
     - two linked lists, one live, one not

   - with and without frag
   - with and without edge filtering (all)
   - with and without object coalescing
   - gc vs pf for all combinations
   - report pause + throughput
*/
