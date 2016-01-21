/*	$OpenBSD: io.h,v 1.8 2016/01/21 15:33:21 krw Exp $	*/

/*
 * io.h - simple io and input parsing routines
 *
 * Written by Eryk Vershen
 */

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

#ifndef __io__
#define __io__


#define MAXIOSIZE	2048

extern const long kDefault;

void bad_input(const char *, ...);
void flush_to_newline(int);
int get_command(const char *, int, int *);
unsigned long get_multiplier(long);
int get_number_argument(const char *, long *, long);
int get_okay(const char *, int);
int get_partition_modifier(void);
int get_string_argument(const char *, char **, int);
int number_of_digits(unsigned long);
void my_ungetch(int);

#endif /* __io__ */
