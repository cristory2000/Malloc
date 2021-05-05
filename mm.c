/*
 ***********************************************************************************
 *                                   mm.c                                          *
 *  Starter package for a 64-bit struct-based explicit free list memory allocator  *
 *                                                                                 *
 *  ********************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

#include "memlib.h"
#include "mm.h"

 /*
 *
 * Each block has header and footer of the form:
 *
 *      63                  4  3  2  1  0
 *      -----------------------------------
 *     | s  s  s  s  ... s  s  0  0  0  a/f
 *      -----------------------------------
 *
 * where s are the meaningful size bits and a/f is set
 * iff the block is allocated. The list has the following form:
 *
 *
 *    begin                                   end
 *    heap                                    heap
 *  +-----------------------------------------------+
 *  | ftr(0:a)   | zero or more usr blks | hdr(0:a) |
 *  +-----------------------------------------------+
 *  |  prologue  |                       | epilogue |
 *  |  block     |                       | block    |
 *
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 *
 */

/*  Empty block
 *  ------------------------------------------------*
 *  |HEADER:    block size   |     |     | alloc bit|
 *  |-----------------------------------------------|
 *  | pointer to prev free block in this size list  |
 *  |-----------------------------------------------|
 *  | pointer to next free block in this size list  |
 *  |-----------------------------------------------|
 *  |FOOTER:    block size   |     |     | alloc bit|
 *  ------------------------------------------------
 */

/*   Allocated block
 *   ------------------------------------------------*
 *   |HEADER:    block size   |     |     | alloc bit|
 *   |-----------------------------------------------|
 *   |               Data                            |
 *   |-----------------------------------------------|
 *   |               Data                            |
 *   |-----------------------------------------------|
 *   |FOOTER:    block size   |     |     | alloc bit|
 *   -------------------------------------------------
 */

/* Basic constants */

typedef uint64_t word_t;

// Word and header size (bytes)
static const size_t wsize = sizeof(word_t);

// Double word size (bytes)
static const size_t dsize = 2 * sizeof(word_t);

/*
  Minimum useable block size (bytes):
  two words for header & footer, two words for payload
*/
static const size_t min_block_size = 4 * sizeof(word_t);

/* Initial heap size (bytes), requires (chunksize % 16 == 0)
*/
static const size_t chunksize = (1 << 12);

// Mask to extract allocated bit from header
static const word_t alloc_mask = 0x1;

/*
 * Assume: All block sizes are a multiple of 16
 * and so can use lower 4 bits for flags
 */
static const word_t size_mask = ~(word_t) 0xF;

/*
  All blocks have both headers and footers

  Both the header and the footer consist of a single word containing the
  size and the allocation flag, where size is the total size of the block,
  including header, (possibly payload), unused space, and footer
*/

typedef struct block block_t;

/* Representation of the header and payload of one block in the heap */
struct block
{
    /* Header contains:
    *  a. size
    *  b. allocation flag
    */
    word_t header;

    union
    {
        struct
        {
            block_t *prev;
            block_t *next;
        } links;
        /*
        * We don't know what the size of the payload will be, so we will
        * declare it as a zero-length array.  This allows us to obtain a
        * pointer to the start of the payload.
        */
        unsigned char data[0];

    /*
     * Payload contains:
     * a. only data if allocated
     * b. pointers to next/previous free blocks if unallocated
     */
    } payload;

    /*
     * We can't declare the footer as part of the struct, since its starting
     * position is unknown
     */
};

/* Global variables */

// Pointer to first block
static block_t *heap_start = NULL;

// Pointer to the first block in the free list
static block_t *free_list_head = NULL;

/* Function prototypes for internal helper routines */

static size_t max(size_t x, size_t y);
static block_t *find_fit(size_t asize);
static block_t *coalesce_block(block_t *block);
static void split_block(block_t *block, size_t asize);

static size_t round_up(size_t size, size_t n);
static word_t pack(size_t size, bool alloc);

static size_t extract_size(word_t header);
static size_t get_size(block_t *block);

static bool extract_alloc(word_t header);
static bool get_alloc(block_t *block);

static void write_header(block_t *block, size_t size, bool alloc);
static void write_footer(block_t *block, size_t size, bool alloc);

static block_t *payload_to_header(void *bp);
static void *header_to_payload(block_t *block);
static word_t *header_to_footer(block_t *block);

static block_t *find_next(block_t *block);
static word_t *find_prev_footer(block_t *block);
static block_t *find_prev(block_t *block);

static bool check_heap();
static void examine_heap();

static block_t *extend_heap(size_t size);
static void insert_block(block_t *free_block);
static void remove_block(block_t *free_block);

