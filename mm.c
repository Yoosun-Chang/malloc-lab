/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    "team 4",
    "Yoosun-Chang",
    "yoosun0815@gmail.com",
    "",
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* 기본 상수 */
#define WSIZE 4          
#define DSIZE 8             
#define CHUNKSIZE (1 << 12)

/* 매크로 */
#define MAX(x, y) ((x) > (y) ? (x) : (y)) 
#define PACK(size, alloc) ((size) | (alloc)) 
#define GET(p) (*(unsigned int *)(p)) 
#define PUT(p, val) (*(unsigned int *)(p) = (val)) 
#define GET_SIZE(p) (GET(p) & ~0x7) 
#define GET_ALLOC(p) (GET(p) & 0x1) 
#define HDRP(bp) ((char *)(bp) - WSIZE) 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static void *extend_heap(size_t words);
/* 
 * mm_init - initialize the malloc package.
 * 최초의 가용블록(4words)을 가지고 힙을 생성하고 할당기를 초기화한다. 
 */
int mm_init(void)
{
    char *heap_listp;

    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) 
        return -1;
    PUT(heap_listp, 0);                            
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); 
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); 
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/* 요청받은 words만큼 추가 메모리를 요청한다.*/
static void *extend_heap(size_t words)
{
    char *bp;

    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; 
    if ((long)(bp = mem_sbrk(size)) == -1) 
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));         
    PUT(FTRP(bp), PACK(size, 0));         
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 

    return coalesce(bp); 
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 * 요청한 블록을 반환한다.
 */
void mm_free(void *bp){ 
    size_t size = GET_SIZE(HDRP(bp)); 
    PUT(HDRP(bp),PACK(size,0)); 
    PUT(FTRP(bp), PACK(size,0)); 
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














