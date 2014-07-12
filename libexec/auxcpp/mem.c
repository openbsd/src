/*
 * Memory manipulation routines
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

#include "mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Shifting a pointer of that some bytes is supposed to satisfy
 * alignment requirements. This is *not* guaranteed by the standard
 * but should work everywhere anyway.
 */
#define ALIGNSHIFT	(sizeof(long) > sizeof(long double) \
			? sizeof(long) : sizeof(long double))

#ifdef AUDIT
void die(void)
{
	abort();
}

static void suicide(unsigned long e)
{
	fprintf(stderr, "ouch: Schrodinger's beef is not dead ! %lx\n", e);
	die();
}
#else
void die(void)
{
	exit(EXIT_FAILURE);
}
#endif

#if defined AUDIT || defined MEM_CHECK || defined MEM_DEBUG
/*
 * This function is equivalent to a malloc(), but will display an error
 * message and exit if the wanted memory is not available
 */
#ifdef MEM_DEBUG
static void *getmem_raw(size_t x)
#else
void *(getmem)(size_t x)
#endif
{
	void *m;

#ifdef AUDIT
	m = malloc(x + ALIGNSHIFT);
#else
	m = malloc(x);
#endif
	if (m == 0) {
		fprintf(stderr, "ouch: malloc() failed\n");
		die();
	}
#ifdef AUDIT
	*((unsigned long *)m) = 0xdeadbeefUL;
	return (void *)(((char *)m) + ALIGNSHIFT);
#else
	return m;
#endif
}
#endif

#ifndef MEM_DEBUG
/*
 * This function is equivalent to a realloc(); if the realloc() call
 * fails, it will try a malloc() and a memcpy(). If not enough memory is
 * available, the program exits with an error message
 */
void *(incmem)(void *m, size_t x, size_t nx)
{
	void *nm;

#ifdef AUDIT
	m = (void *)(((char *)m) - ALIGNSHIFT);
	if (*((unsigned long *)m) != 0xdeadbeefUL)
		suicide(*((unsigned long *)m));
	x += ALIGNSHIFT; nx += ALIGNSHIFT;
#endif
	if (!(nm = realloc(m, nx))) {
		if (x > nx) x = nx;
		nm = (getmem)(nx);
		memcpy(nm, m, x);
		/* free() and not freemem(), because of the Schrodinger beef */
		free(m);
	}
#ifdef AUDIT
	return (void *)(((char *)nm) + ALIGNSHIFT);
#else
	return nm;
#endif
}
#endif

#if defined AUDIT || defined MEM_DEBUG
/*
 * This function frees the given block
 */
#ifdef MEM_DEBUG
static void freemem_raw(void *x)
#else
void (freemem)(void *x)
#endif
{
#ifdef AUDIT
	void *y = (void *)(((char *)x) - ALIGNSHIFT);

	if ((*((unsigned long *)y)) != 0xdeadbeefUL)
		suicide(*((unsigned long *)y));
	*((unsigned long *)y) = 0xfeedbabeUL;
	free(y);
#else
	free(x);
#endif
}
#endif

#ifdef AUDIT
/*
 * This function copies n bytes from src to dest
 */
void *mmv(void *dest, void *src, size_t n)
{
	return memcpy(dest, src, n);
}

/*
 * This function copies n bytes from src to dest
 */
void *mmvwo(void *dest, void *src, size_t n)
{
	return memmove(dest, src, n);
}
#endif

#ifndef MEM_DEBUG
/*
 * This function creates a new char * and fills it with a copy of src
 */
char *(sdup)(char *src)
{
	size_t n = 1 + strlen(src);
	char *x = getmem(n);

	mmv(x, src, n);
	return x;
}
#endif

#ifdef MEM_DEBUG
/*
 * We include here special versions of getmem(), freemem() and incmem()
 * that track allocations and are used to detect memory leaks.
 *
 * Each allocation is referenced in a list, with a serial number.
 */

/*
 * Define "true" functions for applications that need pointers
 * to such functions.
 */
void *(getmem)(size_t n)
{
	return getmem(n);
}

void (freemem)(void *x)
{
	freemem(x);
}

void *(incmem)(void *x, size_t s, size_t ns)
{
	return incmem(x, s, ns);
}

char *(sdup)(char *s)
{
	return sdup(s);
}

static long current_serial = 0L;

/* must be a power of two */
#define MEMDEBUG_MEMG	128U

static struct mem_track {
	void *block;
	long serial;
	char *file;
	int line;
} *mem = 0;

static size_t meml = 0;

static unsigned int current_ptr = 0;

static void *true_incmem(void *x, size_t old_size, size_t new_size)
{
	void * y = realloc(x, new_size);

	if (y == 0) {
		y = malloc(new_size);
		if (y == 0) {
			fprintf(stderr, "ouch: malloc() failed\n");
			die();
		}
		mmv(y, x, old_size < new_size ? old_size : new_size);
		free(x);
	}
	return y;
}

static long find_free_block(void)
{
	unsigned int n;
	size_t i;

	for (i = 0, n = current_ptr; i < meml; i ++) {
		if (mem[n].block == 0) {
			current_ptr = n;
			return n;
		}
		n = (n + 1) & (meml - 1U);
	}
	if (meml == 0) {
		size_t j;

		meml = MEMDEBUG_MEMG;
		mem = malloc(meml * sizeof(struct mem_track));
		current_ptr = 0;
		for (j = 0; j < meml ; j ++) mem[j].block = 0;
	} else {
		size_t j;

		mem = true_incmem(mem, meml * sizeof(struct mem_track),
			2 * meml * sizeof(struct mem_track));
		current_ptr = meml;
		for (j = meml; j < 2 * meml ; j ++) mem[j].block = 0;
		meml *= 2;
	}
	return current_ptr;
}

void *getmem_debug(size_t n, char *file, int line)
{
	void *x = getmem_raw(n + ALIGNSHIFT);
	long i = find_free_block();

	*(long *)x = i;
	mem[i].block = x;
	mem[i].serial = current_serial ++;
	mem[i].file = file;
	mem[i].line = line;
	return (void *)((unsigned char *)x + ALIGNSHIFT);
}

void freemem_debug(void *x, char *file, int line)
{
	void *y = (unsigned char *)x - ALIGNSHIFT;
	long i = *(long *)y;

	if (i < 0 || (size_t)i >= meml || mem[i].block != y) {
		fprintf(stderr, "ouch: freeing free people (from %s:%d)\n",
			file, line);
		die();
	}
	mem[i].block = 0;
	freemem_raw(y);
}

void *incmem_debug(void *x, size_t ol, size_t nl, char *file, int line)
{
	void *y = getmem_debug(nl, file, line);
	mmv(y, x, ol < nl ? ol : nl);
	freemem_debug(x, file, line);
	return y;
}

char *sdup_debug(char *src, char *file, int line)
{
	size_t n = 1 + strlen(src);
	char *x = getmem_debug(n, file, line);

	mmv(x, src, n);
	return x;
}

void report_leaks(void)
{
	size_t i;

	for (i = 0; i < meml; i ++) {
		if (mem[i].block) fprintf(stderr, "leak: serial %ld, %s:%d\n",
			mem[i].serial, mem[i].file, mem[i].line);
	}
}

#endif
