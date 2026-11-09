#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#include <malloc.h>
#include <stdio.h>
/**
 * Assumptions:
 * - single-threaded
 * - myfree only receives pointers from mymalloc/mycalloc (or NULL) & no double frees
 * - no alignment beyond what sbrk provides
 * - no splitting or coalescing.
 */

// Include any other headers we need here
#include <assert.h>  // assert
#include <stdbool.h> // bool, true, false
#include <stdint.h>  // intptr_t, INTPTR_MAX, SIZE_MAX
#include <string.h>  // memset
#include <unistd.h>  // sbrk

// NOTE: You should NOT include <stdlib.h> in your final implementation.
// malloc.h turns every call to malloc/calloc/free in this file into a call
// to mymalloc/mycalloc/myfree, so calling them here would recurse forever.

#include <debug.h> // definition of debug_printf

/**
 * Header stored immediately before every chunk of user memory.
 *
 * size - number of usable bytes after the header (size originally
 *        requested from sbrk for this block, excluding the header)
 * next - next block in sbrk order (free or in use); NULL for the tail
 * free - true if the block is available for reuse
 */
typedef struct block
{
  size_t size;
  struct block *next;
  bool free;
} block_t;

#define BLOCK_SIZE sizeof(block_t)

/**
 * The allocator's only global state: the ends of the block list. Keeping a
 * tail pointer makes appending a new block O(1) instead of walking the list.
 * Invariant: head == NULL iff tail == NULL, & tail->next == NULL.
 */
static struct
{
  block_t *head;
  block_t *tail;
} heap = {NULL, NULL};

/**
 * Convert a block header to the user pointer that follows it.
 *
 * b - a valid block header (not NULL)
 * Returns the address of the first usable byte after the header.
 */
static void *block_to_ptr(block_t *b)
{
  assert(b != NULL);
  return (void *)(b + 1); // pointer arithmetic: skips exactly one block_t
}

/**
 * Convert a user pointer back to its block header.
 *
 * ptr - a pointer previously returned by mymalloc (not NULL)
 * Returns the header that sits immediately before ptr.
 */
static block_t *ptr_to_block(void *ptr)
{
  assert(ptr != NULL);
  return ((block_t *)ptr) - 1;
}

/**
 * First-fit search: find the first free block, starting at the head of the
 * list, that can hold at least `size` bytes.
 *
 * size - requested number of bytes (> 0)
 * Returns the matching block, or NULL if no free block is large enough.
 */
static block_t *find_free_block(size_t size)
{
  assert(size > 0);
  for (block_t *cur = heap.head; cur != NULL; cur = cur->next)
  {
    if (cur->free && cur->size >= size)
    {
      return cur;
    }
  }
  return NULL;
}

/**
 * Append a block to the tail of the block list.
 *
 * b - a freshly obtained block whose next field is NULL
 */
static void append_block(block_t *b)
{
  assert(b != NULL && b->next == NULL);
  assert((heap.head == NULL) == (heap.tail == NULL));

  if (heap.tail == NULL)
  {
    heap.head = b;
  }
  else
  {
    heap.tail->next = b;
  }
  heap.tail = b;
}

/**
 * Ask the OS for a new block big enough for `size` user bytes, using a single
 * sbrk call of exactly BLOCK_SIZE + size bytes, & add it to the list.
 *
 * size - requested number of user bytes (> 0)
 * Returns the new (in use) block, or NULL if the request is too large for
 * sbrk's argument type or sbrk fails.
 */
static block_t *request_block(size_t size)
{
  assert(size > 0);

  // sbrk takes a signed intptr_t; refuse requests it cannot represent.
  if (size > (size_t)INTPTR_MAX - BLOCK_SIZE)
  {
    return NULL;
  }

  void *mem = sbrk((intptr_t)(BLOCK_SIZE + size));
  if (mem == (void *)-1)
  {
    return NULL; // out of memory
  }

  block_t *b = (block_t *)mem;
  b->size = size;
  b->next = NULL;
  b->free = false;
  append_block(b);
  return b;
}

/**
 * Allocate `s` bytes of uninitialized memory.
 *
 * s - number of bytes requested
 * Returns a pointer to at least s usable bytes, or NULL if s == 0 or the
 * memory cannot be obtained. Prints "Malloc <s> bytes" for non-zero requests.
 */
void *mymalloc(size_t s)
{
  if (s == 0)
  {
    return NULL; // spec: malloc(0) returns NULL & prints nothing
  }
  debug_printf("Malloc %zu bytes\n", s);

  block_t *b = find_free_block(s);
  if (b != NULL)
  {
    b->free = false; // reuse an existing block, so sbrk is not needed
  }
  else
  {
    b = request_block(s);
    if (b == NULL)
    {
      return NULL; // sbrk failed, or request too large
    }
  }

  assert(!b->free && b->size >= s);
  return block_to_ptr(b);
}
/**
 * Allocate zero-initialized memory for an array of nmemb elements of s bytes.
 *
 * nmemb - number of elements
 * s - size of each element in bytes
 * total - total number of bytes requested (nmemb * s)
 * Returns a pointer to nmemb * s zeroed bytes, or NULL if the product
 * overflows, is zero, or mymalloc fails. Prints "Calloc <nmemb * s> bytes"
 * after mymalloc's own "Malloc ..." line on success.
 */
void *mycalloc(size_t nmemb, size_t s)
{
  if (s != 0 && nmemb > SIZE_MAX / s)
  {
    return NULL; // nmemb * s would overflow size_t
  }
  size_t total = nmemb * s;

  void *p = mymalloc(total);
  if (p == NULL)
  {
    return NULL; // zero-size request, or out of memory
  }
  debug_printf("Calloc %zu bytes\n", total);

  memset(p, 0, total);
  return p;
}

/**
 * Release memory previously returned by mymalloc or mycalloc so that later
 * allocations can reuse it. The memory is not returned to the OS.
 *
 * ptr - pointer returned by mymalloc/mycalloc, or NULL (which is a no-op)
 * Prints "Freed <size> bytes", where size is the block's stored size.
 */
void myfree(void *ptr)
{
  if (ptr == NULL)
  {
    return; // spec: free(NULL) does nothing & prints nothing
  }

  block_t *b = ptr_to_block(ptr);
  assert(!b->free); // catches double frees (& some invalid pointers)

  b->free = true;
  debug_printf("Freed %zu bytes\n", b->size);
}
