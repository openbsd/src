/*	$OpenBSD: wsconsctl.h,v 1.6 2002/12/17 07:10:25 mickey Exp $	*/
/*	$NetBSD: wsconsctl.h 1.1 1998/12/28 14:01:17 hannken Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <dev/wscons/wsksymvar.h>

struct field {
	char *name;
	void *valp;
#define FMT_UINT	1		/* unsigned integer */
#define FMT_INT		2		/* signed integer */
#define FMT_BOOL	3		/* boolean on/off */
#define FMT_PC		4		/* percentage fixed point 000.00 */
#define FMT_KBDTYPE	101		/* keyboard type */
#define FMT_MSTYPE	102		/* mouse type */
#define FMT_DPYTYPE	103		/* display type */
#define FMT_KBDENC	104		/* keyboard encoding */
#define FMT_KBMAP	105		/* keyboard map */
	int format;
#define FLG_RDONLY	0x0001		/* variable cannot be modified */
#define FLG_WRONLY	0x0002		/* variable cannot be displayed */
#define FLG_NOAUTO	0x0004		/* skip variable on -a flag */
#define FLG_MODIFY	0x0008		/* variable may be modified with += */
#define FLG_GET		0x0100		/* read this variable from driver */
#define FLG_SET		0x0200		/* write this variable to driver */
#define FLG_INIT	0x0400		/* init (read) before write */
#define FLG_DEAD	0x0800		/* the var isn't there, let it rest */
	int flags;
};

struct field_pc {
	int max, min, cur;
};

struct field *field_by_name(struct field *, char *);
struct field *field_by_value(struct field *, void *);
void pr_field(const char *, struct field *, const char *);
void rd_field(struct field *, char *, int);
int name2ksym(char *);
char *ksym2name(int);
keysym_t ksym_upcase(keysym_t);
void keyboard_get_values(const char *,int);
void keyboard_put_values(const char *,int);
void mouse_get_values(const char *,int);
void mouse_put_values(const char *,int);
void display_get_values(const char *,int);
void display_put_values(const char *,int);
int yyparse(void);
void yyerror(char *);
int yylex(void);
void map_scan_setinput(char *);
