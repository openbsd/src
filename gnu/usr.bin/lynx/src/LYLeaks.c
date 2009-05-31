/*
 *	Copyright (c) 1994, University of Kansas, All Rights Reserved
 *
 *	This code will be used only if LY_FIND_LEAKS is defined.
 *
 *  Revision History:
 *	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
 *	10-30-97	modified to handle StrAllocCopy() and
 *			  StrAllocCat(). - KW & FM
 */

/*
 *	Disable the overriding of the memory routines for this file.
 */
#define NO_MEMORY_TRACKING

#include <HTUtils.h>
#include <LYexit.h>
#include <LYLeaks.h>
#include <LYUtils.h>
#include <LYGlobalDefs.h>

#ifdef LY_FIND_LEAKS

static AllocationList *ALp_RunTimeAllocations = NULL;

#define LEAK_SUMMARY

#ifdef LEAK_SUMMARY

static long now_allocated = 0;
static long peak_alloced = 0;

static long total_alloced = 0;
static long total_freed = 0;

static long count_mallocs = 0;
static long count_frees = 0;

static void CountMallocs(long size)
{
    ++count_mallocs;
    total_alloced += size;
    now_allocated += size;
    if (peak_alloced < now_allocated)
	peak_alloced = now_allocated;
}

static void CountFrees(long size)
{
    ++count_frees;
    total_freed += size;
    now_allocated -= size;
}

#else
#define CountMallocs() ++count_mallocs
#define CountFrees()		/* nothing */
#endif

/*
 *  Purpose:	Add a new allocation item to the list.
 *  Arguments:		ALp_new The new item to add.
 *  Return Value:	void
 *  Remarks/Portability/Dependencies/Restrictions:
 *		Static function made to make code reusable in projects beyond
 *		Lynx (some might ask why not use HTList).
 *  Revision History:
 *	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
 */
static void AddToList(AllocationList * ALp_new)
{
    /*
     * Just make this the first item in the list.
     */
    ALp_new->ALp_Next = ALp_RunTimeAllocations;
    ALp_RunTimeAllocations = ALp_new;
}

/*
 *  Purpose:	Find the place in the list where vp_find is currently
 *		tracked.
 *  Arguments:		vp_find A pointer to look for in the list.
 *  Return Value:	AllocationList *	Either vp_find's place in the
 *						list or NULL if not found.
 *  Remarks/Portability/Dependencies/Restrictions:
 *		Static function made to make code reusable in projects outside
 *		of Lynx (some might ask why not use HTList).
 *  Revision History:
 *	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
 */
static AllocationList *FindInList(void *vp_find)
{
    AllocationList *ALp_find = ALp_RunTimeAllocations;

    /*
     * Go through the list of allocated pointers until end of list or vp_find
     * is found.
     */
    while (ALp_find != NULL) {
	if (ALp_find->vp_Alloced == vp_find) {
	    break;
	}
	ALp_find = ALp_find->ALp_Next;
    }

    return (ALp_find);
}

/*
 *  Purpose:	Remove the specified item from the list.
 *  Arguments:		ALp_del The item to remove from the list.
 *  Return Value:	void
 *  Remarks/Portability/Dependencies/Restrictions:
 *		Static function made to make code reusable in projects outside
 *		of Lynx (some might ask why not use HTList).
 *  Revision History:
 *	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
 */
static void RemoveFromList(AllocationList * ALp_del)
{
    AllocationList *ALp_findbefore = ALp_RunTimeAllocations;

    /*
     * There is one special case, where the item to remove is the first in the
     * list.
     */
    if (ALp_del == ALp_findbefore) {
	ALp_RunTimeAllocations = ALp_del->ALp_Next;
	return;
    }

    /*
     * Loop through checking all of the next values, if a match don't continue. 
     * Always assume the item will be found.
     */
    while (ALp_findbefore->ALp_Next != ALp_del) {
	ALp_findbefore = ALp_findbefore->ALp_Next;
    }

    /*
     * We are one item before the one to get rid of.  Get rid of it.
     */
    ALp_findbefore->ALp_Next = ALp_del->ALp_Next;
}

/*
 * Make the malloc-sequence available for debugging/tracing.
 */
#ifndef LYLeakSequence
long LYLeakSequence(void)
{
    return count_mallocs;
}
#endif

