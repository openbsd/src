/*
 * $OpenBSD: amigagraph.h,v 1.2 2001/08/12 12:03:02 heko Exp $
 * $NetBSD: amigagraph.h,v 1.1.1.1 1996/11/29 23:36:29 is Exp $
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

#ifndef AMIGA_GRAPH_H
#define AMIGA_GRAPH_H

#define SA_Title	0x80000028
#define SA_Colors	0x80000029

#define SA_Sysfont	0x8000002C
#define SA_Type		0x8000002D

#define SA_DisplayID	0x80000032
#define SA_ShowTitle	0x80000036
#define SA_Quiet	0x80000038
#define SA_AutoScroll	0x80000039
#define SA_Pens		0x8000003A

#define PUBLICSCREEN 2
#define CUSTOMSCREEN 0xF

#define	WA_Left		0x80000064
#define	WA_Top		0x80000065
#define	WA_Width	0x80000066
#define	WA_Height	0x80000067
#define WA_DetailPen	0x80000068
#define WA_BlockPen	0x80000069
#define WA_IDCMP	0x8000006A
#define WA_Flags	0x8000006B
#define WA_Gadgets	0x8000006C
#define WA_Checkmark	0x8000006D
#define WA_Title	0x8000006E
#define WA_ScreenTitle	0x8000006F
#define WA_CustomScreen	0x80000070
#define WA_SuperBitMap	0x80000071
#define WA_MinWidth	0x80000072
#define WA_MinHeight	0x80000073
#define WA_MaxWidth	0x80000074
#define WA_MaxHeight	0x80000075
#define WA_Backdrop	0x80000085
#define WA_Borderless	0x80000088
#define WA_Activate	0x80000089
#define WA_AutoAdjust	0x80000090

struct Window {
	u_int8_t dum1[136];
};
#endif /* AMIGA_GRAPH_H */
