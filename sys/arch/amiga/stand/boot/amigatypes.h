/*
 * $OpenBSD: amigatypes.h,v 1.1 1997/01/16 09:26:26 niklas Exp $
 * $NetBSD: amigatypes.h,v 1.1.1.1 1996/11/29 23:36:29 is Exp $
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

#ifndef _AMIGA_TYPES_H_
#define _AMIGA_TYPES_H_

/* Dummy structs, used only as abstract pointers */

struct Library;
struct TextAttr;
struct Gadget;
struct BitMap;
struct NewScreen;
struct MemNode;

/* real structs */

struct TagItem {u_int32_t item; void * data;};

struct Library {
	u_int8_t Dmy1[20];
	u_int16_t Version, Revision;
	u_int8_t Dmy2[34-24];
};

struct MemHead {
	struct MemHead *next;
	u_int8_t Dmy1[  9-  4];
	u_int8_t Pri;
	u_int8_t Dmy2[ 14- 10];
	u_int16_t Attribs;
	u_int32_t First, Lower, Upper, Free;
};

struct ExecBase {
	struct Library LibNode;
	u_int8_t Dmy1[296-34];
	u_int16_t AttnFlags;	/* 296 */
	u_int8_t Dmy2[300-298];	/* 298 */
	void *ResModules;	/* 300 */
	u_int8_t Dmy3[322-304];	/* 304 */
	struct MemHead *MemLst;	/* 322 */
	/*
	 * XXX: actually, its a longer List base, but we only need to 
	 * search it once.
	 */
	u_int8_t Dmy4[568-326];	/* 326 */
	u_int32_t EClockFreq;	/* 330 */
	u_int8_t Dmy5[632-334];
};

#endif /* _AMIGA_TYPES_H */
