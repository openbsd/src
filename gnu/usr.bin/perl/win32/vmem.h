/* vmem.h
 *
 * (c) 1999 Microsoft Corporation. All rights reserved. 
 * Portions (c) 1999 ActiveState Tool Corp, http://www.ActiveState.com/
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 *
 * Knuth's boundary tag algorithm Vol #1, Page 440.
 *
 * Each block in the heap has tag words before and after it,
 *  TAG
 *  block
 *  TAG
 * The size is stored in these tags as a long word, and includes the 8 bytes
 * of overhead that the boundary tags consume.  Blocks are allocated on long
 * word boundaries, so the size is always multiples of long words.  When the
 * block is allocated, bit 0, (the tag bit), of the size is set to 1.  When 
 * a block is freed, it is merged with adjacent free blocks, and the tag bit
 * is set to 0.
 *
 * A linked list is used to manage the free list. The first two long words of
 * the block contain double links.  These links are only valid when the block
 * is freed, therefore space needs to be reserved for them.  Thus, the minimum
 * block size (not counting the tags) is 8 bytes.
 *
 * Since memory allocation may occur on a single threaded, explict locks are
 * provided.
 * 
 */

#ifndef ___VMEM_H_INC___
#define ___VMEM_H_INC___

const long lAllocStart = 0x00010000; /* start at 64K */
const long minBlockSize = sizeof(void*)*2;
const long sizeofTag = sizeof(long);
const long blockOverhead = sizeofTag*2;
const long minAllocSize = minBlockSize+blockOverhead;

typedef BYTE* PBLOCK;	/* pointer to a memory block */

/*
 * Macros for accessing hidden fields in a memory block:
 *
 * SIZE	    size of this block (tag bit 0 is 1 if block is allocated)
 * PSIZE    size of previous physical block
 */

#define SIZE(block)	(*(ULONG*)(((PBLOCK)(block))-sizeofTag))
#define PSIZE(block)	(*(ULONG*)(((PBLOCK)(block))-(sizeofTag*2)))
inline void SetTags(PBLOCK block, long size)
{
    SIZE(block) = size;
    PSIZE(block+(size&~1)) = size;
}

/*
 * Free list pointers
 * PREV	pointer to previous block
 * NEXT	pointer to next block
 */

#define PREV(block)	(*(PBLOCK*)(block))
#define NEXT(block)	(*(PBLOCK*)((block)+sizeof(PBLOCK)))
inline void SetLink(PBLOCK block, PBLOCK prev, PBLOCK next)
{
    PREV(block) = prev;
    NEXT(block) = next;
}
inline void Unlink(PBLOCK p)
{
    PBLOCK next = NEXT(p);
    PBLOCK prev = PREV(p);
    NEXT(prev) = next;
    PREV(next) = prev;
}
inline void AddToFreeList(PBLOCK block, PBLOCK pInList)
{
    PBLOCK next = NEXT(pInList);
    NEXT(pInList) = block;
    SetLink(block, pInList, next);
    PREV(next) = block;
}


/* Macro for rounding up to the next sizeof(long) */
#define ROUND_UP(n)	(((ULONG)(n)+sizeof(long)-1)&~(sizeof(long)-1))
#define ROUND_UP64K(n)	(((ULONG)(n)+0x10000-1)&~(0x10000-1))
#define ROUND_DOWN(n)	((ULONG)(n)&~(sizeof(long)-1))

/*
 * HeapRec - a list of all non-contiguous heap areas
 *
 * Each record in this array contains information about a non-contiguous heap area.
 */

const int maxHeaps = 64;
const long lAllocMax   = 0x80000000; /* max size of allocation */

typedef struct _HeapRec
{
    PBLOCK	base;	/* base of heap area */
    ULONG	len;	/* size of heap area */
} HeapRec;


class VMem
{
public:
    VMem();
    ~VMem();
    virtual void* Malloc(size_t size);
    virtual void* Realloc(void* pMem, size_t size);
    virtual void Free(void* pMem);
    virtual void GetLock(void);
    virtual void FreeLock(void);
    virtual int IsLocked(void);
    virtual long Release(void);
    virtual long AddRef(void);

    inline BOOL CreateOk(void)
    {
	return m_hHeap != NULL;
    };

    void ReInit(void);

protected:
    void Init(void);
    int Getmem(size_t size);
    int HeapAdd(void* ptr, size_t size);
    void* Expand(void* block, size_t size);
    void WalkHeap(void);

