/*
 * $OpenBSD: aout2bb.h,v 1.1 1997/01/16 09:26:49 niklas Exp $
 * $NetBSD: aout2bb.h,v 1.1.1.1 1996/11/29 23:36:30 is Exp $
 *
 * Copyright (c) 1996 Ignatios Souvatzis
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
 *      This product includes software developed by Ignatios Souvatzis
 *      for the NetBSD project.
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
 *
 */

/*
 * Relocator version definitions for aout to Amiga bootblock converter.
 */

/* 
 * All the tables are at the end of the bootblock, with logical start at the
 * end.
 */

/* 
 * The following formats are for a length of 8 kBytes only.
 */

/*
 * Absolute words in Motorola byte order, end of table marked by 0x0000
 */
#define RELVER_ABSOLUTE_WORDS	0

/*
 * Unsigned bytes relative to previous address to relocate; first one to 0.
 * If the difference is >255, the logical next two bytes (in Motorola byte
 * order) give the absolute address to relocate.
 */
#define RELVER_RELATIVE_BYTES	1

/*
 * Same as above, but with the bytes stored in forward direction beginning
 * with the __relocation_bytes symbol
 */
#define RELVER_RELATIVE_BYTES_FORWARD 2

/*
 * loader can autoload
 */

#define RELFLAG_SELFLOADING	0x10
