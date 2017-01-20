/* Tar Heels Allocator
 * 
 * Simple Hoard-style malloc/free implementation.
 * Not suitable for use for large allocatoins, or 
 * in multi-threaded programs.
 * 
 * to use: 
 * $ export LD_PRELOAD=/path/to/th_alloc.so <your command>
 */

/* Hard-code some system parameters */
/**
  Names: Luke DiGiacomo, Marcus Wallace
  Honor Pledge:  I certify that no unauthorized assistance has been received or given in the completion of this work
  Signature: Luke DiGiacomo, Marcus Wallace
*/
#define SUPER_BLOCK_SIZE 4096
#define SUPER_BLOCK_MASK (~(SUPER_BLOCK_SIZE-1))
#define MIN_ALLOC 32 /* Smallest real allocation.  Round smaller mallocs up */
#define MAX_ALLOC 2048 /* Fail if anything bigger is attempted.  
            * Challenge: handle big allocations */
#define RESERVE_SUPERBLOCK_THRESHOLD 2

#define FREE_POISON 0xab
#define ALLOC_POISON 0xcd

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <sys/mman.h>
#include <string.h>

#define assert(cond) if (!(cond)) __asm__ __volatile__ ("int $3")

/* Object: One return from malloc/input to free. */
struct __attribute__((packed)) object {
  union {
    struct object *next; /*For free list (when not in use) */
    char * raw; /* Actual data */
  };
};

/* Super block bookeeping; one per superblock.  "steal" the first
 * object to store this structure
 */
struct __attribute__((packed)) superblock_bookkeeping {
  struct superblock_bookkeeping * next; // next super block
  struct object *free_list;
  // Free count in this superblock
  uint8_t free_count; // Max objects per superblock is 128-1, so a byte is sufficient
  uint8_t level;
};
  
/* Superblock: a chunk of contiguous virtual memory.
 * Subdivide into allocations of same power-of-two size. */
struct __attribute__((packed)) superblock {
  struct superblock_bookkeeping bkeep;
  void *raw;  // Actual data here
};


/* The structure for one pool of superblocks.  
 * One of these per power-of-two */
struct superblock_pool {
  struct superblock_bookkeeping *next;
  uint64_t free_objects; // Total number of free objects across all superblocks
  uint64_t whole_superblocks; // Superblocks with all entries free
};

// 10^5 -- 10^11 == 7 levels
#define LEVELS 7
static struct superblock_pool levels[LEVELS] = {{NULL, 0, 0},
            {NULL, 0, 0},
            {NULL, 0, 0},
            {NULL, 0, 0},
            {NULL, 0, 0},
            {NULL, 0, 0},
            {NULL, 0, 0}};

static inline int size2level (ssize_t size) 
  {
   /* Your code here.
    * Convert the size to the correct power of two. 
    * Recall that the 0th entry in levels is really 2^5, 
    * the second level represents 2^6, etc.
    */
    
    double force = (double) size; //force the size to a double
    int l =0;

    if(size > MAX_ALLOC)  //prechecks to see if size is correct
      return -1;
   
    else if(size < 1 )
      return -1;
    
    else
    {
      while(force > 1)    //divide by 2 and keep track of the number of time with the variable l
      {
         force=force/2;
         l++;
      }
    }

  if(l >= 0 && l <= 5)
    return 0;

  else if(l > 5 && l < 12)
    return l - 5;


  else
    return -1;
}

static inline
struct superblock_bookkeeping * alloc_super (int power) 
{

  void *page;
  struct superblock* sb;
  int free_objects = 0, bytes_per_object = 0;
  char *cursor;
  // Your code here  
  // Allocate a page of anonymous memory
  // WARNING: DO NOT use brk---use mmap, lest you face untold suffering
  page = mmap(NULL, SUPER_BLOCK_SIZE,PROT_READ|PROT_WRITE,MAP_ANONYMOUS|MAP_PRIVATE,-1,0); //maps anonymous page of memory
  sb = (struct superblock*) page;
  // Put this one the list.
  sb->bkeep.next = levels[power].next;
  levels[power].next = &sb->bkeep;
  levels[power].whole_superblocks++;
  
  sb->bkeep.level = power;
  sb->bkeep.free_list = NULL;
  
  // Your code here: Calculate and fill the number of free objects in this superblock
  //  Be sure to add this many objects to levels[power]->free_objects, reserving
  //  the first one for the bookkeeping.
  bytes_per_object = 1<<(power+5);
  sb->bkeep.free_count = (SUPER_BLOCK_SIZE/bytes_per_object)-1;
  free_objects = sb->bkeep.free_count;
  
   
  if(!(levels[power].free_objects))
    levels[power].free_objects = sb->bkeep.free_count;

  else 
    levels[power].free_objects += sb->bkeep.free_count;

 
  // The following loop populates the free list with some atrocious
  // pointer math.  You should not need to change this, provided that you
  // correctly calculate free_objects.
  
