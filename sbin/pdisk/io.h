//
// io.h - simple io and input parsing routines
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

#ifndef __io__
#define __io__


//
// Defines
//
#define MAXIOSIZE	2048


//
// Types
//


//
// Global Constants
//
extern const long kDefault;


//
// Global Variables
//


//
// Forward declarations
//
void bad_input(const char *fmt, ...);
void flush_to_newline(int keep_newline);
int get_command(const char *prompt, int promptBeforeGet, int *command);
unsigned long get_multiplier(long divisor);
int get_number_argument(const char *prompt, long *number, long default_value);
int get_okay(const char *prompt, int default_value);
int get_partition_modifier(void);
int get_string_argument(const char *prompt, char **string, int reprompt);
int number_of_digits(unsigned long value);

#endif /* __io__ */