    HANDLE		m_hHeap;		    // memory heap for this script
    char		m_FreeDummy[minAllocSize];  // dummy free block
    PBLOCK		m_pFreeList;		    // pointer to first block on free list
    PBLOCK		m_pRover;		    // roving pointer into the free list
    HeapRec		m_heaps[maxHeaps];	    // list of all non-contiguous heap areas 
    int			m_nHeaps;		    // no. of heaps in m_heaps 
    long		m_lAllocSize;		    // current alloc size
    long		m_lRefCount;		    // number of current users
    CRITICAL_SECTION	m_cs;			    // access lock
};

// #define _DEBUG_MEM
#ifdef _DEBUG_MEM
#define ASSERT(f) if(!(f)) DebugBreak();

inline void MEMODS(char *str)
{
    OutputDebugString(str);
    OutputDebugString("\n");
}

inline void MEMODSlx(char *str, long x)
{
    char szBuffer[512];	
    sprintf(szBuffer, "%s %lx\n", str, x);
    OutputDebugString(szBuffer);
}

#define WALKHEAP() WalkHeap()
#define WALKHEAPTRACE() m_pRover = NULL; WalkHeap()

#else

#define ASSERT(f)
#define MEMODS(x)
#define MEMODSlx(x, y)
#define WALKHEAP()
#define WALKHEAPTRACE()

#endif


VMem::VMem()
{
    m_lRefCount = 1;
    BOOL bRet = (NULL != (m_hHeap = HeapCreate(HEAP_NO_SERIALIZE,
				lAllocStart,	/* initial size of heap */
				0)));		/* no upper limit on size of heap */
    ASSERT(bRet);

    InitializeCriticalSection(&m_cs);

    Init();
}

VMem::~VMem(void)
{
    ASSERT(HeapValidate(m_hHeap, HEAP_NO_SERIALIZE, NULL));
    WALKHEAPTRACE();
    DeleteCriticalSection(&m_cs);
    BOOL bRet = HeapDestroy(m_hHeap);
    ASSERT(bRet);
}

void VMem::ReInit(void)
{
    for(int index = 0; index < m_nHeaps; ++index)
	HeapFree(m_hHeap, HEAP_NO_SERIALIZE, m_heaps[index].base);

    Init();
}

void VMem::Init(void)
{   /*
     * Initialize the free list by placing a dummy zero-length block on it.
     * Set the number of non-contiguous heaps to zero.
     */
    m_pFreeList = m_pRover = (PBLOCK)(&m_FreeDummy[minBlockSize]);
    PSIZE(m_pFreeList) = SIZE(m_pFreeList) = 0;
    PREV(m_pFreeList) = NEXT(m_pFreeList) = m_pFreeList;

    m_nHeaps = 0;
    m_lAllocSize = lAllocStart;
}

void* VMem::Malloc(size_t size)
{
    WALKHEAP();

    /*
     * Adjust the real size of the block to be a multiple of sizeof(long), and add
     * the overhead for the boundary tags.  Disallow negative or zero sizes.
     */
    size_t realsize = (size < blockOverhead) ? minAllocSize : (size_t)ROUND_UP(size) + minBlockSize;
    if((int)realsize < minAllocSize || size == 0)
	return NULL;

    /*
     * Start searching the free list at the rover.  If we arrive back at rover without
     * finding anything, allocate some memory from the heap and try again.
     */
    PBLOCK ptr = m_pRover;	/* start searching at rover */
    int loops = 2;		/* allow two times through the loop  */
    for(;;) {
	size_t lsize = SIZE(ptr);
	ASSERT((lsize&1)==0);
	/* is block big enough? */
	if(lsize >= realsize) {	
	    /* if the remainder is too small, don't bother splitting the block. */
	    size_t rem = lsize - realsize;
	    if(rem < minAllocSize) {
		if(m_pRover == ptr)
		    m_pRover = NEXT(ptr);

		/* Unlink the block from the free list. */
		Unlink(ptr);
	    }
	    else {
		/*
		 * split the block
		 * The remainder is big enough to split off into a new block.
		 * Use the end of the block, resize the beginning of the block
		 * no need to change the free list.
		 */
		SetTags(ptr, rem);
		ptr += SIZE(ptr);
		lsize = realsize;
	    }
	    /* Set the boundary tags to mark it as allocated. */
	    SetTags(ptr, lsize | 1);
	    return ((void *)ptr);
	}

	/*
	 * This block was unsuitable.  If we've gone through this list once already without
	 * finding anything, allocate some new memory from the heap and try again.
	 */
	ptr = NEXT(ptr);
	if(ptr == m_pRover) {
	    if(!(loops-- && Getmem(realsize))) {
		return NULL;
	    }
	    ptr = m_pRover;
	}
    }
}