/*
 * mm_init - Initialize the memory manager
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    word_t *start = (word_t *)(mem_sbrk(2*wsize));
    if ((ssize_t)start == -1) {
        printf("ERROR: mem_sbrk failed in mm_init, returning %p\n", start);
        return -1;
    }

    /* Prologue footer */
    start[0] = pack(0, true);
    /* Epilogue header */
    start[1] = pack(0, true);

    /* Heap starts with first "block header", currently the epilogue header */
    heap_start = (block_t *) &(start[1]);

    /* Extend the empty heap with a free block of chunksize bytes */
    block_t *free_block = extend_heap(chunksize);
    if (free_block == NULL) {
        printf("ERROR: extend_heap failed in mm_init, returning");
        return -1;
    }

    /* Set the head of the free list to this new free block */
    free_list_head = free_block;
    free_list_head->payload.links.prev = NULL;
    free_list_head->payload.links.next = NULL;

    return 0;
}

/*
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
void *mm_malloc(size_t size)
{
    size_t asize;      // Allocated block size

    if (size == 0) // Ignore spurious request
        return NULL;

    // Too small block
    if (size <= dsize) {
        asize = min_block_size;
    } else {
        // Round up and adjust to meet alignment requirements
        asize = round_up(size + dsize, dsize);
    }

  // TODO: Implement mm_malloc.  You can change or remove any of the above
  // code.  It is included as a suggestion of where to start.
  // You will want to replace this return statement...
  block_t *goodblock = find_fit(asize);
  if(goodblock==NULL)
  {
   goodblock=extend_heap(asize+16);
   if(goodblock==NULL)
   {
     return NULL;
   }
  }
  //size_t goodblocksize = get_size(goodblock);
  write_header(goodblock,get_size(goodblock),true);
  write_footer(goodblock,get_size(goodblock),true);
  split_block(goodblock,asize);

  return  header_to_payload(goodblock);
  //return (void *) (goodblock->payload);

}


/*
 * mm_free - Free a block
 */
void mm_free(void *bp)
{
  ///void *new_bp;
    // if (bp == NULL)
    //     return;

    // TODO: Implement mm_free

    //size_t size=
//set header and footer to free
    write_header(payload_to_header(bp),get_size(payload_to_header(bp)),false);
    write_footer(payload_to_header(bp),get_size(payload_to_header(bp)),false);
    coalesce_block(payload_to_header(bp));




}

/*
 * insert_block - Insert block at the head of the free list (e.g., LIFO policy)
 */
static void insert_block(block_t *free_block)
{
  //if free list is empty
  if(free_list_head== NULL)
  {
    free_block->payload.links.next=NULL;
    free_block->payload.links.prev=NULL;
    free_list_head=free_block;
    return;
  }
  free_list_head->payload.links.prev=free_block;
  free_block->payload.links.next=free_list_head;
  free_block->payload.links.prev=NULL;
  free_list_head=free_block;
	// TODO: Implement insert block on the free list


}

/*
 * remove_block - Remove a free block from the free list
 if(free_list_head->payload.links.prev == NULL && free_list_head->payload.links.next == NULL)
 {
   free_list_head->payload.links.prev=free_block;
   free_block->payload.links.next=free_list_head;
   free_block->payload.links.prev=NULL;
   free_list_head=free_block;
   return;
 }
 */
static void remove_block(block_t *free_block)
{
  //  block_t * prev;
//block_t * next;

  //if nothing in free list
  // if(free_list_head==NULL)
  // {
  //   return;
  // }
  if(free_list_head==free_block && free_block->payload.links.next==NULL )
  {
    free_list_head=NULL;
    return;
  }
  if(free_list_head==free_block)
  {
    free_list_head=free_list_head->payload.links.next;
    free_list_head->payload.links.prev=NULL;
    return;
  }
  if(free_block->payload.links.next==NULL )
  {
    free_block->payload.links.prev->payload.links.next=NULL;
    return;
  }

    free_block->payload.links.next->payload.links.prev=free_block->payload.links.prev;
    free_block->payload.links.prev->payload.links.next= free_block->payload.links.next;
    //curr=NULL;
    return;
  }





/*
 * Finds a free block that of size at least asize
 */
