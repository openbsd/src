/*	$OpenBSD: bpf_compile.h,v 1.1 1996/03/25 15:55:31 niklas Exp $	*/

/*
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 * from: NetBSD: pcap.h,v 1.2 1995/03/06 11:39:07 mycroft Exp
 * from: @(#) Header: pcap.h,v 1.15 94/06/14 20:03:34 leres Exp (LBL)
 */

#ifndef _BPF_COMPILE_H
#define _BPF_COMPILE_H

#include <sys/types.h>
#include <sys/time.h>

#include <net/bpf.h>

#define PCAP_ERRBUF_SIZE 256

int	bpf_compile __P((struct bpf_program *, char *, int));
char	*bpf_geterr __P((void));

unsigned int bpf_filter __P((struct bpf_insn *, unsigned char *,
			     unsigned int, unsigned int));

unsigned long	**pcap_nametoaddr __P((const char *));
unsigned long	pcap_nametonetaddr __P((const char *));

int	pcap_nametoport __P((const char *, int *, int *));
int	pcap_nametoproto __P((const char *));
int	pcap_nametopppproto __P((const char *));

/*
 * If a protocol is unknown, PROTO_UNDEF is returned.
 * Also, pcap_nametoport() returns the protocol along with the port number.
 * If there are ambiguous entries in /etc/services (i.e. domain
 * can be either tcp or udp) PROTO_UNDEF is returned.
 */
#define PROTO_UNDEF		-1

unsigned long	__pcap_atoin __P((const char *));

#endif /* _BPF_COMPILE_H */