/*
 *  Purpose:	Print a report of all memory left unallocated by
 *		Lynx code or attempted unallocations on
 *		pointers that are not valid and then free
 *		all unfreed memory.
 *  Arguments:		void
 *  Return Value:	void
 *  Remarks/Portability/Dependencies/Restrictions:
 *		This function should be registered for execution with the
 *		atexit (stdlib.h) function as the first statement
 *		in main.
 *		All output of this function is sent to the file defined in
 *		the header LYLeaks.h (LEAKAGE_SINK).
 */
void LYLeaks(void)
{
    AllocationList *ALp_head;
    size_t st_total = (size_t) 0;
    FILE *Fp_leakagesink;

    if (LYfind_leaks == FALSE)
	return;

    /*
     * Open the leakage sink to take all the output.  Recreate the file each
     * time.  Do nothing if unable to open the file.
     */
    Fp_leakagesink = LYNewTxtFile(LEAKAGE_SINK);
    if (Fp_leakagesink == NULL) {
	return;
    }

    while (ALp_RunTimeAllocations != NULL) {
	/*
	 * Take the head off of the run time allocation list.
	 */
	ALp_head = ALp_RunTimeAllocations;
	ALp_RunTimeAllocations = ALp_head->ALp_Next;

	/*
	 * Print the type of leak/error.  Free off memory when we no longer
	 * need it.
	 */
	if (ALp_head->vp_Alloced == NULL) {
	    /*
	     * If there is realloc information on the bad request, then it was
	     * a bad pointer value in a realloc statement.
	     */
	    fprintf(Fp_leakagesink, "%s.\n",
		    gettext("Invalid pointer detected."));
	    fprintf(Fp_leakagesink, "%s\t%ld\n",
		    gettext("Sequence:"),
		    ALp_head->st_Sequence);
	    fprintf(Fp_leakagesink, "%s\t%p\n",
		    gettext("Pointer:"), ALp_head->vp_BadRequest);

	    /*
	     * Don't free the bad request, it is an invalid pointer.  If the
	     * free source information is empty, we should check the realloc
	     * information too since it can get passed bad pointer values also.
	     */
	    if (ALp_head->SL_memory.cp_FileName == NULL) {
		fprintf(Fp_leakagesink, "%s\t%s\n",
			gettext("FileName:"),
			ALp_head->SL_realloc.cp_FileName);
		fprintf(Fp_leakagesink, "%s\t%d\n",
			gettext("LineCount:"),
			ALp_head->SL_realloc.ssi_LineNumber);
	    } else {
		fprintf(Fp_leakagesink, "%s\t%s\n",
			gettext("FileName:"),
			ALp_head->SL_memory.cp_FileName);
		fprintf(Fp_leakagesink, "%s\t%d\n",
			gettext("LineCount:"),
			ALp_head->SL_memory.ssi_LineNumber);
	    }
	} else {
	    size_t i_counter;
	    char *value = (char *) (ALp_head->vp_Alloced);

	    /*
	     * Increment the count of total memory lost and then print the
	     * information.
	     */
	    st_total += ALp_head->st_Bytes;

	    fprintf(Fp_leakagesink, "%s\n",
		    gettext("Memory leak detected."));
	    fprintf(Fp_leakagesink, "%s\t%ld\n",
		    gettext("Sequence:"),
		    ALp_head->st_Sequence);
	    fprintf(Fp_leakagesink, "%s\t%p\n",
		    gettext("Pointer:"),
		    ALp_head->vp_Alloced);
	    fprintf(Fp_leakagesink, "%s\t",
		    gettext("Contains:"));
	    for (i_counter = 0;
		 i_counter < ALp_head->st_Bytes &&
		 i_counter < MAX_CONTENT_LENGTH;
		 i_counter++) {
		if (isprint(UCH(value[i_counter]))) {
		    fprintf(Fp_leakagesink, "%c", value[i_counter]);
		} else {
		    fprintf(Fp_leakagesink, "|");
		}
	    }
	    fprintf(Fp_leakagesink, "\n");
	    fprintf(Fp_leakagesink, "%s\t%d\n",
		    gettext("ByteSize:"),
		    (int) (ALp_head->st_Bytes));
	    fprintf(Fp_leakagesink, "%s\t%s\n",
		    gettext("FileName:"),
		    ALp_head->SL_memory.cp_FileName);
	    fprintf(Fp_leakagesink, "%s\t%d\n",
		    gettext("LineCount:"),
		    ALp_head->SL_memory.ssi_LineNumber);
	    /*
	     * Give the last time the pointer was realloced if it happened
	     * also.
	     */
	    if (ALp_head->SL_realloc.cp_FileName != NULL) {
		fprintf(Fp_leakagesink, "%s\t%s\n",
			gettext("realloced:"),
			ALp_head->SL_realloc.cp_FileName);
		fprintf(Fp_leakagesink, "%s\t%d\n",
			gettext("LineCount:"),
			ALp_head->SL_realloc.ssi_LineNumber);
	    }
	    fflush(Fp_leakagesink);
	    FREE(ALp_head->vp_Alloced);
	}

	/*
	 * Create a blank line and release the memory held by the item.
	 */
	fprintf(Fp_leakagesink, "\n");
	FREE(ALp_head);
    }

    /*
     * Give a grand total of the leakage.  Close the output file.
     */
    fprintf(Fp_leakagesink, "%s\t%u\n",
	    gettext("Total memory leakage this run:"),
	    (unsigned) st_total);
#ifdef LEAK_SUMMARY
    fprintf(Fp_leakagesink, "%s\t%ld\n", gettext("Peak allocation"), peak_alloced);
    fprintf(Fp_leakagesink, "%s\t%ld\n", gettext("Bytes allocated"), total_alloced);
    fprintf(Fp_leakagesink, "%s\t%ld\n", gettext("Total mallocs"), count_mallocs);
    fprintf(Fp_leakagesink, "%s\t%ld\n", gettext("Total frees"), count_frees);
#endif
    fclose(Fp_leakagesink);

    HTSYS_purge(LEAKAGE_SINK);
#if defined(NCURSES) && defined(HAVE__NC_FREEALL)
    _nc_freeall();
#endif
}

