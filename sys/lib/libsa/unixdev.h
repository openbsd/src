/*	$OpenBSD: unixdev.h,v 1.3 1998/05/25 18:37:31 mickey Exp $	*/

/*
 * Copyright (c) 1996 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


/* unixdev.c */
int unixstrategy __P((void *, int, daddr_t, size_t, void *, size_t *));
int unixopen __P((struct open_file *, ...));
int unixclose __P((struct open_file *));
int unixioctl __P((struct open_file *, u_long, void *));

void unix_probe __P((struct consdev *));
void unix_init __P((struct consdev *));
int unix_getc __P((dev_t));
void unix_putc __P((dev_t, int));
int unix_ischar __P((dev_t));

/* unixsys.S */
int uopen __P((const char *, int, ...));
int uread __P((int, void *, size_t));
int uwrite __P((int, void *, size_t));
int uioctl __P((int, u_long, char *));
int uclose __P((int));
off_t ulseek __P((int, off_t, int));
void uexit __P((int)) __attribute__((noreturn));
int syscall __P((int, ...));
int __syscall __P((quad_t, ...));
