/*	$OpenBSD: mulaw.c,v 1.2 1997/11/07 08:06:36 niklas Exp $	*/

/*
 * Copyright (c) 1991-1993 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
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

#include <sys/types.h>
#include <sys/audioio.h>
#include <dev/mulaw.h>

static u_char mulawtolin[256] = {
	128, 4, 8, 12, 16, 20, 24, 28, 
	32, 36, 40, 44, 48, 52, 56, 60, 
	64, 66, 68, 70, 72, 74, 76, 78, 
	80, 82, 84, 86, 88, 90, 92, 94, 
	96, 97, 98, 99, 100, 101, 102, 103, 
	104, 105, 106, 107, 108, 109, 110, 111, 
	112, 112, 113, 113, 114, 114, 115, 115, 
	116, 116, 117, 117, 118, 118, 119, 119, 
	120, 120, 120, 121, 121, 121, 121, 122, 
	122, 122, 122, 123, 123, 123, 123, 124, 
	124, 124, 124, 124, 125, 125, 125, 125, 
	125, 125, 125, 125, 126, 126, 126, 126, 
	126, 126, 126, 126, 126, 126, 126, 126, 
	127, 127, 127, 127, 127, 127, 127, 127, 
	127, 127, 127, 127, 127, 127, 127, 127, 
	127, 127, 127, 127, 127, 127, 127, 127, 
	255, 251, 247, 243, 239, 235, 231, 227, 
	223, 219, 215, 211, 207, 203, 199, 195, 
	191, 189, 187, 185, 183, 181, 179, 177, 
	175, 173, 171, 169, 167, 165, 163, 161, 
	159, 158, 157, 156, 155, 154, 153, 152, 
	151, 150, 149, 148, 147, 146, 145, 144, 
	143, 143, 142, 142, 141, 141, 140, 140, 
	139, 139, 138, 138, 137, 137, 136, 136, 
	135, 135, 135, 134, 134, 134, 134, 133, 
	133, 133, 133, 132, 132, 132, 132, 131, 
	131, 131, 131, 131, 130, 130, 130, 130, 
	130, 130, 130, 130, 129, 129, 129, 129, 
	129, 129, 129, 129, 129, 129, 129, 129, 
	128, 128, 128, 128, 128, 128, 128, 128, 
	128, 128, 128, 128, 128, 128, 128, 128, 
	128, 128, 128, 128, 128, 128, 128, 128, 
};

static u_char lintomulaw[256] = {
	0, 0, 0, 0, 0, 1, 1, 1, 
	1, 2, 2, 2, 2, 3, 3, 3, 
	3, 4, 4, 4, 4, 5, 5, 5, 
	5, 6, 6, 6, 6, 7, 7, 7, 
	7, 8, 8, 8, 8, 9, 9, 9, 
	9, 10, 10, 10, 10, 11, 11, 11, 
	11, 12, 12, 12, 12, 13, 13, 13, 
	13, 14, 14, 14, 14, 15, 15, 15, 
	15, 16, 16, 17, 17, 18, 18, 19, 
	19, 20, 20, 21, 21, 22, 22, 23, 
	23, 24, 24, 25, 25, 26, 26, 27, 
	27, 28, 28, 29, 29, 30, 30, 31, 
	31, 32, 33, 34, 35, 36, 37, 38, 
	39, 40, 41, 42, 43, 44, 45, 46, 
	47, 48, 50, 52, 54, 56, 58, 60, 
	62, 65, 69, 73, 77, 83, 91, 103, 
	255, 231, 219, 211, 205, 201, 197, 193, 
	190, 188, 186, 184, 182, 180, 178, 176, 
	175, 174, 173, 172, 171, 170, 169, 168, 
	167, 166, 165, 164, 163, 162, 161, 160, 
	159, 159, 158, 158, 157, 157, 156, 156, 
	155, 155, 154, 154, 153, 153, 152, 152, 
	151, 151, 150, 150, 149, 149, 148, 148, 
	147, 147, 146, 146, 145, 145, 144, 144, 
	143, 143, 143, 143, 142, 142, 142, 142, 
	141, 141, 141, 141, 140, 140, 140, 140, 
	139, 139, 139, 139, 138, 138, 138, 138, 
	137, 137, 137, 137, 136, 136, 136, 136, 
	135, 135, 135, 135, 134, 134, 134, 134, 
	133, 133, 133, 133, 132, 132, 132, 132, 
	131, 131, 131, 131, 130, 130, 130, 130, 
	129, 129, 129, 129, 128, 128, 128, 128, 
};

void
mulaw_compress(hdl, e, p, cc)
	void *hdl;
	int e;
	u_char *p;
	int cc;
{
	u_char *tab;

	switch (e) {
	case AUDIO_ENCODING_ULAW:
		tab = lintomulaw;
		break;
	default:
		return;
	}

	while (--cc >= 0) {
		*p = tab[*p];
		++p;
	}
}

void
mulaw_expand(hdl, e, p, cc)
	void *hdl;
	int e;
	u_char *p;
	int cc;
{
	u_char *tab;

	switch (e) {
	case AUDIO_ENCODING_ULAW:
		tab = mulawtolin;
		break;
	default:
		return;
	}
	
	while (--cc >= 0) {
		*p = tab[*p];
		++p;
	}
}

