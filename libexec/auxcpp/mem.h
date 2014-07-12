/*
 * (c) Thomas Pornin 1998 - 2002
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. The name of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef UCPP__MEM__
#define UCPP__MEM__

#include <stdlib.h>

void die(void);

#if defined AUDIT || defined MEM_CHECK || defined MEM_DEBUG
void *getmem(size_t);
#else
#define getmem		malloc
#endif

#if defined MEM_DEBUG
void *getmem_debug(size_t, char *, int);
#undef getmem
#define getmem(x)	getmem_debug(x, __FILE__, __LINE__)
#endif

#if defined AUDIT || defined MEM_DEBUG
void freemem(void *);
#else
#define freemem		free
#endif

#if defined MEM_DEBUG
void freemem_debug(void *, char *, int);
#undef freemem
#define freemem(x)	freemem_debug(x, __FILE__, __LINE__)
#endif

void *incmem(void *, size_t, size_t);
char *sdup(char *);

#if defined MEM_DEBUG
void *incmem_debug(void *, size_t, size_t, char *, int);
#undef incmem
#define incmem(x, y, z)	incmem_debug(x, y, z, __FILE__, __LINE__)
void report_leaks(void);
char *sdup_debug(char *, char *, int);
#define sdup(x)		sdup_debug(x, __FILE__, __LINE__)
#endif

#ifdef AUDIT
void *mmv(void *, void *, size_t);
void *mmvwo(void *, void *, size_t);
#else
#define mmv	memcpy
#define mmvwo	memmove
#endif

/*
 * this macro adds the object obj at the end of the array list, handling
 * memory allocation when needed; ptr contains the number of elements in
 * the array, and memg is the granularity of memory allocations (a power
 * of 2 is recommanded, for optimization reasons).
 *
 * list and ptr may be updated, and thus need to be lvalues.
 */
#define aol(list, ptr, obj, memg)	do { \
		if (((ptr) % (memg)) == 0) { \
			if ((ptr) != 0) { \
				(list) = incmem((list), (ptr) * sizeof(obj), \
					((ptr) + (memg)) * sizeof(obj)); \
			} else { \
				(list) = getmem((memg) * sizeof(obj)); \
			} \
		} \
		(list)[(ptr) ++] = (obj); \
	} while (0)

/*
 * bol() does the same as aol(), but adds the new item at the beginning
 * of the list; beware, the computational cost is greater.
 */
#define bol(list, ptr, obj, memg)	do { \
		if (((ptr) % (memg)) == 0) { \
			if ((ptr) != 0) { \
				(list) = incmem((list), (ptr) * sizeof(obj), \
					((ptr) + (memg)) * sizeof(obj)); \
			} else { \
				(list) = getmem((memg) * sizeof(obj)); \
			} \
		} \
		if ((ptr) != 0) \
			mmvwo((list) + 1, (list), (ptr) * sizeof(obj)); \
		(ptr) ++; \
		(list)[0] = (obj); \
	} while (0)

/*
 * mbol() does the same as bol(), but adds the new item at the given
 * emplacement; bol() is equivalent to mbol with 0 as last argument.
 */
#define mbol(list, ptr, obj, memg, n)	do { \
		if (((ptr) % (memg)) == 0) { \
			if ((ptr) != 0) { \
				(list) = incmem((list), (ptr) * sizeof(obj), \
					((ptr) + (memg)) * sizeof(obj)); \
			} else { \
				(list) = getmem((memg) * sizeof(obj)); \
			} \
		} \
		if ((ptr) > n) \
			mmvwo((list) + n + 1, (list) + n, \
				((ptr) - n) * sizeof(obj)); \
		(ptr) ++; \
		(list)[n] = (obj); \
	} while (0)

/*
 * this macro adds the object obj at the end of the array list, doubling
 * the size of list when needed; as for aol(), ptr and list must be
 * lvalues, and so must be llng
 */

#define wan(list, ptr, obj, llng)	do { \
		if ((ptr) == (llng)) { \
			(llng) += (llng); \
			(list) = incmem((list), (ptr) * sizeof(obj), \
				(llng) * sizeof(obj)); \
		} \
		(list)[(ptr) ++] = (obj); \
	} while (0)

#endif
