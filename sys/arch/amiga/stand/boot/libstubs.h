/*
 * $OpenBSD: libstubs.h,v 1.1 1997/01/16 09:26:35 niklas Exp $
 * $NetBSD: libstubs.h,v 1.1.1.1 1996/11/29 23:36:29 is Exp $
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

#include "amigaio.h"
#include "amigagraph.h"
#include "amigatypes.h"
#include <sys/types.h>

extern struct ExecBase *SysBase;
extern struct Library *IntuitionBase;
extern struct Library *ExpansionBase;

void *AllocMem (size_t, u_int32_t);
void FreeMem (void *, size_t);

struct Library *OpenLibrary (const char *, u_int32_t);
void CloseLibrary (struct Library *);
struct MsgPort *CreateMsgPort(void);
void *CreateIORequest(struct MsgPort *, u_int32_t);
void DeleteIORequest(void *);
void DeleteMsgPort(struct MsgPort *);

u_int8_t DoIO(struct AmigaIO *);
void SendIO(struct AmigaIO *);
struct AmigaIO *CheckIO(struct AmigaIO *);
void *WaitPort(struct MsgPort *);
void AbortIO(struct AmigaIO *);
u_int8_t WaitIO(struct AmigaIO *);

int OpenDevice(const char *, u_int32_t, struct AmigaIO *, u_int32_t);

void *FindResident(const char *);
void *OpenResource(const char *);

u_int32_t CachePreDMA(u_int32_t, u_int32_t *, int);
#define DMAF_Continue		2
#define DMAF_NoModify		4
#define DMAF_ReadFromRAM	8

void Forbid(void);
void Permit(void);

struct Screen *OpenScreenTagList(struct NewScreen *, const u_int32_t *);
struct Screen *OpenScreenTag(struct NewScreen *, ...);
struct Window *OpenWindowTagList(struct Window *, const u_int32_t *);
struct Window *OpenWindowTag(struct Window *, ...);

#ifdef nomore
u_int32_t mytime(void);
#endif

struct cfdev *FindConfigDev(struct cfdev *, int, int);

#ifndef DOINLINES
void CacheClearU(void);
#else
#define LibCallNone(lib, what)  \
	asm("movl a6,sp@-; movl %0,a6; " what "; movl sp@+,a6" :: \
	    "r"(lib) : "d0", "d1", "a0", "a1")

#define CacheClearU() LibCallNone(SysBase, "jsr a6@(-0x27c)")
#endif
