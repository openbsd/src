
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	NWTInfo.c
 * DESCRIPTION	:	Thread-local storage for Perl.
 *					The thread's information is stored in a hashed table that is based on
 *					the lowest 5 bits of the current thread ID.
 * Author		:	SGP, HYAK
 * Date			:	January 2001.
 *
 */



#include "win32ish.h"		// For "BOOL", "TRUE" and "FALSE"
#include "nwtinfo.h"

#ifdef MPK_ON
	#include <mpktypes.h>	
	#include <mpkapis.h>
#else
	#include <nwsemaph.h>
#endif	//MPK_ON

// Number of entries in the hashtable
//
#define NUM_ENTRIES 32  /* 2^5 */


// macro to calculate the hash index for a given Thread ID
//
#define INDEXOF(tid) ((tid) & 0x1f)


// Semaphore to control access to global linked list
//
#ifdef MPK_ON
	static SEMAPHORE g_tinfoSem = NULL;
	static SEMAPHORE g_tCtxSem = NULL;
#else
	static LONG g_tinfoSem = 0L;
	static LONG g_tCtxSem = 0L;
#endif	//MPK_ON

// Hash table of thread information structures
//
ThreadInfo* g_ThreadInfo[NUM_ENTRIES];
ThreadContext* g_ThreadCtx;



/*============================================================================================

 Function		:	fnTerminateThreadInfo

 Description	:	This function undoes fnInitializeThreadInfo; call once per NLM instance.

 Parameters 	:	None.

 Returns		:	Boolean.

==============================================================================================*/

BOOL fnTerminateThreadInfo(void)
{
	int index = 0;

	if (g_tinfoSem)
	{
		#ifdef MPK_ON
			kSemaphoreWait(g_tinfoSem);
		#else
			WaitOnLocalSemaphore(g_tinfoSem);
		#endif	//MPK_ON
		for (index = 0; index < NUM_ENTRIES; index++)
		{
			if (g_ThreadInfo[index] != NULL)
			{
				#ifdef MPK_ON
					kSemaphoreSignal(g_tinfoSem);
				#else
					SignalLocalSemaphore(g_tinfoSem);
				#endif	//MPK_ON
				return FALSE;
			}
		}
		#ifdef MPK_ON
			kSemaphoreFree(g_tinfoSem);
			g_tinfoSem = NULL;
		#else
			CloseLocalSemaphore(g_tinfoSem);
			g_tinfoSem = 0;
		#endif	//MPK_ON
	}

	return TRUE;
}


/*============================================================================================

 Function		:	fnInitializeThreadInfo

 Description	:	Initializes the global ThreadInfo hashtable and semaphore.
					Call once per NLM instance

 Parameters 	:	None.

 Returns		:	Nothing.

==============================================================================================*/

void fnInitializeThreadInfo(void)
{
	int index = 0;

	if (g_tinfoSem)
		return;

	#ifdef MPK_ON
		g_tinfoSem = kSemaphoreAlloc((BYTE *)"threadInfo", 1);
	#else
		g_tinfoSem = OpenLocalSemaphore(1);
	#endif	//MPK_ON
	

	for (index = 0; index < NUM_ENTRIES; index++)
		g_ThreadInfo[index] = NULL;

	return;
}


/*============================================================================================

 Function		:	fnRegisterWithThreadTable

 Description	:	This function registers/adds a new thread with the thread table.

 Parameters 	:	None.

 Returns		:	Boolean.

==============================================================================================*/

BOOL fnRegisterWithThreadTable(void)
{
	ThreadInfo* tinfo = NULL;

	#ifdef MPK_ON
		tinfo = fnAddThreadInfo(labs((int)kCurrentThread()));
	#else
		tinfo = fnAddThreadInfo(GetThreadID());
	#endif	//MPK_ON
	
	if (!tinfo)
		return FALSE;
	else
		return TRUE;
}


/*============================================================================================

 Function		:	fnUnregisterWithThreadTable

 Description	:	This function unregisters/removes a thread from the thread table.

 Parameters 	:	None.

 Returns		:	Boolean.

==============================================================================================*/

BOOL fnUnregisterWithThreadTable(void)
{
	#ifdef MPK_ON
		return fnRemoveThreadInfo(labs((int)kCurrentThread()));
	#else
		return fnRemoveThreadInfo(GetThreadID());
	#endif	//MPK_ON
}


