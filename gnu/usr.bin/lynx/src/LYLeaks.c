/*
**	Copyright (c) 1994, University of Kansas, All Rights Reserved
**
**	This code will be used only if LY_FIND_LEAKS is defined.
*/

/*
**	Disable the overriding of the memory routines for this file.
*/
#define NO_MEMORY_TRACKING

#include <HTUtils.h>
#include <LYexit.h>
#include <LYLeaks.h>
#include <LYUtils.h>

PRIVATE AllocationList *ALp_RunTimeAllocations = NULL;

PRIVATE void AddToList PARAMS((
	AllocationList *	ALp_new));
PRIVATE AllocationList *FindInList PARAMS((
	void *			vp_find));
PRIVATE void RemoveFromList PARAMS((
	AllocationList *	ALp_del));

/*
**  Purpose:	Print a report of all memory left unallocated by
**		Lynx code or attempted unallocations on
**		pointers that are not valid and then free
**		all unfreed memory.
**  Arguments:		void
**  Return Value:	void
**  Remarks/Portability/Dependencies/Restrictions:
**		This function should be registered for execution with the
**		atexit (stdlib.h) function as the first statement
**		in main.
**		All output of this function is sent to the file defined in
**		the header LYLeaks.h (LEAKAGE_SINK).
**  Revision History:
**	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
**	10-30-97	modified to handle StrAllocCopy() and
**			  StrAllocCat(). - KW & FM
*/
PUBLIC void LYLeaks NOARGS
{
    AllocationList *ALp_head;
    size_t st_total = (size_t)0;
    FILE *Fp_leakagesink;

    /*
     *	Open the leakage sink to take all the output.
     *	Recreate the file each time.
     *	Do nothing if unable to open the file.
     */
    Fp_leakagesink = LYNewTxtFile(LEAKAGE_SINK);
    if (Fp_leakagesink == NULL) {
	return;
    }

    while (ALp_RunTimeAllocations != NULL) {
	/*
	 *  Take the head off of the run time allocation list.
	 */
	ALp_head = ALp_RunTimeAllocations;
	ALp_RunTimeAllocations = ALp_head->ALp_Next;

	/*
	 *  Print the type of leak/error.
	 *  Free off memory when we no longer need it.
	 */
	if (ALp_head->vp_Alloced == NULL) {
	    /*
	     *	If there is realloc information on the
	     *	bad request, then it was a bad pointer
	     *	value in a realloc statement.
	     */
	    fprintf(Fp_leakagesink, "%s.\n",
		    gettext("Invalid pointer detected."));
	    fprintf(Fp_leakagesink, "%s\t%p\n",
		    gettext("Pointer:"), ALp_head->vp_BadRequest);

	    /*
	     *	Don't free the bad request, it is an invalid pointer.
	     *	If the free source information is empty, we
	     *	should check the realloc information
	     *	too since it can get passed bad pointer
	     *	values also.
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
	    char *value = (char *)(ALp_head->vp_Alloced);

	    /*
	     *	Increment the count of total memory lost and
	     *	then print the information.
	     */
	    st_total += ALp_head->st_Bytes;

	    fprintf(Fp_leakagesink, "%s\n",
		    gettext("Memory leak detected."));
	    fprintf(Fp_leakagesink, "%s\t%p\n",
		    gettext("Pointer:"),
		    ALp_head->vp_Alloced);
	    fprintf(Fp_leakagesink, "%s:\t",
		    gettext("Contains:"));
	    for (i_counter = 0;
		 i_counter < ALp_head->st_Bytes &&
		 i_counter < MAX_CONTENT_LENGTH;
		 i_counter++) {
		if (isprint(value[i_counter])) {
		    fprintf(Fp_leakagesink, "%c", value[i_counter]);
		} else {
		    fprintf(Fp_leakagesink, "|");
		}
	    }
	    fprintf(Fp_leakagesink, "\n");
	    FREE(ALp_head->vp_Alloced);
	    fprintf(Fp_leakagesink, "%s\t%d\n",
				    gettext("ByteSize:"),
				    (int)(ALp_head->st_Bytes));
	    fprintf(Fp_leakagesink, "%s\t%s\n",
				    gettext("FileName:"),
				    ALp_head->SL_memory.cp_FileName);
	    fprintf(Fp_leakagesink, "%s\t%d\n",
				    gettext("LineCount:"),
				    ALp_head->SL_memory.ssi_LineNumber);
	    /*
	     *	Give the last time the pointer was realloced
	     *	if it happened also.
	     */
	    if (ALp_head->SL_realloc.cp_FileName != NULL) {
		fprintf(Fp_leakagesink, "%s\t%s\n",
			gettext("realloced:"),
			ALp_head->SL_realloc.cp_FileName);
		fprintf(Fp_leakagesink, "%s\t%d\n",
			gettext("LineCount:"),
			ALp_head->SL_realloc.ssi_LineNumber);
	    }
	}

	/*
	 *  Create a blank line and release the memory held
	 *  by the item.
	 */
	fprintf(Fp_leakagesink, "\n");
	FREE(ALp_head);
    }

    /*
     *	Give a grand total of the leakage.
     *	Close the output file.
     */
    fprintf(Fp_leakagesink, "\n%s\t%u\n",
	    gettext("Total memory leakage this run:"),
	    (unsigned)st_total);
    fclose(Fp_leakagesink);

    HTSYS_purge(LEAKAGE_SINK);
}