/*
 *  Purpose:	Capture allocations using malloc (stdlib.h) and track
 *		the information in a list.
 *  Arguments:	st_bytes	The size of the allocation requested
 *				in bytes.
 *		cp_File		The file from which the request for
 *				allocation came from.
 *		ssi_Line	The line number in cp_File where the
 *				allocation request came from.
 *  Return Value:	void *	A pointer to the allocated memory or NULL on
 *				failure as per malloc (stdlib.h)
 *  Remarks/Portability/Dependencies/Restrictions:
 *		If no memory is allocated, then no entry is added to the
 *		allocation list.
 *  Revision History:
 *	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
 */
void *LYLeakMalloc(size_t st_bytes, const char *cp_File,
		   const short ssi_Line)
{
    void *vp_malloc;

    if (LYfind_leaks == FALSE)
	return (void *) malloc(st_bytes);

    /*
     * Do the actual allocation.
     */
    vp_malloc = (void *) malloc(st_bytes);
    CountMallocs(st_bytes);

    /*
     * Only on successful allocation do we track any information.
     */
    if (vp_malloc != NULL) {
	/*
	 * Further allocate memory to store the information.  Just return on
	 * failure to allocate more.
	 */
	AllocationList *ALp_new = typecalloc(AllocationList);

	if (ALp_new == NULL) {
	    return (vp_malloc);
	}
	/*
	 * Copy over the relevant information.  There is no need to allocate
	 * more memory for the file name as it is a static string anyhow.
	 */
	ALp_new->st_Sequence = count_mallocs;
	ALp_new->vp_Alloced = vp_malloc;
	ALp_new->st_Bytes = st_bytes;
	ALp_new->SL_memory.cp_FileName = cp_File;
	ALp_new->SL_memory.ssi_LineNumber = ssi_Line;

	/*
	 * Add the new item to the allocation list.
	 */
	AddToList(ALp_new);
    }

    return (vp_malloc);
}

/*
 *  Purpose:	Add information about new allocation to the list,
 *		after a call to malloc or calloc or an equivalent
 *		function which may or may not have already created
 *		a list entry.
 *  Arguments:	vp_malloc	The pointer to newly allocated memory.
 *  Arguments:	st_bytes	The size of the allocation requested
 *				in bytes.
 *		cp_File		The file from which the request for
 *				allocation came from.
 *		ssi_Line	The line number in cp_File where the
 *				allocation request came from.
 *  Return Value:	void *	A pointer to the allocated memory or NULL on
 *				failure.
 *  Remarks/Portability/Dependencies/Restrictions:
 *		If no memory is allocated, then no entry is added to the
 *		allocation list.
 *  Revision History:
 *	1999-02-08	created, modelled after LYLeakMalloc - kw
 */