/*============================================================================================

 Function		:	fnAddThreadInfo

 Description	:	Adds a new ThreadInfo for the requested thread.

 Parameters 	:	tid	(IN)	-	ID of the thread.

 Returns		:	Pointer to the ThreadInfo Structure.

==============================================================================================*/

ThreadInfo* fnAddThreadInfo(int tid)
{
	ThreadInfo* tip = NULL;
	int index = 0;

	if (g_tinfoSem)
	{
		#ifdef MPK_ON
			kSemaphoreWait(g_tinfoSem);
		#else
			WaitOnLocalSemaphore(g_tinfoSem);
		#endif	//MPK_ON
	}

	// Add a new one to the beginning of the hash entry
	//
	tip = (ThreadInfo *) malloc(sizeof(ThreadInfo));
	if (tip == NULL)
	{  
		if (g_tinfoSem)
		{
			#ifdef MPK_ON
				kSemaphoreSignal(g_tinfoSem);
			#else
				SignalLocalSemaphore(g_tinfoSem);
			#endif	//MPK_ON
		}
		return NULL;
	}
	index = INDEXOF(tid);     // just take the bottom five bits
	tip->next            =  g_ThreadInfo[index];
	tip->tid             =  tid;
	tip->m_dontTouchHashLists = FALSE;
	tip->m_allocList = NULL;

	g_ThreadInfo [index] =  tip;
	if (g_tinfoSem)
	{
		#ifdef MPK_ON
			kSemaphoreSignal(g_tinfoSem);
		#else
			SignalLocalSemaphore(g_tinfoSem);
		#endif	//MPK_ON
	}

	return tip;
}


/*============================================================================================

 Function		:	fnRemoveThreadInfo

 Description	:	Frees the specified thread info structure and removes it from the
					global linked list.

 Parameters 	:	tid	(IN)	-	ID of the thread.

 Returns		:	Boolean.

==============================================================================================*/

BOOL fnRemoveThreadInfo(int tid)
{
	ThreadInfo* tip = NULL;
	ThreadInfo* prevt = NULL;
	int index = INDEXOF(tid);     // just take the bottom five bits

	if (g_tinfoSem)
	{
		#ifdef MPK_ON
			kSemaphoreWait(g_tinfoSem);
		#else
			WaitOnLocalSemaphore(g_tinfoSem);
		#endif	//MPK_ON
	}

	for (tip = g_ThreadInfo[index]; tip != NULL; tip = tip->next)
	{
		if (tip->tid == tid)
		{
			if (prevt == NULL)
				g_ThreadInfo[index] = tip->next;
			else
				prevt->next = tip->next;

			free(tip);
			tip=NULL;
			if (g_tinfoSem)
			{
				#ifdef MPK_ON
					kSemaphoreSignal(g_tinfoSem);
				#else
					SignalLocalSemaphore(g_tinfoSem);
				#endif	//MPK_ON
			}

			return TRUE;
		}
		prevt = tip;
	}

	if (g_tinfoSem)
	{
		#ifdef MPK_ON
			kSemaphoreSignal(g_tinfoSem);
		#else
			SignalLocalSemaphore(g_tinfoSem);
		#endif	//MPK_ON
	}

	return FALSE;       // entry not found
}


/*============================================================================================

 Function		:	fnGetThreadInfo

 Description	:	Returns the thread info for the given thread ID or NULL if not successful.

 Parameters 	:	tid	(IN)	-	ID of the thread.

 Returns		:	Pointer to the ThreadInfo Structure.

==============================================================================================*/

ThreadInfo* fnGetThreadInfo(int tid)
{
	ThreadInfo*  tip;   
	int index = INDEXOF(tid);     // just take the bottom five bits

	if (g_tinfoSem) {
		#ifdef MPK_ON
			kSemaphoreWait(g_tinfoSem);
		#else
			WaitOnLocalSemaphore(g_tinfoSem);
		#endif	//MPK_ON
	}

	// see if this is already in the table at the index'th offset
	//
	for (tip = g_ThreadInfo[index]; tip != NULL; tip = tip->next)
	{
		if (tip->tid == tid)
		{
			if (g_tinfoSem)
			{
				#ifdef MPK_ON
					kSemaphoreSignal(g_tinfoSem);
				#else
					SignalLocalSemaphore(g_tinfoSem);
				#endif	//MPK_ON
			}
			return tip;
		}
	}

	if (g_tinfoSem)
	{
		#ifdef MPK_ON
			kSemaphoreSignal(g_tinfoSem);
		#else
			SignalLocalSemaphore(g_tinfoSem);
		#endif	//MPK_ON
	}

	return NULL;
}