/*
**  Purpose:	Capture allocations using malloc (stdlib.h) and track
**		the information in a list.
**  Arguments:	st_bytes	The size of the allocation requested
**				in bytes.
**		cp_File		The file from which the request for
**				allocation came from.
**		ssi_Line	The line number in cp_File where the
**				allocation request came from.
**  Return Value:	void *	A pointer to the allocated memory or NULL on
**				failure as per malloc (stdlib.h)
**  Remarks/Portability/Dependencies/Restrictions:
**		If no memory is allocated, then no entry is added to the
**		allocation list.
**  Revision History:
**	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
*/
PUBLIC void *LYLeakMalloc ARGS3(
	size_t,		st_bytes,
	CONST char *,	cp_File,
	CONST short,	ssi_Line)
{
    /*
     *	Do the actual allocation.
     */
    void *vp_malloc = (void *)malloc(st_bytes);

    /*
     *	Only on successful allocation do we track any information.
     */
    if (vp_malloc != NULL) {
	/*
	 *  Further allocate memory to store the information.
	 *  Just return on failure to allocate more.
	 */
	AllocationList *ALp_new =
			(AllocationList *)calloc(1, sizeof(AllocationList));

	if (ALp_new == NULL) {
	    return(vp_malloc);
	}
	/*
	 *  Copy over the relevant information.
	 *  There is no need to allocate more memory for the
	 *  file name as it is a static string anyhow.
	 */
	ALp_new->vp_Alloced = vp_malloc;
	ALp_new->st_Bytes = st_bytes;
	ALp_new->SL_memory.cp_FileName = cp_File;
	ALp_new->SL_memory.ssi_LineNumber = ssi_Line;

	/*
	 *  Add the new item to the allocation list.
	 */
	AddToList(ALp_new);
    }

    return(vp_malloc);
}

/*
**  Purpose:	Capture allocations by calloc (stdlib.h) and
**		save relevant information in a list.
**  Arguments:	st_number	The number of items to allocate.
**		st_bytes	The size of each item.
**		cp_File		The file which wants to allocation.
**		ssi_Line	The line number in cp_File requesting
**				the allocation.
**  Return Value:	void *	The allocated memory, or NULL on failure as
**				per calloc (stdlib.h)
**  Remarks/Portability/Dependencies/Restrictions:
**		If no memory can be allocated, then no entry will be added
**		to the list.
**  Revision History:
**		05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
*/
PUBLIC void *LYLeakCalloc ARGS4(
	size_t,		st_number,
	size_t,		st_bytes,
	CONST char *,	cp_File,
	CONST short,	ssi_Line)
{
    /*
     *	Allocate the requested memory.
     */
    void *vp_calloc = (void *)calloc(st_number, st_bytes);

    /*
     *	Only if the allocation was a success do we track information.
     */
    if (vp_calloc != NULL) {
	/*
	 *  Allocate memory for the item to be in the list.
	 *  If unable, just return.
	 */
	AllocationList *ALp_new =
			(AllocationList *)calloc(1, sizeof(AllocationList));

	if (ALp_new == NULL) {
		return(vp_calloc);
	}

	/*
	 *  Copy over the relevant information.
	 *  There is no need to allocate memory for the file
	 *  name as it is a static string anyway.
	 */
	ALp_new->vp_Alloced = vp_calloc;
	ALp_new->st_Bytes = (st_number * st_bytes);
	ALp_new->SL_memory.cp_FileName = cp_File;
	ALp_new->SL_memory.ssi_LineNumber = ssi_Line;

	/*
	 *	Add the item to the allocation list.
	 */
	AddToList(ALp_new);
    }

    return(vp_calloc);
}