void* VMem::Realloc(void* block, size_t size)
{
    WALKHEAP();

    /* if size is zero, free the block. */
    if(size == 0) {
	Free(block);
	return (NULL);
    }

    /* if block pointer is NULL, do a Malloc(). */
    if(block == NULL)
	return Malloc(size);

    /*
     * Grow or shrink the block in place.
     * if the block grows then the next block will be used if free
     */
    if(Expand(block, size) != NULL)
	return block;

    /*
     * adjust the real size of the block to be a multiple of sizeof(long), and add the
     * overhead for the boundary tags.  Disallow negative or zero sizes.
     */
    size_t realsize = (size < blockOverhead) ? minAllocSize : (size_t)ROUND_UP(size) + minBlockSize;
    if((int)realsize < minAllocSize)
	return NULL;

    /*
     * see if the previous block is free, and is it big enough to cover the new size
     * if merged with the current block.
     */
    PBLOCK ptr = (PBLOCK)block;
    size_t cursize = SIZE(ptr) & ~1;
    size_t psize = PSIZE(ptr);
    if((psize&1) == 0 && (psize + cursize) >= realsize) {
	PBLOCK prev = ptr - psize;
	if(m_pRover == prev)
	    m_pRover = NEXT(prev);

	/* Unlink the next block from the free list. */
	Unlink(prev);

	/* Copy contents of old block to new location, make it the current block. */
	memmove(prev, ptr, cursize);
	cursize += psize;	/* combine sizes */
	ptr = prev;

	size_t rem = cursize - realsize;
	if(rem >= minAllocSize) {
	    /*
	     * The remainder is big enough to be a new block.  Set boundary
	     * tags for the resized block and the new block.
	     */
	    prev = ptr + realsize;
	    /*
	     * add the new block to the free list.
	     * next block cannot be free
	     */
	    SetTags(prev, rem);
	    AddToFreeList(prev, m_pFreeList);
	    cursize = realsize;
        }
	/* Set the boundary tags to mark it as allocated. */
	SetTags(ptr, cursize | 1);
        return ((void *)ptr);
    }

    /* Allocate a new block, copy the old to the new, and free the old. */
    if((ptr = (PBLOCK)Malloc(size)) != NULL) {
	memmove(ptr, block, cursize-minBlockSize);
	Free(block);
    }
    return ((void *)ptr);
}

void VMem::Free(void* p)
{
    WALKHEAP();

    /* Ignore null pointer. */
    if(p == NULL)
	return;

    PBLOCK ptr = (PBLOCK)p;

    /* Check for attempt to free a block that's already free. */
    size_t size = SIZE(ptr);
    if((size&1) == 0) {
	MEMODSlx("Attempt to free previously freed block", (long)p);
	return;
    }
    size &= ~1;	/* remove allocated tag */

    /* if previous block is free, add this block to it. */
    int linked = FALSE;
    size_t psize = PSIZE(ptr);
    if((psize&1) == 0) {
	ptr -= psize;	/* point to previous block */
	size += psize;	/* merge the sizes of the two blocks */
	linked = TRUE;	/* it's already on the free list */
    }

    /* if the next physical block is free, merge it with this block. */
    PBLOCK next = ptr + size;	/* point to next physical block */
    size_t nsize = SIZE(next);
    if((nsize&1) == 0) {
	/* block is free move rover if needed */
	if(m_pRover == next)
	    m_pRover = NEXT(next);

	/* unlink the next block from the free list. */
	Unlink(next);

	/* merge the sizes of this block and the next block. */
	size += nsize;
    }

    /* Set the boundary tags for the block; */
    SetTags(ptr, size);

    /* Link the block to the head of the free list. */
    if(!linked) {
	AddToFreeList(ptr, m_pFreeList);
    }
}

void VMem::GetLock(void)
{
    EnterCriticalSection(&m_cs);
}

void VMem::FreeLock(void)
{
    LeaveCriticalSection(&m_cs);
}

int VMem::IsLocked(void)
{
#if 0
    /* XXX TryEnterCriticalSection() is not available in some versions
     * of Windows 95.  Since this code is not used anywhere yet, we 
     * skirt the issue for now. */
    BOOL bAccessed = TryEnterCriticalSection(&m_cs);
    if(bAccessed) {
	LeaveCriticalSection(&m_cs);
    }
    return !bAccessed;
#else
    ASSERT(0);	/* alarm bells for when somebody calls this */
    return 0;
#endif
}