BOOL fnInsertHashListAddrs(void *addrs, BOOL dontTouchHashList)
{
	ThreadInfo*  tip;   
	int index,tid;

	if (g_tinfoSem) 
	{
		#ifdef MPK_ON
			kSemaphoreWait(g_tinfoSem);
		#else
			WaitOnLocalSemaphore(g_tinfoSem);
		#endif	//MPK_ON
	}

	#ifdef MPK_ON
		tid=index = abs(kCurrentThread());
	#else
		tid=index = GetThreadID();
	#endif	//MPK_ON

	index = INDEXOF(index);     // just take the bottom five bits   

	// see if this is already in the table at the index'th offset
	//
	for (tip = g_ThreadInfo[index]; tip != NULL; tip = tip->next)
	{
		if (tip->tid == tid)
		{
			if (g_tinfoSem)
			{
				#ifdef MPK_ON
					kSemaphoreSignal(g_tinfoSem);
				#else
					SignalLocalSemaphore(g_tinfoSem);
				#endif	//MPK_ON
			}
			tip->m_allocList = addrs;
			tip->m_dontTouchHashLists = dontTouchHashList;
			return TRUE;
		}
	}

	if (g_tinfoSem)
	{
		#ifdef MPK_ON
			kSemaphoreSignal(g_tinfoSem);
		#else
			SignalLocalSemaphore(g_tinfoSem);
		#endif	//MPK_ON
	}

	return FALSE;
}

BOOL fnGetHashListAddrs(void **addrs, BOOL *dontTouchHashList)
{
	ThreadInfo*  tip;   
	int index,tid;   

	if (g_tinfoSem) 
	{
		#ifdef MPK_ON
			kSemaphoreWait(g_tinfoSem);
		#else
			WaitOnLocalSemaphore(g_tinfoSem);
		#endif	//MPK_ON
	}

	#ifdef MPK_ON
		tid=index = abs(kCurrentThread());
	#else
		tid=index = GetThreadID();
	#endif	//MPK_ON

	index = INDEXOF(index);     // just take the bottom five bits 

	// see if this is already in the table at the index'th offset
	//
	for (tip = g_ThreadInfo[index]; tip != NULL; tip = tip->next)
	{
		if (tip->tid == tid)
		{
			if (g_tinfoSem)
			{
				#ifdef MPK_ON
					kSemaphoreSignal(g_tinfoSem);
				#else
					SignalLocalSemaphore(g_tinfoSem);
				#endif	//MPK_ON
			}
			*addrs = tip->m_allocList;
			*dontTouchHashList = tip->m_dontTouchHashLists;
			return TRUE;
		}
	}

	if (g_tinfoSem)
	{
		#ifdef MPK_ON
			kSemaphoreSignal(g_tinfoSem);
		#else
			SignalLocalSemaphore(g_tinfoSem);
		#endif	//MPK_ON
	}

	return FALSE;
}


/*============================================================================================

 Function		:	fnInitializeThreadCtx

 Description	:	Initialises the thread context.

 Parameters 	:	None.

 Returns		:	Nothing.

==============================================================================================*/

long fnInitializeThreadCtx(void)
{
	int index = 0;
	//long tid;

	if (!g_tCtxSem) {
		#ifdef MPK_ON
			g_tCtxSem = kSemaphoreAlloc((BYTE *)"threadCtx", 1);
		#else
			g_tCtxSem = OpenLocalSemaphore(1);
		#endif	//MPK_ON

		g_ThreadCtx =NULL;
	}

	return 0l;
}


/*============================================================================================

 Function		:	fnAddThreadCtx

 Description	:	Add a new thread context.

 Parameters 	:	lTLSIndex	(IN)	-	Index
					t	(IN)	-	void pointer.

 Returns		:	Pointer to ThreadContext structure.

==============================================================================================*/