/*
**  Purpose:	Capture any realloc (stdlib.h) calls in order to
**		properly keep track of our run time allocation
**		table.
**  Arguments:	vp_Alloced	The previously allocated block of
**				memory to resize.  If NULL,
**				realloc works just like
**				malloc.
**		st_newBytes	The new size of the chunk of memory.
**		cp_File		The file containing the realloc.
**		ssi_Line	The line containing the realloc in cp_File.
**  Return Value:	void *	The new pointer value (could be the same) or
**				NULL if unable to resize (old block
**				still exists).
**  Remarks/Portability/Dependencies/Restrictions:
**		If unable to resize vp_Alloced, then no change in the
**		allocation list will be made.
**		If vp_Alloced is an invalid pointer value, the program will
**		exit after one last entry is added to the allocation list.
**  Revision History:
**	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
*/
PUBLIC void *LYLeakRealloc ARGS4(
	void *,		vp_Alloced,
	size_t,		st_newBytes,
	CONST char *,	cp_File,
	CONST short,	ssi_Line)
{
    void *vp_realloc;
    AllocationList *ALp_renew;

    /*
     *	If we are asked to resize a NULL pointer, this is just a
     *	malloc call.
     */
    if (vp_Alloced == NULL) {
	return(LYLeakMalloc(st_newBytes, cp_File, ssi_Line));
    }

    /*
     *	Find the current vp_Alloced block in the list.
     *	If NULL, this is an invalid pointer value.
     */
    ALp_renew = FindInList(vp_Alloced);
    if (ALp_renew == NULL) {
	/*
	 *  Track the invalid pointer value and then exit.
	 *  If unable to allocate, just exit.
	 */
	auto AllocationList *ALp_new =
			     (AllocationList *)calloc(1,
						      sizeof(AllocationList));

	if (ALp_new == NULL) {
	    exit(-1);
	}

	/*
	 *  Set the information up; no need to allocate file name
	 *  since it is a static string.
	 */
	ALp_new->vp_Alloced = NULL;
	ALp_new->vp_BadRequest = vp_Alloced;
	ALp_new->SL_realloc.cp_FileName = cp_File;
	ALp_new->SL_realloc.ssi_LineNumber = ssi_Line;

	/*
	 *  Add the item to the list.
	 *  Exit.
	 */
	AddToList(ALp_new);
	exit(-1);
    }

    /*
     *	Perform the resize.
     *	If not NULL, record the information.
     */
    vp_realloc = (void *)realloc(vp_Alloced, st_newBytes);
    if (vp_realloc != NULL) {
	ALp_renew->vp_Alloced = vp_realloc;
	ALp_renew->st_Bytes = st_newBytes;

	/*
	 *  Update the realloc information, too.
	 *  No need to allocate file name, static string.
	 */
	ALp_renew->SL_realloc.cp_FileName = cp_File;
	ALp_renew->SL_realloc.ssi_LineNumber = ssi_Line;
    }

    return(vp_realloc);
}

/*
**  Purpose:	Capture all requests to free information and also
**		remove items from the allocation list.
**  Arguments:	vp_Alloced	The memory to free.
**		cp_File		The file calling free.
**		ssi_Line	The line of cp_File calling free.
**  Return Value:	void
**  Remarks/Portability/Dependencies/Restrictions:
**		If the pointer value is invalid, then an item will be added
**		to the list and nothing else is done.
**		I really like the name of this function and one day hope
**		that Lynx is Leak Free.
**  Revision History:
**	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
*/
PUBLIC void LYLeakFree ARGS3(
	void *,		vp_Alloced,
	CONST char *,	cp_File,
	CONST short,	ssi_Line)
{
    AllocationList *ALp_free;

    /*
     *	Find the pointer in the allocated list.
     *	If not found, bad pointer.
     *	If found, free list item and vp_Allloced.
     */
    ALp_free = FindInList(vp_Alloced);
    if (ALp_free == NULL) {
	/*
	 *  Create the final entry before exiting marking this error.
	 *  If unable to allocate more memory just exit.
	 */
	AllocationList *ALp_new =
			(AllocationList *)calloc(1,
						 sizeof(AllocationList));

	if (ALp_new == NULL) {
	    exit(-1);
	}

	/*
	 *  Set up the information, no memory need be allocated
	 *  for the file name since it is a static string.
	 */
	ALp_new->vp_Alloced = NULL;
	ALp_new->vp_BadRequest = vp_Alloced;
	ALp_new->SL_memory.cp_FileName = cp_File;
	ALp_new->SL_memory.ssi_LineNumber = ssi_Line;

	/*
	 *  Add the entry to the list and then return.
	 */
	AddToList(ALp_new);
	return;
    } else {
	/*
	 *  Free off the memory.
	 *  Take entry out of allocation list.
	 */
	RemoveFromList(ALp_free);
	FREE(ALp_free);
	FREE(vp_Alloced);
    }
}

