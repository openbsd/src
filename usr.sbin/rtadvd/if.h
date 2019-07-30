/*	$OpenBSD: if.h,v 1.14 2017/08/10 19:07:14 jca Exp $	*/
/*	$KAME: if.h,v 1.6 2001/01/21 15:37:14 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, 1998, and 1999 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define RTADV_TYPE2BITMASK(type) (0x1 << type)

extern struct if_msghdr **iflist;

struct nd_opt_hdr;
struct sockaddr_dl *if_nametosdl(char *);
int if_getmtu(char *);
int if_getflags(int, int);
int lladdropt_length(struct sockaddr_dl *);
void lladdropt_fill(struct sockaddr_dl *, struct nd_opt_hdr *);
int validate_msg(char *);
struct in6_addr *get_addr(char *);
int get_rtm_ifindex(char *);
int get_ifm_ifindex(char *);
int get_ifam_ifindex(char *);
int get_ifm_flags(char *);
int get_prefixlen(char *);
int prefixlen(u_char *, u_char *);
int rtmsg_type(char *);
int ifmsg_type(char *);
int rtmsg_len(char *);
void init_iflist(void);