ThreadContext* fnAddThreadCtx(long lTLSIndex, void *t)
{
	ThreadContext* tip = NULL;
	ThreadContext* temp = NULL;

	if (g_tCtxSem)
	{
		#ifdef MPK_ON
			kSemaphoreWait(g_tCtxSem);
		#else
			WaitOnLocalSemaphore(g_tCtxSem);
		#endif	//MPK_ON
	}

	// add a new one to the beginning of the list
	//
	tip = (ThreadContext *) malloc(sizeof(ThreadContext));
	if (tip == NULL)
	{  
		if (g_tCtxSem)
		{
			#ifdef MPK_ON
				kSemaphoreSignal(g_tCtxSem);
			#else
				SignalLocalSemaphore(g_tCtxSem);
			#endif	//MPK_ON
		}
		return NULL;
	}

	#ifdef MPK_ON
		lTLSIndex = labs(kCurrentThread());
	#else
		lTLSIndex = GetThreadID();
	#endif	//MPK_ON

	tip->next            =  NULL;
	tip->tid             =  lTLSIndex;
	tip->tInfo			 =  t;

	if(g_ThreadCtx==NULL) {
		g_ThreadCtx = tip;
	} else {
		int count=0;
		//Traverse to the end
		temp = g_ThreadCtx;
		while(temp->next != NULL)
		{
			temp = temp->next;
			count++;
		}
		temp->next = tip;
	}

	if (g_tCtxSem)
	{
		#ifdef MPK_ON
			kSemaphoreSignal(g_tCtxSem);
		#else
			SignalLocalSemaphore(g_tCtxSem);
		#endif	//MPK_ON
	}
	return tip;
}


/*============================================================================================

 Function		:	fnRemoveThreadCtx

 Description	:	Removes a thread context.

 Parameters 	:	lTLSIndex	(IN)	-	Index

 Returns		:	Boolean.

==============================================================================================*/

BOOL fnRemoveThreadCtx(long lTLSIndex)
{
	ThreadContext* tip = NULL;
	ThreadContext* prevt = NULL;

	if (g_tCtxSem)
	{
		#ifdef MPK_ON
			kSemaphoreWait(g_tCtxSem);
		#else
			WaitOnLocalSemaphore(g_tCtxSem);
		#endif	//MPK_ON
	}

	#ifdef MPK_ON
		lTLSIndex = labs(kCurrentThread());
	#else
		lTLSIndex = GetThreadID();
	#endif	//MPK_ON

	tip = g_ThreadCtx;
	while(tip) {
		if (tip->tid == lTLSIndex) {
			if (prevt == NULL)
				g_ThreadCtx = tip->next;
			else
				prevt->next = tip->next;

			free(tip);
			tip=NULL;
			if (g_tCtxSem)
			{
				#ifdef MPK_ON
					kSemaphoreSignal(g_tCtxSem);
				#else
					SignalLocalSemaphore(g_tCtxSem);
				#endif	//MPK_ON
			}
			return TRUE;
		}
		prevt = tip;
		tip = tip->next;
	}

	if (g_tCtxSem)
	{
		#ifdef MPK_ON
			kSemaphoreSignal(g_tCtxSem);
		#else
			SignalLocalSemaphore(g_tCtxSem);
		#endif	//MPK_ON
	}

	return FALSE;       // entry not found
}


/*============================================================================================

 Function		:	fnGetThreadCtx

 Description	:	Get a thread context.

 Parameters 	:	lTLSIndex	(IN)	-	Index

 Returns		:	Nothing.

==============================================================================================*/

void* fnGetThreadCtx(long lTLSIndex)
{
	ThreadContext*  tip;   

	if (g_tCtxSem) 
	{
		#ifdef MPK_ON
			kSemaphoreWait(g_tCtxSem);
		#else
			WaitOnLocalSemaphore(g_tCtxSem);
		#endif	//MPK_ON
	}

	#ifdef MPK_ON
		lTLSIndex = labs(kCurrentThread());
	#else
		lTLSIndex = GetThreadID();
	#endif	//MPK_ON

	tip = g_ThreadCtx;
	while(tip) {
		if (tip->tid == lTLSIndex) {
			if (g_tCtxSem)
			{
				#ifdef MPK_ON
					kSemaphoreSignal(g_tCtxSem);
				#else
					SignalLocalSemaphore(g_tCtxSem);
				#endif	//MPK_ON
			}
			return (tip->tInfo);
		}
		tip=tip->next;
	}

	if (g_tCtxSem)
	{
		#ifdef MPK_ON
			kSemaphoreSignal(g_tCtxSem);
		#else
			SignalLocalSemaphore(g_tCtxSem);
		#endif	//MPK_ON
	}

	return NULL;
}