/*
**  Allocates a new copy of a string, and returns it.
**  Tracks allocations by using other LYLeakFoo functions.
**  Equivalent to HTSACopy in HTUtils.c - KW
*/
PUBLIC char * LYLeakSACopy ARGS4(
	char **,	dest,
	CONST char *,	src,
	CONST char *,	cp_File,
	CONST short,	ssi_Line)
{
    if (src != NULL && src == *dest) {
	CTRACE(tfp,
	       "LYLeakSACopy: *dest equals src, contains \"%s\"\n",
	       src);
	return *dest;
    }
    if (*dest) {
	LYLeakFree(*dest, cp_File, ssi_Line);
	*dest = NULL;
    }
    if (src) {
	*dest = (char *)LYLeakMalloc(strlen(src) + 1, cp_File, ssi_Line);
	if (*dest == NULL)
	    outofmem(__FILE__, "LYLeakSACopy");
	strcpy (*dest, src);
    }
    return *dest;
}

/*
**  String Allocate and Concatenate.
**  Tracks allocations by using other LYLeakFoo functions.
**  Equivalent to HTSACat in HTUtils.c - KW
*/
PUBLIC char * LYLeakSACat ARGS4(
	char **,	dest,
	CONST char *,	src,
	CONST char *,	cp_File,
	CONST short,	ssi_Line)
{
    if (src && *src) {
	if (src == *dest) {
	    CTRACE(tfp,
		   "LYLeakSACat:  *dest equals src, contains \"%s\"\n",
		   src);
	    return *dest;
	}
	if (*dest) {
	    int length = strlen(*dest);
	    *dest = (char *)LYLeakRealloc(*dest,
					  (length + strlen(src) + 1),
					  cp_File,
					  ssi_Line);
	    if (*dest == NULL)
		outofmem(__FILE__, "LYLeakSACat");
	    strcpy (*dest + length, src);
	} else {
	    *dest = (char *)LYLeakMalloc((strlen(src) + 1),
					 cp_File,
					 ssi_Line);
	    if (*dest == NULL)
		outofmem(__FILE__, "LYLeakSACat");
	    strcpy (*dest, src);
	}
    }
    return *dest;
}

/*
**  Purpose:	Add a new allocation item to the list.
**  Arguments:		ALp_new The new item to add.
**  Return Value:	void
**  Remarks/Portability/Dependencies/Restrictions:
**		Static function made to make code reusable in projects beyond
**		Lynx (some might ask why not use HTList).
**  Revision History:
**	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
*/
PRIVATE void AddToList ARGS1(
	AllocationList *,	ALp_new)
{
    /*
     *	Just make this the first item in the list.
     */
    ALp_new->ALp_Next = ALp_RunTimeAllocations;
    ALp_RunTimeAllocations = ALp_new;
}

/*
**  Purpose:	Find the place in the list where vp_find is currently
**		tracked.
**  Arguments:		vp_find A pointer to look for in the list.
**  Return Value:	AllocationList *	Either vp_find's place in the
**						list or NULL if not found.
**  Remarks/Portability/Dependencies/Restrictions:
**		Static function made to make code reusable in projects outside
**		of Lynx (some might ask why not use HTList).
**  Revision History:
**	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
*/
PRIVATE AllocationList *FindInList ARGS1(
	void *,		vp_find)
{
    AllocationList *ALp_find = ALp_RunTimeAllocations;

    /*
     *	Go through the list of allocated pointers until end of list
     *		or vp_find is found.
     */
    while (ALp_find != NULL) {
	if (ALp_find->vp_Alloced == vp_find) {
	    break;
	}
	ALp_find = ALp_find->ALp_Next;
    }

    return(ALp_find);
}

/*
**  Purpose:	Remove the specified item from the list.
**  Arguments:		ALp_del The item to remove from the list.
**  Return Value:	void
**  Remarks/Portability/Dependencies/Restrictions:
**		Static function made to make code reusable in projects outside
**		of Lynx (some might ask why not use HTList).
**  Revision History:
**	05-26-94	created Lynx 2-3-1 Garrett Arch Blythe
*/
PRIVATE void RemoveFromList ARGS1(
	AllocationList *,	ALp_del)
{
    AllocationList *ALp_findbefore = ALp_RunTimeAllocations;

    /*
     *	There is one special case, where the item to remove is the
     *		first in the list.
     */
    if (ALp_del == ALp_findbefore) {
	ALp_RunTimeAllocations = ALp_del->ALp_Next;
	return;
    }

    /*
     *	Loop through checking all of the next values, if a match
     *	don't continue.  Always assume the item will be found.
     */
    while (ALp_findbefore->ALp_Next != ALp_del) {
	ALp_findbefore = ALp_findbefore->ALp_Next;
    }

    /*
     *	We are one item before the one to get rid of.
     *	Get rid of it.
     */
    ALp_findbefore->ALp_Next = ALp_del->ALp_Next;
}