AllocationList *LYLeak_mark_malloced(void *vp_malloced,
				     size_t st_bytes,
				     const char *cp_File,
				     const short ssi_Line)
{
    AllocationList *ALp_new = NULL;

    if (LYfind_leaks == FALSE)
	return NULL;

    /*
     * The actual allocation has already been done!
     *
     * Only on successful allocation do we track any information.
     */
    if (vp_malloced != NULL) {
	/*
	 * See if there is already an entry.  If so, just update the source
	 * location info.
	 */
	ALp_new = FindInList(vp_malloced);
	if (ALp_new) {
	    ALp_new->SL_memory.cp_FileName = cp_File;
	    ALp_new->SL_memory.ssi_LineNumber = ssi_Line;
	    return (ALp_new);
	}
	/*
	 * Further allocate memory to store the information.  Just return on
	 * failure to allocate more.
	 */
	ALp_new = typecalloc(AllocationList);

	if (ALp_new == NULL) {
	    return (NULL);
	}
	/*
	 * Copy over the relevant information.
	 */
	ALp_new->vp_Alloced = vp_malloced;
	ALp_new->st_Bytes = st_bytes;
	ALp_new->SL_memory.cp_FileName = cp_File;
	ALp_new->SL_memory.ssi_LineNumber = ssi_Line;

	/*
	 * Add the new item to the allocation list.
	 */
	AddToList(ALp_new);
    }

    return (ALp_new);
}

/*
 *  Purpose:	Capture allocations by calloc (stdlib.h) and
 *		save relevant information in a list.
 *  Arguments:	st_number	The number of items to allocate.
 *		st_bytes	The size of each item.
 *		cp_File		The file which wants to allocation.
 *		ssi_Line	The line number in cp_File requesting
 *				the allocation.
 *  Return Value:	void *	The allocated memory, or NULL on failure as
 *				per calloc (stdlib.h)
 *  Remarks/Portability/Dependencies/Restrictions:
 *		If no memory can be allocated, then no entry will be added
 *		to the list.
 *  Revision History:
 *		05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
 */
void *LYLeakCalloc(size_t st_number, size_t st_bytes, const char *cp_File,
		   const short ssi_Line)
{
    void *vp_calloc;

    if (LYfind_leaks == FALSE)
	return (void *) calloc(st_number, st_bytes);

    /*
     * Allocate the requested memory.
     */
    vp_calloc = (void *) calloc(st_number, st_bytes);
    CountMallocs(st_bytes);

    /*
     * Only if the allocation was a success do we track information.
     */
    if (vp_calloc != NULL) {
	/*
	 * Allocate memory for the item to be in the list.  If unable, just
	 * return.
	 */
	AllocationList *ALp_new = typecalloc(AllocationList);

	if (ALp_new == NULL) {
	    return (vp_calloc);
	}

	/*
	 * Copy over the relevant information.  There is no need to allocate
	 * memory for the file name as it is a static string anyway.
	 */
	ALp_new->st_Sequence = count_mallocs;
	ALp_new->vp_Alloced = vp_calloc;
	ALp_new->st_Bytes = (st_number * st_bytes);
	ALp_new->SL_memory.cp_FileName = cp_File;
	ALp_new->SL_memory.ssi_LineNumber = ssi_Line;

	/*
	 * Add the item to the allocation list.
	 */
	AddToList(ALp_new);
    }

    return (vp_calloc);
}

/*
 *  Purpose:	Capture any realloc (stdlib.h) calls in order to
 *		properly keep track of our run time allocation
 *		table.
 *  Arguments:	vp_Alloced	The previously allocated block of
 *				memory to resize.  If NULL,
 *				realloc works just like
 *				malloc.
 *		st_newBytes	The new size of the chunk of memory.
 *		cp_File		The file containing the realloc.
 *		ssi_Line	The line containing the realloc in cp_File.
 *  Return Value:	void *	The new pointer value (could be the same) or
 *				NULL if unable to resize (old block
 *				still exists).
 *  Remarks/Portability/Dependencies/Restrictions:
 *		If unable to resize vp_Alloced, then no change in the
 *		allocation list will be made.
 *		If vp_Alloced is an invalid pointer value, the program will
 *		exit after one last entry is added to the allocation list.
 *  Revision History:
 *	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
 */