static block_t *find_fit(size_t asize)
{
  block_t *curr=free_list_head;
  block_t *best=NULL;  // TODO: Implement find_fit.  You should start with a simple first-fit policy.
  size_t split;
  //size_t final;
  size_t first;
    // if(free_list_head==NULL)
    // {
    //   return NULL;
    // }
    // if(curr->payload.links.next==NULL)
    // {
    //   size_t isize= get_size(curr);
    //   if(isize>=asize)
    //   {
    //     return curr;
    //   }
    //   return NULL;
    // }
      while(curr!=NULL)
      {
        //bool free = get_alloc(curr);
        size_t size= get_size(curr);
        split=size-asize;
        if(split==0)
        {
          return curr;
        }

        if(split<first && split>=0)
        {
          first = split;
          best = curr;
        }

        // if(size>=asize)
        // {
        //   return curr;
        // }
      curr=curr->payload.links.next;
      }
      // if(best==NULL)
      // {
      //   return NULL;
      // }
      return best;
//
// while(curr!=NULL)
// {
//   size_t size= get_size(curr);
//   if(size>=asize)
//   {
//     return curr;
//   }
// curr=curr->payload.links.next;
// }


/*   for(block=heap_start;  get_size(block)>0 && block != (block_t*) mem_heap_hi(); block=find_next(block))
    {
      bool free = get_alloc(block);
      size_t size= get_size(block);
      if(free==false && size>=asize)
      {
        return block;
      }
    }

    return NULL; // no fit found
*/
}

/*
 * Coalesces current block with previous and next blocks if either or both are unallocated; otherwise the block is not modified.
 * Returns pointer to the coalesced block. After coalescing, the immediate contiguous previous and next blocks must be allocated.
 */
static block_t *coalesce_block(block_t *block)
{
  if(block==NULL)
  {
    return NULL;
  }

  int block_size;//check
  block_t *next= find_next(block);//chec
  word_t *prev_footer = find_prev_footer(block);//check
  block_t *prev;
  bool next_alloc= get_alloc(next);
  bool prev_alloc=get_alloc((block_t*) prev_footer);
  int next_size;
  int prev_size;
  block_t * final=block;


    //front and back are both empty

    if(next_alloc==false && prev_alloc)//just front is empty
    {
      block_size = get_size(block);
      remove_block(next);
      next_size=get_size(next);
      write_header(block,next_size+block_size,false);
      write_footer(block,next_size+block_size,false);
      final=block;

    }
    //back is empty
    else if(prev_alloc==false && next_alloc)
    {
      block_size = get_size(block);
      prev=find_prev(block);
      remove_block(prev);
      prev_size=get_size(prev);
      write_header(prev,prev_size+block_size,false);
      write_footer(prev,prev_size+block_size,false);
      final=prev;


    }
    else if(next_alloc==false && prev_alloc==false)
    {

      block_size = get_size(block);
      prev=find_prev(block);
    next_size=get_size(next);
      remove_block(prev);
      remove_block(next);
        next_size=get_size(next);
        prev_size=get_size(prev);
      write_header(prev,prev_size+block_size+next_size,false);
      write_footer(prev,prev_size+block_size+next_size,false);
      final=prev;


    }
    //back
insert_block(final);
    //front

return final;


}

/*
 * See if new block can be split one to satisfy allocation
 * and one to keep free
 */
static void split_block(block_t *block, size_t asize)//correct
{
  size_t block_size=get_size(block);
  remove_block(block);

  if((block_size-asize)>=min_block_size)
  {
    write_header(block,asize,true);
    write_footer(block,asize,true);

    block_t *block_next=find_next(block);
    write_header(block_next,block_size-asize,false);
    write_footer(block_next,block_size-asize,false);
    insert_block(block_next);

    return;
  }

//return NULL;
  // TODO: Implement block splitting

}


/*
 * Extends the heap with the requested number of bytes, and recreates end header.
 * Returns a pointer to the result of coalescing the newly-created block with previous free block,
 * if applicable, or NULL in failure.
 */
