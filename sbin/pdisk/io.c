//
// io.c - simple io and input parsing routines
//
// Written by Eryk Vershen
//

/*
 * Copyright 1996,1997,1998 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

// for *printf()
#include <stdio.h>

// for malloc() & free()
#if !defined(__linux__)
#include <stdlib.h>
#else
#include <malloc.h>
#endif
// for strncpy()
#include <string.h>
// for va_start(), etc.
#include <stdarg.h>
// for errno
#include <errno.h>

#include "io.h"
#include "errors.h"


//
// Defines
//
#define BAD_DIGIT 17	/* must be greater than any base */
#define	STRING_CHUNK	16
#define UNGET_MAX_COUNT 10
#ifndef __linux__
#ifndef __unix__
#define SCSI_FD 8
#endif
#ifdef NeXT
#define loff_t off_t
#define llseek lseek
#else
#define loff_t long
#define llseek lseek
#endif
#endif


//
// Types
//


//
// Global Constants
//
const long kDefault = -1;


//
// Global Variables
//
short unget_buf[UNGET_MAX_COUNT+1];
int unget_count;
char io_buffer[MAXIOSIZE];


//
// Forward declarations
//
long get_number(int first_char);
char* get_string(int eos);
int my_getch(void);
void my_ungetch(int c);


//
// Routines
//
int
my_getch()
{
    if (unget_count > 0) {
	return (unget_buf[--unget_count]);
    } else {
	return (getc(stdin));
    }
}


void
my_ungetch(int c)
{
    // In practice there is never more than one character in
    // the unget_buf, but what's a little overkill among friends?

    if (unget_count < UNGET_MAX_COUNT) {
	unget_buf[unget_count++] = c;
    } else {
	fatal(-1, "Programmer error in my_ungetch().");
    }
}

	
void
flush_to_newline(int keep_newline)
{
    int		c;

    for (;;) {
	c = my_getch();

	if (c <= 0) {
	    break;
	} else if (c == '\n') {
	    if (keep_newline) {
		my_ungetch(c);
	    }
	    break;
	} else {
	    // skip
	}
    }
    return;
}


int
get_okay(const char *prompt, int default_value)
{
    int		c;

    flush_to_newline(0);
    printf(prompt);

    for (;;) {
	c = my_getch();

	if (c <= 0) {
	    break;
	} else if (c == ' ' || c == '\t') {
	    // skip blanks and tabs
	} else if (c == '\n') {
	    my_ungetch(c);
	    return default_value;
	} else if (c == 'y' || c == 'Y') {
	    return 1;
	} else if (c == 'n' || c == 'N') {
	    return 0;
	} else {
	    flush_to_newline(0);
	    printf(prompt);
	}
    }
    return -1;
}

	
int
get_command(const char *prompt, int promptBeforeGet, int *command)
{
    int		c;

    if (promptBeforeGet) {
	printf(prompt);
    }	
    for (;;) {
	c = my_getch();

	if (c <= 0) {
	    break;
	} else if (c == ' ' || c == '\t') {
	    // skip blanks and tabs
	} else if (c == '\n') {
	    printf(prompt);
	} else {
	    *command = c;
	    return 1;
	}
    }
    return 0;
}

	
int
get_number_argument(const char *prompt, long *number, long default_value)
{
    int c;
    int result = 0;

    for (;;) {
	c = my_getch();

	if (c <= 0) {
	    break;
	} else if (c == ' ' || c == '\t') {
	    // skip blanks and tabs
	} else if (c == '\n') {
	    if (default_value == kDefault) {
		printf(prompt);
	    } else {
		my_ungetch(c);
		*number = default_value;
		result = 1;
		break;
	    }
	} else if ('0' <= c && c <= '9') {
	    *number = get_number(c);
	    result = 1;
	    break;
	} else {
	    my_ungetch(c);
	    *number = 0;
	    break;
	}
    }
    return result;
}