void *LYLeakRealloc(void *vp_Alloced,
		    size_t st_newBytes,
		    const char *cp_File,
		    const short ssi_Line)
{
    void *vp_realloc;
    AllocationList *ALp_renew;

    if (LYfind_leaks == FALSE)
	return (void *) realloc(vp_Alloced, st_newBytes);

    /*
     * If we are asked to resize a NULL pointer, this is just a malloc call.
     */
    if (vp_Alloced == NULL) {
	return (LYLeakMalloc(st_newBytes, cp_File, ssi_Line));
    }

    /*
     * Find the current vp_Alloced block in the list.  If NULL, this is an
     * invalid pointer value.
     */
    ALp_renew = FindInList(vp_Alloced);
    if (ALp_renew == NULL) {
	/*
	 * Track the invalid pointer value and then exit.  If unable to
	 * allocate, just exit.
	 */
	auto AllocationList *ALp_new = typecalloc(AllocationList);

	if (ALp_new == NULL) {
	    exit_immediately(EXIT_FAILURE);
	}

	/*
	 * Set the information up; no need to allocate file name since it is a
	 * static string.
	 */
	ALp_new->vp_Alloced = NULL;
	ALp_new->vp_BadRequest = vp_Alloced;
	ALp_new->SL_realloc.cp_FileName = cp_File;
	ALp_new->SL_realloc.ssi_LineNumber = ssi_Line;

	/*
	 * Add the item to the list.  Exit.
	 */
	AddToList(ALp_new);
	exit_immediately(EXIT_FAILURE);
    }

    /*
     * Perform the resize.  If not NULL, record the information.
     */
    vp_realloc = (void *) realloc(vp_Alloced, st_newBytes);
    CountMallocs(st_newBytes);
    CountFrees(ALp_renew->st_Bytes);

    if (vp_realloc != NULL) {
	ALp_renew->st_Sequence = count_mallocs;
	ALp_renew->vp_Alloced = vp_realloc;
	ALp_renew->st_Bytes = st_newBytes;

	/*
	 * Update the realloc information, too.  No need to allocate file name,
	 * static string.
	 */
	ALp_renew->SL_realloc.cp_FileName = cp_File;
	ALp_renew->SL_realloc.ssi_LineNumber = ssi_Line;
    }

    return (vp_realloc);
}

/*
 *  Purpose:	Add information about reallocated memory to the list,
 *		after a call to realloc or an equivalent
 *		function which has not already created or updated
 *		a list entry.
 *  Arguments:	ALp_old		List entry for previously allocated
 *				block of memory to resize.  If NULL,
 *				mark_realloced works just like
 *				mark_malloced.
 *		vp_realloced	The new pointer, after resizing.
 *		st_newBytes	The new size of the chunk of memory.
 *		cp_File		The file to record.
 *		ssi_Line	The line to record.
 *  Return Value:		Pointer to new or updated list entry
 *				for this memory block.
 *				NULL on allocation error.
 *  Revision History:
 *	1999-02-11	created kw
 */
#if defined(LY_FIND_LEAKS) && defined(LY_FIND_LEAKS_EXTENDED)
static AllocationList *mark_realloced(AllocationList * ALp_old, void *vp_realloced,
				      size_t st_newBytes,
				      const char *cp_File,
				      const short ssi_Line)
{
    /*
     * If there is no list entry for the old allocation, treat this as if a new
     * allocation had happened.
     */
    if (ALp_old == NULL) {
	return (LYLeak_mark_malloced(vp_realloced, st_newBytes, cp_File, ssi_Line));
    }

    /*
     * ALp_old represents the memory block before reallocation.  Assume that if
     * we get here, there isn't yet a list entry for the new, possibly
     * different, address after realloc, that is our list hasn't been updated -
     * so we're going to do that now.
     */

    if (vp_realloced != NULL) {
	ALp_old->vp_Alloced = vp_realloced;
	ALp_old->st_Bytes = st_newBytes;
	ALp_old->SL_realloc.cp_FileName = cp_File;
	ALp_old->SL_realloc.ssi_LineNumber = ssi_Line;
    }

    return (ALp_old);
}
#endif /* not LY_FIND_LEAKS and LY_FIND_LEAKS_EXTENDED */