long VMem::Release(void)
{
    long lCount = InterlockedDecrement(&m_lRefCount);
    if(!lCount)
	delete this;
    return lCount;
}

long VMem::AddRef(void)
{
    long lCount = InterlockedIncrement(&m_lRefCount);
    return lCount;
}


int VMem::Getmem(size_t requestSize)
{   /* returns -1 is successful 0 if not */
    void *ptr;

    /* Round up size to next multiple of 64K. */
    size_t size = (size_t)ROUND_UP64K(requestSize);
    
    /*
     * if the size requested is smaller than our current allocation size
     * adjust up
     */
    if(size < (unsigned long)m_lAllocSize)
	size = m_lAllocSize;

    /* Update the size to allocate on the next request */
    if(m_lAllocSize != lAllocMax)
	m_lAllocSize <<= 1;

    if(m_nHeaps != 0) {
	/* Expand the last allocated heap */
	ptr = HeapReAlloc(m_hHeap, HEAP_REALLOC_IN_PLACE_ONLY|HEAP_ZERO_MEMORY|HEAP_NO_SERIALIZE,
		m_heaps[m_nHeaps-1].base,
		m_heaps[m_nHeaps-1].len + size);
	if(ptr != 0) {
	    HeapAdd(((char*)ptr) + m_heaps[m_nHeaps-1].len, size);
	    return -1;
	}
    }

    /*
     * if we didn't expand a block to cover the requested size
     * allocate a new Heap
     * the size of this block must include the additional dummy tags at either end
     * the above ROUND_UP64K may not have added any memory to include this.
     */
    if(size == requestSize)
	size = (size_t)ROUND_UP64K(requestSize+(sizeofTag*2));

    ptr = HeapAlloc(m_hHeap, HEAP_ZERO_MEMORY|HEAP_NO_SERIALIZE, size);
    if(ptr == 0) {
	MEMODSlx("HeapAlloc failed on size!!!", size);
	return 0;
    }

    HeapAdd(ptr, size);
    return -1;
}

int VMem::HeapAdd(void *p, size_t size)
{   /* if the block can be succesfully added to the heap, returns 0; otherwise -1. */
    int index;

    /* Check size, then round size down to next long word boundary. */
    if(size < minAllocSize)
	return -1;

    size = (size_t)ROUND_DOWN(size);
    PBLOCK ptr = (PBLOCK)p;

    /*
     * Search for another heap area that's contiguous with the bottom of this new area.
     * (It should be extremely unusual to find one that's contiguous with the top).
     */
    for(index = 0; index < m_nHeaps; ++index) {
	if(ptr == m_heaps[index].base + (int)m_heaps[index].len) {
	    /*
	     * The new block is contiguous with a previously allocated heap area.  Add its
	     * length to that of the previous heap.  Merge it with the the dummy end-of-heap
	     * area marker of the previous heap.
	     */
	    m_heaps[index].len += size;
	    break;
	}
    }

    if(index == m_nHeaps) {
	/* The new block is not contiguous.  Add it to the heap list. */
	if(m_nHeaps == maxHeaps) {
	    return -1;	/* too many non-contiguous heaps */
	}
	m_heaps[m_nHeaps].base = ptr;
	m_heaps[m_nHeaps].len = size;
	m_nHeaps++;

	/*
	 * Reserve the first LONG in the block for the ending boundary tag of a dummy
	 * block at the start of the heap area.
	 */
	size -= minBlockSize;
	ptr += minBlockSize;
	PSIZE(ptr) = 1;	/* mark the dummy previous block as allocated */
    }

    /*
     * Convert the heap to one large block.  Set up its boundary tags, and those of
     * marker block after it.  The marker block before the heap will already have
     * been set up if this heap is not contiguous with the end of another heap.
     */
    SetTags(ptr, size | 1);
    PBLOCK next = ptr + size;	/* point to dummy end block */
    SIZE(next) = 1;	/* mark the dummy end block as allocated */

    /*
     * Link the block to the start of the free list by calling free().
     * This will merge the block with any adjacent free blocks.
     */
    Free(ptr);
    return 0;
}


