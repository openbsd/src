/*	$OpenBSD: display.h,v 1.9 2007/05/29 00:56:56 otto Exp $	*/

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS EMPLOYER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* constants needed for display.c */

#define  MT_standout  1
#define  MT_delayed   2

/* prototypes */
int display_resize(void);
void i_loadave(int, double *);
void u_loadave(int, double *);
void i_timeofday(time_t *);
void i_procstates(int, int *);
void u_procstates(int, int *);
void i_cpustates(int64_t *);
void u_cpustates(int64_t *);
void i_memory(int *);
void u_memory(int *);
void i_message(void);
void u_message(void);
void i_header(char *);
void u_header(char *);
void i_process(int, char *, int);
void u_process(int, char *, int);
void u_endscreen(int);
void display_header(int);
void new_message(int, const char *, ...);
void clear_message(void);
int readline(char *, int, int);
char *printable(char *);
void show_help(void);
void anykey(void);

#define putr() do { if (!smart_terminal) if (putchar('\r') == EOF) exit(1); } \
	while (0)
#define putn() do { if (!smart_terminal) if (putchar('\n') == EOF) exit(1); } \
	while (0)