/*
 *  Purpose:	Capture all requests to free information and also
 *		remove items from the allocation list.
 *  Arguments:	vp_Alloced	The memory to free.
 *		cp_File		The file calling free.
 *		ssi_Line	The line of cp_File calling free.
 *  Return Value:	void
 *  Remarks/Portability/Dependencies/Restrictions:
 *		If the pointer value is invalid, then an item will be added
 *		to the list and nothing else is done.
 *		I really like the name of this function and one day hope
 *		that Lynx is Leak Free.
 *  Revision History:
 *	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
 */
void LYLeakFree(void *vp_Alloced,
		const char *cp_File,
		const short ssi_Line)
{
    AllocationList *ALp_free;

    if (LYfind_leaks == FALSE) {
	free(vp_Alloced);
	return;
    }

    /*
     * Find the pointer in the allocated list.  If not found, bad pointer.  If
     * found, free list item and vp_Allloced.
     */
    ALp_free = FindInList(vp_Alloced);
    if (ALp_free == NULL) {
	/*
	 * Create the final entry before exiting marking this error.  If unable
	 * to allocate more memory just exit.
	 */
	AllocationList *ALp_new = typecalloc(AllocationList);

	if (ALp_new == NULL) {
	    exit_immediately(EXIT_FAILURE);
	}

	/*
	 * Set up the information, no memory need be allocated for the file
	 * name since it is a static string.
	 */
	ALp_new->vp_Alloced = NULL;
	ALp_new->vp_BadRequest = vp_Alloced;
	ALp_new->SL_memory.cp_FileName = cp_File;
	ALp_new->SL_memory.ssi_LineNumber = ssi_Line;

	/*
	 * Add the entry to the list and then return.
	 */
	AddToList(ALp_new);
	return;
    } else {
	/*
	 * Free off the memory.  Take entry out of allocation list.
	 */
	CountFrees(ALp_free->st_Bytes);
	RemoveFromList(ALp_free);
	FREE(ALp_free);
	FREE(vp_Alloced);
    }
}

/*
 *  Allocates a new copy of a string, and returns it.
 *  Tracks allocations by using other LYLeakFoo functions.
 *  Equivalent to HTSACopy in HTUtils.c - KW
 */
char *LYLeakSACopy(char **dest,
		   const char *src,
		   const char *cp_File,
		   const short ssi_Line)
{
    if (src != NULL && src == *dest) {
	CTRACE((tfp,
		"LYLeakSACopy: *dest equals src, contains \"%s\"\n",
		src));
	return *dest;
    }
    if (*dest) {
	LYLeakFree(*dest, cp_File, ssi_Line);
	*dest = NULL;
    }
    if (src) {
	*dest = (char *) LYLeakMalloc(strlen(src) + 1, cp_File, ssi_Line);
	if (*dest == NULL)
	    outofmem(__FILE__, "LYLeakSACopy");
	strcpy(*dest, src);
    }
    return *dest;
}

/*
 *  String Allocate and Concatenate.
 *  Tracks allocations by using other LYLeakFoo functions.
 *  Equivalent to HTSACat in HTUtils.c - KW
 */
char *LYLeakSACat(char **dest,
		  const char *src,
		  const char *cp_File,
		  const short ssi_Line)
{
    if (src && *src) {
	if (src == *dest) {
	    CTRACE((tfp,
		    "LYLeakSACat:  *dest equals src, contains \"%s\"\n",
		    src));
	    return *dest;
	}
	if (*dest) {
	    int length = strlen(*dest);

	    *dest = (char *) LYLeakRealloc(*dest,
					   (length + strlen(src) + 1),
					   cp_File,
					   ssi_Line);
	    if (*dest == NULL)
		outofmem(__FILE__, "LYLeakSACat");
	    strcpy(*dest + length, src);
	} else {
	    *dest = (char *) LYLeakMalloc((strlen(src) + 1),
					  cp_File,
					  ssi_Line);
	    if (*dest == NULL)
		outofmem(__FILE__, "LYLeakSACat");
	    strcpy(*dest, src);
	}
    }
    return *dest;
}

#if defined(LY_FIND_LEAKS) && defined(LY_FIND_LEAKS_EXTENDED)

const char *leak_cp_File_hack = __FILE__;
short leak_ssi_Line_hack = __LINE__;