long
get_number(int first_char)
{
    int c;
    int base;
    int digit;
    int ret_value;

    if (first_char != '0') {
	c = first_char;
	base = 10;
	digit = BAD_DIGIT;
    } else if ((c=my_getch()) == 'x' || c == 'X') {
	c = my_getch();
	base = 16;
	digit = BAD_DIGIT;
    } else {
	my_ungetch(c);
	c = first_char;
	base = 8;
	digit = 0;
    }
    ret_value = 0;
    for (ret_value = 0; ; c = my_getch()) {
	if (c >= '0' && c <= '9') {
	    digit = c - '0';
	} else if (c >='A' && c <= 'F') {
	    digit = 10 + (c - 'A');
	} else if (c >='a' && c <= 'f') {
	    digit = 10 + (c - 'a');
	} else {
	    digit = BAD_DIGIT;
	}
	if (digit >= base) {
	    break;
	}
	ret_value = ret_value * base + digit;
    }
    my_ungetch(c);
    return(ret_value);
}

	
int
get_string_argument(const char *prompt, char **string, int reprompt)
{
    int c;
    int result = 0;

    for (;;) {
	c = my_getch();

	if (c <= 0) {
	    break;
	} else if (c == ' ' || c == '\t') {
	    // skip blanks and tabs
	} else if (c == '\n') {
	    if (reprompt) {
		printf(prompt);
	    } else {
		my_ungetch(c);
		*string = NULL;
		break;
	    }
	} else if (c == '"' || c == '\'') {
	    *string = get_string(c);
	    result = 1;
	    break;
	} else if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')
		|| (c == '-' || c == '/' || c == '.' || c == ':')) {
	    my_ungetch(c);
	    *string = get_string(' ');
	    result = 1;
	    break;
	} else {
	    my_ungetch(c);
	    *string = NULL;
	    break;
	}
    }
    return result;
}


char *
get_string(int eos)
{
    int c;
    char *s;
    char *ret_value;
    char *limit;
    int length;

    ret_value = (char *) malloc(STRING_CHUNK);
    if (ret_value == NULL) {
	error(errno, "can't allocate memory for string buffer");
	return NULL;
    }
    length = STRING_CHUNK;
    limit = ret_value + length;

    c = my_getch();
    for (s = ret_value; ; c = my_getch()) {
	if (s >= limit) {
	    // expand string
	    limit = (char *) malloc(length+STRING_CHUNK);
	    if (limit == NULL) {
		error(errno, "can't allocate memory for string buffer");
		ret_value[length-1] = 0;
		break;
	    }
	    strncpy(limit, ret_value, length);
	    free(ret_value);
	    s = limit + (s - ret_value);
	    ret_value = limit;
	    length += STRING_CHUNK;
	    limit = ret_value + length;
	}
	if (c <= 0 || c == eos || (eos == ' ' && c == '\t')) {
	    *s++ = 0;
	    break;
	} else if (c == '\n') {
	    *s++ = 0;
	    my_ungetch(c);
	    break;
	} else {
	    *s++ = c;
	}
    }
    return(ret_value);
}


unsigned long
get_multiplier(long divisor)
{
    int c;
    unsigned long result;
    unsigned long extra;

    c = my_getch();

    extra = 1;
    if (c <= 0 || divisor <= 0) {
	result = 0;
    } else if (c == 't' || c == 'T') {
	result = 1024*1024;
	extra = 1024*1024;
    } else if (c == 'g' || c == 'G') {
	result = 1024*1024*1024;
    } else if (c == 'm' || c == 'M') {
	result = 1024*1024;
    } else if (c == 'k' || c == 'K') {
	result = 1024;
    } else {
	my_ungetch(c);
	result = 1;
    }
    if (result > 1) {
	if (extra > 1) {
	    result /= divisor;
	    if (result >= 4096) {
		/* overflow -> 20bits + >12bits */
		result = 0;
	    } else {
		result *= extra;
	    }
	} else if (result >= divisor) {
	    result /= divisor;
	} else {
	    result = 1;
	}
    }
    return result;
}


int
get_partition_modifier(void)
{
    int c;
    int result;

    result = 0;

    c = my_getch();

    if (c == 'p' || c == 'P') {
    	result = 1;
    } else if (c > 0) {
	my_ungetch(c);
    }
    return result;
}


int
number_of_digits(unsigned long value)
{
    int j;

    j = 1;
    while (value > 9) {
	j++;
	value = value / 10;
    }
    return j;
}


//
// Print a message on standard error & flush the input.
//
void
bad_input(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    flush_to_newline(1);
}
