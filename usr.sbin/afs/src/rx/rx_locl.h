/*
 * Copyright (c) 1998, 1999 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

/* $arla: rx_locl.h,v 1.7 2002/12/20 12:52:14 lha Exp $ */

#ifndef __RX_LOCL_H__
#define __RX_LOCL_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <signal.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/param.h>
#include <sys/file.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/stat.h>
#include <atypes.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <netinet/in.h>
#include <sys/time.h>
#ifdef __osf__ /* brain damage */
struct mbuf;
struct ifaddr;
struct ifmulti;
struct rtentry;
#endif
#include <net/if.h>
#include <sys/ioctl.h>
#include <lwp.h>
#include <unistd.h>
#ifdef	AFS_SUN5_ENV
#include <sys/sysmacros.h> /* ??? */
#endif

#include <roken.h>

#include "rx_mach.h"
#include "rx_user.h"
#include "rx_clock.h"
#include "rx_queue.h"
#include "rx.h"
#include "rx_globs.h"
#include "rx_clock.h"
#include "rx_trace.h"
#include "rx_misc.h"
#include "rx_multi.h"

#endif /* __RX_LOCL_H__ */
