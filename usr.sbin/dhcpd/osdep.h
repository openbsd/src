/* osdep.h

   Operating system dependencies... */

/*
 * Copyright (c) 1996, 1997, 1998, 1999 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE INTERNET SOFTWARE CONSORTIUM OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software was written for the Internet Software Consortium by Ted Lemon
 * under a contract with Vixie Laboratories.
 */
#define USE_BPF_SEND
#define USE_BPF_RECEIVE
#define PACKET_ASSEMBLY
#define PACKET_DECODING

/* jmp_buf is assumed to be a struct unless otherwise defined in the
   system header. */
#ifndef jbp_decl
# define jbp_decl(x)	jmp_buf *x
#endif
#ifndef jref
# define jref(x)	(&(x))
#endif
#ifndef jdref
# define jdref(x)	(*(x))
#endif
#ifndef jrefproto
# define jrefproto	jmp_buf *
#endif

#ifndef BPF_FORMAT
#define BPF_FORMAT "/dev/bpf%d"
#endif

#if defined (IFF_POINTOPOINT) && !defined (HAVE_IFF_POINTOPOINT)
# define HAVE_IFF_POINTOPOINT
#endif

#if defined (AF_LINK) && !defined (HAVE_AF_LINK)
# define HAVE_AF_LINK
#endif

#if defined (ARPHRD_TUNNEL) && !defined (HAVE_ARPHRD_TUNNEL)
# define HAVE_ARPHRD_TUNNEL
#endif

#if defined (ARPHRD_LOOPBACK) && !defined (HAVE_ARPHRD_LOOPBACK)
# define HAVE_ARPHRD_LOOPBACK
#endif

#if defined (ARPHRD_ROSE) && !defined (HAVE_ARPHRD_ROSE)
# define HAVE_ARPHRD_ROSE
#endif

#if defined (ARPHRD_IEEE802) && !defined (HAVE_ARPHRD_IEEE802)
# define HAVE_ARPHRD_IEEE802
#endif

#if defined (ARPHRD_FDDI) && !defined (HAVE_ARPHRD_FDDI)
# define HAVE_ARPHRD_FDDI
#endif

#if defined (ARPHRD_AX25) && !defined (HAVE_ARPHRD_AX25)
# define HAVE_ARPHRD_AX25
#endif

#if defined (ARPHRD_NETROM) && !defined (HAVE_ARPHRD_NETROM)
# define HAVE_ARPHRD_NETROM
#endif

#if defined (ARPHRD_METRICOM) && !defined (HAVE_ARPHRD_METRICOM)
# define HAVE_ARPHRD_METRICOM
#endif

#if defined (SO_BINDTODEVICE) && !defined (HAVE_SO_BINDTODEVICE)
# define HAVE_SO_BINDTODEVICE
#endif

#if defined (SIOCGIFHWADDR) && !defined (HAVE_SIOCGIFHWADDR)
# define HAVE_SIOCGIFHWADDR
#endif

#if defined (AF_LINK) && !defined (HAVE_AF_LINK)
# define HAVE_AF_LINK
#endif
