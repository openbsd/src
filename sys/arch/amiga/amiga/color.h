/*	$NetBSD: color.h,v 1.8 1995/02/12 19:34:17 chopps Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * some colors, handy for debugging 
 */
#ifdef DEBUG
#define COL_BLACK	0x000
#define COL_DARK_GRAY	0x444
#define COL_MID_GRAY	0x888
#define COL_LITE_GRAY	0xbbb
#define COL_WHITE	0xfff

#define COL_BLUE	0x00f
#define COL_GREEN	0x0f0
#define COL_RED		0xf00
#define COL_CYAN	0x0ff
#define COL_YELLOW	0xff0
#define COL_MAGENTA	0xf0f

#define COL_LITE_BLUE	0x0af
#define COL_LITE_GREEN	0x0fa
#define COL_ORANGE	0xfa0
#define COL_PURPLE	0xf0a

#define COL24_BLACK	0x0
#define COL24_DGREY	0x0009
#define COL24_LGREY	0x0880
#define COL24_WHITE	0xffff

void rollcolor __P((int));
#endif