  cursor = (char *) sb;
  // skip the first object
  for (cursor += bytes_per_object; free_objects--; cursor += bytes_per_object) {
    // Place the object on the free list
    struct object* tmp = (struct object *) cursor;
    tmp->next = sb->bkeep.free_list;
    sb->bkeep.free_list = tmp;
  }
  return &sb->bkeep;
}

void *malloc(size_t size) 
{


  struct superblock_pool *pool;
  struct superblock_bookkeeping *bkeep;
  void *rv = NULL;
  int power = size2level(size);
 
  
  // Check that the allocation isn't too big
  if (power == -1) 
  {
 
    errno = -ENOMEM;
    return NULL;
  }
  


  
  pool = &levels[power];

  //if there is no free space in the pool create another superblock
  //otherwise  
  if (!pool->free_objects)
    bkeep = alloc_super(power);
  
  else
    bkeep = pool->next;


  int i = 0;
  while (bkeep != NULL) 
  {
    if (bkeep->free_count) 
    {
      
      struct object *next = bkeep->free_list;
      /* Remove an object from the free list. */
      rv = next;   /* set return value equal to the first available object in superblock */
      bkeep->free_list= next->next;
                /*make the first object in the freelist equal to the next available object in the supeblock */
      // Your code here
      //
      // NB: If you take the first object out of a whole
      //     superblock, decrement levels[power]->whole_superblocks
   
      if(bkeep->free_count == (SUPER_BLOCK_SIZE/(1<<(bkeep->level+5)))-1)          //does neccessary bookeeping takes on superblock off whole superlcoks when allocated
        pool->whole_superblocks--;

      bkeep->free_count--;     
      pool->free_objects--;             

     
      
      break;
    }

    i++;
  }

 
  // assert that rv doesn't end up being NULL at this point
  assert(rv != NULL);

  /* Exercise 3: Poison a newly allocated object to detect init errors.
   * Hint: use ALLOC_POISON
   */
    memset(rv, ALLOC_POISON,sizeof(rv));          //poison the return value, **size of return value
   ;
  return rv;
}

static inline
struct superblock_bookkeeping * obj2bkeep (void *ptr) 
{
  uint64_t addr = (uint64_t) ptr;
  addr &= SUPER_BLOCK_MASK;
  return (struct superblock_bookkeeping *) addr;
}

void free(void *ptr) 
{
  
  struct superblock_bookkeeping *bkeep = obj2bkeep(ptr);
  // Your code here.
  //   Be sure to put this back on the free list, and update the
  //   free count.  If you add the final object back to a superblock,
  //   making all objects free, increment whole_superblocks.
  int power = bkeep->level;   // the power is the level of the object
  
  
  struct object *point = (struct object*) ptr;       //need to do this in order to access the object/ do necessary operations
  // adds the object to the free list
  point->next = bkeep->free_list;   
  bkeep->free_list = point;
  //updates the pool and the bookeeping
  bkeep->free_count++;
  levels[power].free_objects++; 
  //poisons the contents after an object is freed, but not the 8 byte pointer
  memset(ptr+8,FREE_POISON,((1<<(power+5)))-8);
 
  if(bkeep->free_count == (SUPER_BLOCK_SIZE/(1<<(bkeep->level+5)))-1)
        levels[power].whole_superblocks++;
  /* Exercise 3: Poison a newly freed object to detect use-after-free errors.
   * Hint: use FREE_POISON.
   */
  



  
 
  if(levels[bkeep->level].whole_superblocks > RESERVE_SUPERBLOCK_THRESHOLD)
  {
    struct superblock_bookkeeping* cur;   // current superblock
    struct superblock_bookkeeping* prev;   // previoud super bloock
    cur = levels[power].next;              // current is the first superblock in the pool

    if(cur->free_count == (SUPER_BLOCK_SIZE/(1<<(bkeep->level+5)))-1)
    {
      // if the first block is empty then unmap it
      levels[power].next = cur->next;
      levels[power].whole_superblocks--;
      levels[power].free_objects -= cur->free_count;
      munmap(cur, SUPER_BLOCK_SIZE);
    
    
    }

    else
    {
      while(cur->next)
      { 
      //continue through the list of superblocks until u find an empty one
        prev = cur;
        cur = cur->next;
        if(cur->free_count == (SUPER_BLOCK_SIZE/(1<<(bkeep->level+5)))-1)
        {       
          //update the linkedlist
          prev->next = cur->next;
          levels[power].whole_superblocks--;
          levels[power].free_objects -= cur->free_count;
          //ummapp the superblock from memory
          munmap(cur, SUPER_BLOCK_SIZE);  
     
          
          break;
        }
      }
    }
  }


 }

// Do NOT touch this - this will catch any attempt to load this into a multi-threaded app
int pthread_create(void __attribute__((unused)) *x, ...) 
{
  exit(-ENOSYS);
}