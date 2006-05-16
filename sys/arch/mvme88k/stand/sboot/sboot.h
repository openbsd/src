/*	$OpenBSD: sboot.h,v 1.5 2006/05/16 22:52:26 miod Exp $	*/

/*
 * Copyright (c) 1995 Charles D. Cranor and Seth Widoff
 * All rights reserved.
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
 *      This product includes software developed by Charles D. Cranor
 *	and Seth Widoff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * sboot.h: stuff for MVME147's serial line boot
 */

typedef unsigned short u_short;
typedef unsigned long u_long;
typedef unsigned char u_char;
typedef unsigned int u_int;
typedef u_long size_t;
typedef char *caddr_t;
extern caddr_t end;

#define NULL ((char *)0)

void bcopy(const void *, void *, size_t);	/* libc_sa */
void *memset(void *, int, size_t);		/* libc_sa */
int printf(const char *, ...);			/* libc_sa */

/* console */
void puts(char *);
void putchar(char);
char cngetc(void);
void ngets(char *, int);

/* sboot */
void callrom(void);
void do_cmd(char *);

/* le */
#define LANCE_ADDR 0xfffe0778
#define ERAM_ADDR  0xfffe0774
#define LANCE_REG_ADDR 0xfffe1800
void le_end(void);
void le_init(void);
int le_get(u_char *, size_t, u_long);
int le_put(u_char *, size_t);

/* etherfun */
#define READ 0
#define ACKN 1
void do_rev_arp(void);
int get_rev_arp(void);
int rev_arp(void);
void do_send_tftp(int);
int do_get_file(void);
void tftp_file(char *, u_long);

/* clock */
u_long time(void);

/* checksum */
u_long oc_cksum(void *, u_long, u_long);

#define CONS_ZS_ADDR (0xfffe3002)
#define CLOCK_ADDR (0xfffe07f8)
#define LOAD_ADDR 0x7000

unsigned char myea[6];                /* my ether addr */
unsigned char myip[4];
unsigned char servip[4];
unsigned char servea[6];
u_short myport;
u_short servport;
unsigned char reboot;