static block_t *extend_heap(size_t size)
{
    void *bp;

    // Allocate an even number of words to maintain alignment
    size = round_up(size, dsize);
    if ((bp = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }

    // bp is a pointer to the new memory block requested

// bp = mem_sbrk(size+16);

    bp=find_prev_footer(bp);

    // writing headers
    write_header((block_t*)bp,size,false);
    //write footer
    write_footer((block_t*)bp,size,false);
    //rrecreate Epilogue
    //word_t* footer = header_to_footer((block_t*) bp);
    //word_t* epilogue = footer+wsize;
    write_header(find_next(bp),0,true);
    // TODO: Implement extend_heap.
    // You will want to replace this return statement...
    bp=coalesce_block(bp);
    //insert_block(bp);
    return  bp ;
}

/******** The remaining content below are helper and debug routines ********/

/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * examine_heap -- Print the heap by iterating through it as an implicit free list.
 */
static void examine_heap() {
  block_t *block;

  /* print to stderr so output isn't buffered and not output if we crash */
  fprintf(stderr, "free_list_head: %p\n", (void *)free_list_head);

  for (block = heap_start; /* first block on heap */
      get_size(block) > 0 && block < (block_t*)mem_heap_hi();
      block = find_next(block)) {

    /* print out common block attributes */
    fprintf(stderr, "%p: %ld %d\t", (void *)block, get_size(block), get_alloc(block));

    /* and allocated/free specific data */
    if (get_alloc(block)) {
      fprintf(stderr, "ALLOCATED\n");
    } else {
      fprintf(stderr, "FREE\tnext: %p, prev: %p\n",
      (void *)block->payload.links.next,
      (void *)block->payload.links.prev);
    }
  }
  fprintf(stderr, "END OF HEAP\n\n");
}


/* check_heap: checks the heap for correctness; returns true if
 *               the heap is correct, and false otherwise.
 */
static bool check_heap()
{

    // Implement a heap consistency checker as needed.

    /* Below is an example, but you will need to write the heap checker yourself. */

    if (!heap_start) {
        printf("NULL heap list pointer!\n");
        return false;
    }

    block_t *curr = heap_start;
    block_t *next;
    block_t *hi = mem_heap_hi();

    while ((next = find_next(curr)) + 1 < hi) {
        word_t hdr = curr->header;
        word_t ftr = *find_prev_footer(next);

        if (hdr != ftr) {
            printf(
                    "Header (0x%016lX) != footer (0x%016lX)\n",
                    hdr, ftr
                  );
            return false;
        }

        curr = next;
    }

    return true;
}


/*
 *****************************************************************************
 * The functions below are short wrapper functions to perform                *
 * bit manipulation, pointer arithmetic, and other helper operations.        *
 *****************************************************************************
 */

/*
 * max: returns x if x > y, and y otherwise.
 */
static size_t max(size_t x, size_t y)
{
    return (x > y) ? x : y;
}


/*
 * round_up: Rounds size up to next multiple of n
 */
static size_t round_up(size_t size, size_t n)
{
    return n * ((size + (n-1)) / n);
}


/*
 * pack: returns a header reflecting a specified size and its alloc status.
 *       If the block is allocated, the lowest bit is set to 1, and 0 otherwise.
 */
static word_t pack(size_t size, bool alloc)
{
    return alloc ? (size | alloc_mask) : size;
}


/*
 * extract_size: returns the size of a given header value based on the header
 *               specification above.
 */
static size_t extract_size(word_t word)
{
    return (word & size_mask);
}


/*
 * get_size: returns the size of a given block by clearing the lowest 4 bits
 *           (as the heap is 16-byte aligned).
 */
static size_t get_size(block_t *block)
{
    return extract_size(block->header);
}


/*
 * extract_alloc: returns the allocation status of a given header value based
 *                on the header specification above.
 */
static bool extract_alloc(word_t word)
{
    return (bool) (word & alloc_mask);
}


/*
 * get_alloc: returns true when the block is allocated based on the
 *            block header's lowest bit, and false otherwise.
 */
static bool get_alloc(block_t *block)
{
    return extract_alloc(block->header);
}


/*
 * write_header: given a block and its size and allocation status,
 *               writes an appropriate value to the block header.
 */
static void write_header(block_t *block, size_t size, bool alloc)
{
    block->header = pack(size, alloc);
}


/*
 * write_footer: given a block and its size and allocation status,
 *               writes an appropriate value to the block footer by first
 *               computing the position of the footer.
 */
static void write_footer(block_t *block, size_t size, bool alloc)
{
    word_t *footerp = header_to_footer(block);
    *footerp = pack(size, alloc);
}


/*
 * find_next: returns the next consecutive block on the heap by adding the
 *            size of the block.
 */
static block_t *find_next(block_t *block)
{
    return (block_t *) ((unsigned char *) block + get_size(block));
}


/*
 * find_prev_footer: returns the footer of the previous block.
 */
static word_t *find_prev_footer(block_t *block)
{
    // Compute previous footer position as one word before the header
    return &(block->header) - 1;
}


/*
 * find_prev: returns the previous block position by checking the previous
 *            block's footer and calculating the start of the previous block
 *            based on its size.
 */
static block_t *find_prev(block_t *block)
{
    word_t *footerp = find_prev_footer(block);
    size_t size = extract_size(*footerp);
    return (block_t *) ((unsigned char *) block - size);
}


/*
 * payload_to_header: given a payload pointer, returns a pointer to the
 *                    corresponding block.
 */
static block_t *payload_to_header(void *bp)
{
    return (block_t *) ((unsigned char *) bp - offsetof(block_t, payload));
}


/*
 * header_to_payload: given a block pointer, returns a pointer to the
 *                    corresponding payload data.
 */
static void *header_to_payload(block_t *block)
{
    return (void *) (block->payload.data);
}


/*
 * header_to_footer: given a block pointer, returns a pointer to the
 *                   corresponding footer.
 */
static word_t *header_to_footer(block_t *block)
{
    return (word_t *) (block->payload.data + get_size(block) - dsize);
}