/*
 * Purpose:	A wrapper around StrAllocVsprintf (the workhorse of
 *		HTSprintf/HTSprintf0, implemented in HTString.c) that
 *		tries to make sure that our allocation list is always
 *		properly updated, whether StrAllocVsprintf itself was
 *		compiled with memory tracking or not (or even a mixture,
 *		like tracking the freeing but not the new allocation).
 *		Some source files can be compiled with LY_FIND_LEAKS_EXTENDED
 *		in effect while others only have LY_FIND_LEAKS in effect,
 *		and as long as HTString.c is complied with memory tracking
 *		(of either kind) string objects allocated by HTSprintf/
 *		HTSprintf0 (or otherwise) can be passed around among them and
 *		manipulated both ways.
 *  Arguments:	dest		As for StrAllocVsprintf.
 *		cp_File		The source file of the caller (i.e. the
 *				caller of HTSprintf/HTSprintf0, hopefully).
 *		ssi_Line	The line of cp_File calling.
 *		inuse,fmt,ap	As for StrAllocVsprintf.
 *  Return Value:	The char pointer to resulting string, as set
 *			by StrAllocVsprintf, or
 *			NULL if dest==0 (wrong use!).
 *  Remarks/Portability/Dependencies/Restrictions:
 *		The price for generality is severe inefficiency: several
 *		list lookups are done to be on the safe side.
 *		We don't get he real allocation size, only a minimum based
 *		on the string length of the result.  So the amount of memory
 *		leakage may get underestimated.
 *		If *dest is an invalid pointer value on entry (i.e. was not
 *		tracked), the program will exit after one last entry is added
 *		to the allocation list.
 *		If StrAllocVsprintf fails to return a valid string via the
 *		indirect string pointer (its first parameter), invalid memory
 *		access will result and the program will probably terminate
 *		with a signal.  This can happen if, on entry, *dest is NULL
 *		and fmt is empty or NULL, so just Don't Do That.
 *  Revision History:
 *	1999-02-11	created kw
 *	1999-10-15	added comments kw
 */
static char *LYLeakSAVsprintf(char **dest,
			      const char *cp_File,
			      const short ssi_Line,
			      size_t inuse,
			      const char *fmt,
			      va_list * ap)
{
    AllocationList *ALp_old;
    void *vp_oldAlloced;

    const char *old_cp_File = __FILE__;
    short old_ssi_Line = __LINE__;

    if (!dest)
	return NULL;

    if (LYfind_leaks == FALSE) {
	StrAllocVsprintf(dest, inuse, fmt, ap);
	return (*dest);
    }

    vp_oldAlloced = *dest;
    if (!vp_oldAlloced) {
	StrAllocVsprintf(dest, inuse, fmt, ap);
	LYLeak_mark_malloced(*dest, strlen(*dest) + 1, cp_File, ssi_Line);
	return (*dest);
    } else {
	void *vp_realloced;

	ALp_old = FindInList(vp_oldAlloced);
	if (ALp_old == NULL) {
	    /*
	     * Track the invalid pointer value and then exit.  If unable to
	     * allocate, just exit.
	     */
	    auto AllocationList *ALp_new = typecalloc(AllocationList);

	    if (ALp_new == NULL) {
		exit_immediately(EXIT_FAILURE);
	    }

	    /*
	     * Set the information up; no need to allocate file name since it
	     * is a static string.
	     */
	    ALp_new->vp_Alloced = NULL;
	    ALp_new->vp_BadRequest = vp_oldAlloced;
	    ALp_new->SL_realloc.cp_FileName = cp_File;
	    ALp_new->SL_realloc.ssi_LineNumber = ssi_Line;

	    /*
	     * Add the item to the list.  Exit.
	     */
	    AddToList(ALp_new);
	    exit_immediately(EXIT_FAILURE);
	}

	old_cp_File = ALp_old->SL_memory.cp_FileName;
	old_ssi_Line = ALp_old->SL_memory.ssi_LineNumber;
	/*
	 * DO THE REAL WORK, by calling StrAllocVsprintf.  If result is not
	 * NULL, record the information.
	 */
	StrAllocVsprintf(dest, inuse, fmt, ap);
	vp_realloced = (void *) *dest;
	if (vp_realloced != NULL) {
	    AllocationList *ALp_new = FindInList(vp_realloced);

	    if (!ALp_new) {
		/* Look up again, list may have changed! - kw */
		ALp_old = FindInList(vp_oldAlloced);
		if (ALp_old == NULL) {
		    LYLeak_mark_malloced(*dest, strlen(*dest) + 1, cp_File, ssi_Line);
		    return (*dest);
		}
		mark_realloced(ALp_old, *dest, strlen(*dest) + 1, cp_File, ssi_Line);
		return (*dest);
	    }
	    if (vp_realloced == vp_oldAlloced) {
		ALp_new->SL_memory.cp_FileName = old_cp_File;
		ALp_new->SL_memory.ssi_LineNumber = old_ssi_Line;
		ALp_new->SL_realloc.cp_FileName = cp_File;
		ALp_new->SL_realloc.ssi_LineNumber = ssi_Line;
		return (*dest);
	    }
	    /* Look up again, list may have changed! - kw */
	    ALp_old = FindInList(vp_oldAlloced);
	    if (ALp_old == NULL) {
		ALp_new->SL_memory.cp_FileName = old_cp_File;
		ALp_new->SL_memory.ssi_LineNumber = old_ssi_Line;
		ALp_new->SL_realloc.cp_FileName = cp_File;
		ALp_new->SL_realloc.ssi_LineNumber = ssi_Line;
	    } else {
		ALp_new->SL_memory.cp_FileName = old_cp_File;
		ALp_new->SL_memory.ssi_LineNumber = old_ssi_Line;
		ALp_new->SL_realloc.cp_FileName = cp_File;
		ALp_new->SL_realloc.ssi_LineNumber = ssi_Line;
	    }
	}
	return (*dest);
    }
}

