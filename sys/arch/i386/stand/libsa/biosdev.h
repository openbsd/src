/*	$OpenBSD: biosdev.h,v 1.13 1997/08/12 19:16:37 mickey Exp $	*/

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


/* biosdev.c */
extern const char *biosdevs[];
int biosstrategy __P((void *, int, daddr_t, size_t, void *, size_t *));
int biosopen __P((struct open_file *, ...));
int biosclose __P((struct open_file *));
int biosioctl __P((struct open_file *, u_long, void *));

/* biosdisk.S */
u_int16_t biosdinfo __P((int dev));
int biosdreset __P((int dev));
int biosread  __P((int dev, int cyl, int hd, int sect, int nsect, void *));
int bioswrite __P((int dev, int cyl, int hd, int sect, int nsect, void *));
int EDDcheck __P((dev_t));
int EDDread __P((dev_t, u_int64_t, u_int32_t, void *));
int EDDwrite __P((dev_t, u_int64_t, u_int32_t, void *));

/* bioskbd.S */
int kbd_probe __P((void));
void kbd_putc __P((int c));
int kbd_getc __P((void));
int kbd_ischar __P((void));

/* bioscom.S */
#define COM_PROTO(n) \
int com##n##_probe __P((void)); \
void com##n##_putc __P((int c)); \
int com##n##_getc __P((void)); \
int com##n##_ischar __P((void));
COM_PROTO(0)
COM_PROTO(1)
COM_PROTO(2)
COM_PROTO(3)
#undef COM_PROTO

/* time.c */
void time_print __P((void));
time_t getsecs __P((void));
