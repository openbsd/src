/*	$OpenBSD: freebsd_util.h,v 1.2 1996/08/02 20:34:50 niklas Exp $	*/
/*	$NetBSD: freebsd_util.h,v 1.1 1995/10/10 01:19:38 mycroft Exp $	*/

/*
 * Copyright (c) 1994 Christos Zoulas
 * Copyright (c) 1995 Frank van der Linden
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
 * 3. The name of the author may not be used to endorse or promote products
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
 * from: svr4_util.h,v 1.5 1994/11/18 02:54:31 christos Exp
 * from: linux_util.h,v 1.3 1995/04/07 22:23:27 fvdl Exp
 */

/*
 * This file is pretty much the same as Christos' svr4_util.h
 * This file is pretty much the same as Fvdl's linux_util.h
 * (for now).
 */

#ifndef	_FREEBSD_UTIL_H_
#define	_FREEBSD_UTIL_H_

#include <compat/common/compat_util.h>

extern const char freebsd_emul_path[];

#define FREEBSD_CHECK_ALT_EXIST(p, sgp, path) \
    CHECK_ALT_EXIST(p, sgp, freebsd_emul_path, path)

#define FREEBSD_CHECK_ALT_CREAT(p, sgp, path) \
    CHECK_ALT_CREAT(p, sgp, freebsd_emul_path, path)

#endif /* !_FREEBSD_UTIL_H_ */
