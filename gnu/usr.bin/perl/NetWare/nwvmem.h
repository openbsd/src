
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	NWVMem.h
 * DESCRIPTION	:	Memory management for Perl Interpreter on NetWare.
 *                  Watcom's hash table is used to store memory pointers.
 *                  All malloc's, realloc's, free's go through this.
 * Author		:	HYAK, SGP
 * Date			:	Januray 2001.
 *
 */



#ifndef ___NWVMEM_H_INC___
#define ___NWVMEM_H_INC___


#include "win32ish.h"		// For "BOOL", "TRUE" and "FALSE"
#include <nwhashcls.h>		// CW changes
#include <nwmalloc.h>
#include "string.h"



class VMem
{
public:
    VMem();
    virtual ~VMem();
    virtual void* Malloc(size_t size);
    virtual void* Realloc(void* pMem, size_t size);
    virtual void Free(void* pMem);
	virtual void* Calloc(size_t num, size_t size);

protected:
	BOOL					m_dontTouchHashLists;
//	WCValHashTable<void*>*	m_allocList;		
	NWPerlHashList *m_allocList;			// CW changes
};




/*============================================================================================

 Function		:	fnAllocListHash

 Description	:	Hashing function for hash table of memory allocations.

 Parameters 	:	invalue	(IN).

 Returns		:	unsigned.

==============================================================================================*/

unsigned fnAllocListHash(void* const& invalue)
{
    return (((unsigned) invalue & 0x0000ff00) >> 8);
}



/*============================================================================================

 Function		:	fnFreeMemEntry

 Description	:	Called for each outstanding memory allocation at the end of a script run.
					Frees the outstanding allocations

 Parameters 	:	ptr	(IN).
					context (IN)

 Returns		:	Nothing.

==============================================================================================*/

void fnFreeMemEntry(void* ptr, void* context)
{
	VMem* pVMem = (VMem*) context;

	if(ptr && pVMem)
	{
		pVMem->Free(ptr);
		ptr=NULL;
		pVMem = NULL;
		context = NULL;
	}
}



/*============================================================================================

 Function		:	VMem Constructor

 Description	:	

 Parameters 	:	

 Returns		:	

==============================================================================================*/

VMem::VMem()
{
	//Constructor
	m_dontTouchHashLists = FALSE;
	m_allocList = NULL;
	// m_allocList = new WCValHashTable<void*> (fnAllocListHash, 256);  
	m_allocList = new NWPerlHashList();			// CW changes
}



/*============================================================================================

 Function		:	VMem Destructor

 Description	:	

 Parameters 	:	

 Returns		:	

==============================================================================================*/

VMem::~VMem(void)
{
	//Destructor
	m_dontTouchHashLists = TRUE;
	if (m_allocList)
	{
		m_allocList->forAll(fnFreeMemEntry, (void*) this);

		delete m_allocList;
		m_allocList = NULL;
	}
	m_dontTouchHashLists = FALSE;
}



/*============================================================================================

 Function		:	VMem::Malloc

 Description	:	Allocates memory.

 Parameters 	:	size	(IN)	-	Size of memory to be allocated.

 Returns		:	Pointer to the allocated memory block.

==============================================================================================*/

void* VMem::Malloc(size_t size)
{
	void *ptr = NULL;

	if (size <= 0)
		return NULL;

	ptr = malloc(size);
	if (ptr)
	{
		if(m_allocList)
			m_allocList->insert(ptr);
	}
	else
	{
		m_dontTouchHashLists = TRUE;
		if (m_allocList)
		{
			m_allocList->forAll(fnFreeMemEntry, (void*) this);
			delete m_allocList;
			m_allocList = NULL;
		}
		m_dontTouchHashLists = FALSE;

		// Serious error since memory allocation falied. So, exiting...
		ExitThread(TSR_THREAD, 1);
	}

	return(ptr);
}



/*============================================================================================

 Function		:	VMem::Realloc

 Description	:	Reallocates block of memory.

 Parameters 	:	block	(IN)	-	Points to a previously allocated memory block.
					size	(IN)	-	Size of memory to be allocated.

 Returns		:	Pointer to the allocated memory block.

==============================================================================================*/

void* VMem::Realloc(void* block, size_t size)
{
	void *ptr = NULL;

	if (size <= 0)
		return NULL;

	ptr = realloc(block, size);
	if (ptr)
	{
		if (block)
		{
			if (m_allocList)
				m_allocList->remove(block);
		}
		if (m_allocList)
			m_allocList->insert(ptr);
	}
	else
	{
		m_dontTouchHashLists = TRUE;
		if (m_allocList)
		{
			m_allocList->forAll(fnFreeMemEntry, (void*) this);
			delete m_allocList;
			m_allocList = NULL;
		}
		m_dontTouchHashLists = FALSE;

		// Serious error since memory allocation falied. So, exiting...
		ExitThread(TSR_THREAD, 1);
	}

	return(ptr);
}



/*============================================================================================

 Function		:	VMem::Calloc

 Description	:	Allocates and clears memory space for an array of objects.

 Parameters 	:	num	(IN)	-	Specifies the number of objects.
					size	(IN)	-	Size of each object.

 Returns		:	Pointer to the allocated memory block.

==============================================================================================*/

void* VMem::Calloc(size_t num, size_t size)
{
	void *ptr = NULL;

	if (size <= 0)
		return NULL;

	ptr = calloc(num, size);
	if (ptr)
	{
		if(m_allocList)
			m_allocList->insert(ptr);
	}
	else
	{
		m_dontTouchHashLists = TRUE;
		if (m_allocList)
		{
			m_allocList->forAll(fnFreeMemEntry, (void*) this);
			delete m_allocList;
			m_allocList = NULL;
		}
		m_dontTouchHashLists = FALSE;

		// Serious error since memory allocation falied. So, exiting...
		ExitThread(TSR_THREAD, 1);
	}

	return(ptr);
}



/*============================================================================================

 Function		:	VMem::Free

 Description	:	Frees allocated memory.

 Parameters 	:	p	(IN)	-	Points to the allocated memory.

 Returns		:	Nothing.

==============================================================================================*/

void VMem::Free(void* p)
{
	// Final clean up, free all the nodes from the hash list
	if (m_dontTouchHashLists)
	{
		if(p)
		{
			free(p);
			p = NULL;
		}
	}
	else
	{
		if(p && m_allocList)
		{
			if (m_allocList->remove(p))
			{
				free(p);
				p = NULL;
			}
			else
			{
				// If it comes here, that means that the memory pointer is not contained in the hash list.
				// But no need to free now, since if is deleted here, it will result in an abend!!
				// If the memory is still there, it will be cleaned during final cleanup anyway.
			}
		}
	}


	return;
}


#endif		//___NWVMEM_H_INC___

