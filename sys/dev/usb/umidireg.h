/*	$OpenBSD: umidireg.h,v 1.5 2004/10/01 04:08:46 jsg Exp $	*/
/*	$NetBSD: umidireg.h,v 1.3 2003/12/04 13:57:31 keihan Exp $	*/
/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Takuya SHIOZAKI (tshiozak@NetBSD.org).
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
 *	  This product includes software developed by the NetBSD
 *	  Foundation, Inc. and its contributors.
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

/* Jack Descriptor */
#define UMIDI_MS_HEADER	0x01
#define UMIDI_IN_JACK	0x02
#define UMIDI_OUT_JACK	0x03

/* Jack Type */
#define UMIDI_EMBEDDED	0x01
#define UMIDI_EXTERNAL	0x02

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uWord		bcdMSC;
	uWord		wTotalLength;
} UPACKED umidi_cs_interface_descriptor_t;
#define UMIDI_CS_INTERFACE_DESCRIPTOR_SIZE 7

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubType;
	uByte		bNumEmbMIDIJack;
} UPACKED umidi_cs_endpoint_descriptor_t;
#define UMIDI_CS_ENDPOINT_DESCRIPTOR_SIZE 4

typedef struct {
	uByte		bLength;
	uByte		bDescriptorType;
	uByte		bDescriptorSubtype;
	uByte		bJackType;
	uByte		bJackID;
} UPACKED umidi_jack_descriptor_t;
#define	UMIDI_JACK_DESCRIPTOR_SIZE	5


#define TO_D(p) ((usb_descriptor_t *)(p))
#define NEXT_D(desc) TO_D((caddr_t)(desc)+(desc)->bLength)
#define TO_IFD(desc) ((usb_interface_descriptor_t *)(desc))
#define TO_CSIFD(desc) ((umidi_cs_interface_descriptor_t *)(desc))
#define TO_EPD(desc) ((usb_endpoint_descriptor_t *)(desc))
#define TO_CSEPD(desc) ((umidi_cs_endpoint_descriptor_t *)(desc))