void* VMem::Expand(void* block, size_t size)
{
    /*
     * Adjust the size of the block to be a multiple of sizeof(long), and add the
     * overhead for the boundary tags.  Disallow negative or zero sizes.
     */
    size_t realsize = (size < blockOverhead) ? minAllocSize : (size_t)ROUND_UP(size) + minBlockSize;
    if((int)realsize < minAllocSize || size == 0)
	return NULL;

    PBLOCK ptr = (PBLOCK)block; 

    /* if the current size is the same as requested, do nothing. */
    size_t cursize = SIZE(ptr) & ~1;
    if(cursize == realsize) {
	return block;
    }

    /* if the block is being shrunk, convert the remainder of the block into a new free block. */
    if(realsize <= cursize) {
	size_t nextsize = cursize - realsize;	/* size of new remainder block */
	if(nextsize >= minAllocSize) {
	    /*
	     * Split the block
	     * Set boundary tags for the resized block and the new block.
	     */
	    SetTags(ptr, realsize | 1);
	    ptr += realsize;

	    /*
	     * add the new block to the free list.
	     * call Free to merge this block with next block if free
	     */
	    SetTags(ptr, nextsize | 1);
	    Free(ptr);
	}

	return block;
    }

    PBLOCK next = ptr + cursize;
    size_t nextsize = SIZE(next);

    /* Check the next block for consistency.*/
    if((nextsize&1) == 0 && (nextsize + cursize) >= realsize) {
	/*
	 * The next block is free and big enough.  Add the part that's needed
	 * to our block, and split the remainder off into a new block.
	 */
	if(m_pRover == next)
	    m_pRover = NEXT(next);

	/* Unlink the next block from the free list. */
	Unlink(next);
	cursize += nextsize;	/* combine sizes */

	size_t rem = cursize - realsize;	/* size of remainder */
	if(rem >= minAllocSize) {
	    /*
	     * The remainder is big enough to be a new block.
	     * Set boundary tags for the resized block and the new block.
	     */
	    next = ptr + realsize;
	    /*
	     * add the new block to the free list.
	     * next block cannot be free
	     */
	    SetTags(next, rem);
	    AddToFreeList(next, m_pFreeList);
	    cursize = realsize;
        }
	/* Set the boundary tags to mark it as allocated. */
	SetTags(ptr, cursize | 1);
	return ((void *)ptr);
    }
    return NULL;
}

#ifdef _DEBUG_MEM
#define LOG_FILENAME "P:\\Apps\\Perl\\Result.txt"

void MemoryUsageMessage(char *str, long x, long y, int c)
{
    static FILE* fp = NULL;
    char szBuffer[512];
    if(str) {
	if(!fp)
	    fp = fopen(LOG_FILENAME, "w");
	sprintf(szBuffer, str, x, y, c);
	fputs(szBuffer, fp);
    }
    else {
	fflush(fp);
	fclose(fp);
    }
}

void VMem::WalkHeap(void)
{
    if(!m_pRover) {
	MemoryUsageMessage("VMem heaps used %d\n", m_nHeaps, 0, 0);
    }

    /* Walk all the heaps - verify structures */
    for(int index = 0; index < m_nHeaps; ++index) {
	PBLOCK ptr = m_heaps[index].base;
	size_t size = m_heaps[index].len;
	ASSERT(HeapValidate(m_hHeap, HEAP_NO_SERIALIZE, p));

	/* set over reserved header block */
	size -= minBlockSize;
	ptr += minBlockSize;
	PBLOCK pLast = ptr + size;
	ASSERT(PSIZE(ptr) == 1); /* dummy previous block is allocated */
	ASSERT(SIZE(pLast) == 1); /* dummy next block is allocated */
	while(ptr < pLast) {
	    ASSERT(ptr > m_heaps[index].base);
	    size_t cursize = SIZE(ptr) & ~1;
	    ASSERT((PSIZE(ptr+cursize) & ~1) == cursize);
	    if(!m_pRover) {
		MemoryUsageMessage("Memory Block %08x: Size %08x %c\n", (long)ptr, cursize, (SIZE(p)&1) ? 'x' : ' ');
	    }
	    if(!(SIZE(ptr)&1)) {
		/* this block is on the free list */
		PBLOCK tmp = NEXT(ptr);
		while(tmp != ptr) {
		    ASSERT((SIZE(tmp)&1)==0);
		    if(tmp == m_pFreeList)
			break;
		    ASSERT(NEXT(tmp));
		    tmp = NEXT(tmp);
		}
		if(tmp == ptr) {
		    MemoryUsageMessage("Memory Block %08x: Size %08x free but not in free list\n", (long)ptr, cursize, 0);
		}
	    }
	    ptr += cursize;
	}
    }
    if(!m_pRover) {
	MemoryUsageMessage(NULL, 0, 0, 0);
    }
}
#endif

#endif	/* ___VMEM_H_INC___ */
