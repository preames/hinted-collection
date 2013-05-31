
#include "gc.h"
#include "gc_mark.h"

#include <malloc.h>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <algorithm>
namespace {

  struct node {
    node* next;
    int value;
  };

  struct queue {
    node* head;
    node* tail;
    int count;

    char filler[40];

    void construct() {
      head = NULL;
      tail = NULL;
      count = 0;
    }
    queue() { construct(); }

    void add(int i) {
      node* tmp = (node*)malloc(sizeof(node));
      assert( tmp );
      tmp->next = NULL;
      tmp->value = i;
      if(tail) {
        tail->next = tmp;
      } else {
        head = tmp;
      }
      tail = tmp;
      count++;
    }

    int remove() {
      assert( head != NULL );
      node* tmp = head;
      head = tmp->next;
      if( head == NULL ) {
        //head == tail, single item
        tail = NULL;
      }
      tmp->next = NULL;
      int rval = tmp->value;
      free(tmp);
      count--;
      return rval;
    }

    int size() { return count; }
  };

  struct object {
    char filler[50];
  };
  struct large_object {
    char filler[3000];
  };
  struct object* g_objects[20000];
  struct large_object* g_large_objects[200];
  void alloc() {
    for(int i = 0; i < 20000; i++) {
      g_objects[i] = new object();
    }
    for(int i = 0; i < 200; i++) {
      g_large_objects[i] = new large_object();
    }
  }

  void flush_cache() {
    for(int i = 0; i < 200; i++) {
      for(int j = 0; j < sizeof(large_object)-30; j++) {
        g_large_objects[i]->filler[j] = g_large_objects[i]->filler[i];
      }
    }
  }

  void pause_test() {
    puts("Running pause test\n");
    GC_disable();

    alloc(); //just take up space
    
    struct queue worklist;
    worklist.construct();
    //preseed the list
    for(int i = 0; i < 200000; i++) {
      worklist.add(i);
    }

    // add at a faster rate - highlight GC running time.
    for(int i = 0; i < 4000000; i++) {
      worklist.add(i);
      worklist.add(i);
      worklist.remove();
      //      flush_cache();
      if (i % 10000 == 0) {
        GC_enable();
        printf("Iteration: %d\n", i);
        printf("----- heap size is %lu bytes\n",
               (unsigned long)GC_get_heap_size());
        printf("----- theoretical best pending free (in ms): %lu\n",
               (unsigned long)GC_get_heap_size()/12000000); //B/ms
#ifdef ENABLE_PF
        GC_pfcollect();
#endif
        GC_disable();
      }
    }
    // empty the worklist
    while( worklist.size() ) {
      worklist.remove();
      if (worklist.size() % 10000 == 0) {
        GC_enable();
        printf("Worklist: %d\n", worklist.size());
        printf("----- heap size is %lu bytes\n",
               (unsigned long)GC_get_heap_size());
        printf("----- theoretical best pending free (in ms): %lu\n",
               (unsigned long)GC_get_heap_size()/12000000); //B/ms
#ifdef ENABLE_PF
        GC_pfcollect();
#endif
        GC_disable();
      }
    }
  }
} 
int main() {
  pause_test();
  printf("passed\n");
  return 0;
}
