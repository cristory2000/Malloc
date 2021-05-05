# Malloc

In this lab I wrote a dynamic storage allocator for C programs...my own version of the
malloc and free routines. I created a program that runs with a 89 percent efficiency.
 # Implementation
 The mm.c file we have given you partially implements an allocator using an explicit free list. Your job
is to complete this implementation by filling out mm malloc and mm free. The three main memory
management functions should work as follows:
* mm init: Before calling mm malloc or mm free, the application program (i.e., the trace-driven
driver program that you will use to evaluate your implementation) calls mm init to perform any
necessary initialization, such as allocating the initial heap area. The return value is -1 if there was a
problem in performing the initialization, 0 otherwise.
* mm malloc: The mm malloc routine returns a pointer to an allocated block payload of at least
size bytes. (size t is a type for describing sizes; it’s an unsigned integer that can represent a size
spanning all of memory, so on x86 64 it is a 64-bit unsigned value.) The entire allocated block
should lie within the heap region and should not overlap with any other allocated block.
2
* mm free: The mm free routine frees the block pointed to by ptr. It returns nothing. This routine is
guaranteed to work only when the passed pointer (ptr) was returned by an earlier call to mm malloc
and has not yet been freed. These semantics match the semantics of the corresponding malloc and free
routines in libc. 
