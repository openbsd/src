/*
 * Copyright (c) 1999 - 2002 Kungliga Tekniska Högskolan
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

/*
 * $arla: fsrv_locl.h,v 1.15 2002/04/20 15:57:17 lha Exp $
 */


#ifndef __FILBUNKE_FSRV_H
#define __FILBUNKE_FSRV_H 1

#include <config.h>

#include <roken.h>

#include <sys/types.h>
#include <unistd.h>

#include <err.h>
#include <assert.h>

#include <rx/rx.h>
#include <rx/rx_null.h>

#ifdef KERBEROS
#ifdef HAVE_OPENSSL
#include <openssl/des.h>
#else
#include <des.h>
#endif
#include <krb.h>
#include <rxkad.h>
#endif

#include <ports.h>
#include <msecurity.h>
#include <netinit.h>
#include <ko.h>
#include <service.h>
#include <part.h>

#include <agetarg.h>

#include <fs.ss.h>
#include <volumeserver.ss.h>

#include "fs_def.h"

#include <pts.h>
#include "connsec.h"

#include <dpart.h>
#include <voldb.h>
#include <vld.h>

#include <fbuf.h>
#include <fdir.h>
#include <mdir.h>

#include <ropa.h>

#include <mlog.h>
#include <mdebug.h>

#include <salvage.h>

#include <dump.h>

#endif /* __FILBUNKE_FSRV_H */