/* Note: the following may need updating if HTSprintf in HTString.c
 * is changed. - kw */
#ifdef ANSI_VARARGS
static char *LYLeakHTSprintf(char **pstr, const char *fmt,...)
#else
static char *LYLeakHTSprintf(va_alist)
    va_dcl
#endif
{
    char *str;
    size_t inuse = 0;
    va_list ap;

    LYva_start(ap, fmt);
    {
#ifndef ANSI_VARARGS
	char **pstr = va_arg(ap, char **);
	const char *fmt = va_arg(ap, const char *);
#endif
	if (pstr != 0 && *pstr != 0)
	    inuse = strlen(*pstr);
	str = LYLeakSAVsprintf(pstr, leak_cp_File_hack, leak_ssi_Line_hack,
			       inuse, fmt, &ap);
    }
    va_end(ap);
    return str;
}

/* Note: the following may need updating if HTSprintf0 in HTString.c
 * is changed. - kw */
#ifdef ANSI_VARARGS
static char *LYLeakHTSprintf0(char **pstr, const char *fmt,...)
#else
static char *LYLeakHTSprintf0(va_alist)
    va_dcl
#endif
{
    char *str;
    va_list ap;

    LYva_start(ap, fmt);
    {
#ifndef ANSI_VARARGS
	char **pstr = va_arg(ap, char **);
	const char *fmt = va_arg(ap, const char *);
#endif
	str = LYLeakSAVsprintf(pstr, leak_cp_File_hack, leak_ssi_Line_hack,
			       0, fmt, &ap);
    }
    va_end(ap);
    return str;
}

/*
 * HTSprintf and HTSprintf0 will be defined such that they effectively call one
 * of the following two functions that store away a copy to the File & Line
 * info in temporary hack variables, and then call the real function (which is
 * returned here as a function pointer) to the regular HTSprintf/HTSprintf0
 * arguments.  It's probably a bit inefficient, but that shouldn't be
 * noticeable compared to all the time that memory tracking takes up for list
 * traversal.  - kw
 */
HTSprintflike *Get_htsprintf_fn(const char *cp_File,
				const short ssi_Line)
{
    leak_cp_File_hack = cp_File;
    leak_ssi_Line_hack = ssi_Line;
    return &LYLeakHTSprintf;
}

HTSprintflike *Get_htsprintf0_fn(const char *cp_File,
				 const short ssi_Line)
{
    leak_cp_File_hack = cp_File;
    leak_ssi_Line_hack = ssi_Line;
    return &LYLeakHTSprintf0;
}

#endif /* LY_FIND_LEAKS and LY_FIND_LEAKS_EXTENDED */
#else
/* Standard C forbids an empty file */
void no_leak_checking(void);
void no_leak_checking(void)
{
}
#endif /* LY_FIND_LEAKS */
