/*
 * Copyright (c) 1998 - 2000 Kungliga Tekniska Högskolan
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

/* $arla: fs_errors.h,v 1.12 2003/03/06 00:39:29 lha Exp $ */

#ifndef _FS_ERRORS_H_
#define _FS_ERRORS_H_ 1

#include <rx/rx.h>
#include <uae.h>

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

#define VICE_SPECIAL_ERRORS	101

#define VICE_SPECIAL_ERRORS_MIN	101
#define VICE_SPECIAL_ERRORS_MAX	112

#define VSALVAGE		101
#define VNOVNODE		102
#define VNOVOL			103
#define VVOLEXISTS		104
#define VNOSERVICE		105
#define VOFFLINE		106
#define VONLINE			107
#define VDISKFULL		108
#define VOVERQUOTA		109
#define VBUSY			110
#define VMOVED			111
#define VIO			112

#define VRESTARTING		-100

#define ARLA_SPECIAL_ERROR_ADD	  4000
#define ARLA_SPECIAL2_ERROR_ADD	  4600
#define ARLA_SPECIAL_RX_ERROR_ADD 4800 /* ->4899 */

#define ARLA_VSALVAGE		(ARLA_SPECIAL_ERROR_ADD+VSALVAGE)
#define ARLA_VNOVNODE		(ARLA_SPECIAL_ERROR_ADD+VNOVNODE)
#define ARLA_VNOVOL		(ARLA_SPECIAL_ERROR_ADD+VNOVOL)
#define ARLA_VVOLEXISTS		(ARLA_SPECIAL_ERROR_ADD+VVOLEXISTS)
#define ARLA_VNOSERVICE		(ARLA_SPECIAL_ERROR_ADD+VNOSERVICE)
#define ARLA_VOFFLINE		(ARLA_SPECIAL_ERROR_ADD+VOFFLINE)
#define ARLA_VONLINE		(ARLA_SPECIAL_ERROR_ADD+VONLINE)
#define ARLA_VDISKFULL		(ARLA_SPECIAL_ERROR_ADD+VDISKFULL)
#define ARLA_VOVERQUOTA		(ARLA_SPECIAL_ERROR_ADD+VOVERQUOTA)
#define ARLA_VBUSY		(ARLA_SPECIAL_ERROR_ADD+VBUSY)
#define ARLA_VMOVED		(ARLA_SPECIAL_ERROR_ADD+VMOVED)
#define ARLA_VIO		(ARLA_SPECIAL_ERROR_ADD+VIO)

#define ARLA_VRESTARTING	(ARLA_SPECIAL2_ERROR_ADD+VRESTARTING)

#define ARLA_CALL_DEAD		(ARLA_SPECIAL_RX_ERROR_ADD-RX_CALL_DEAD)
#define ARLA_INVALID_OPERATION	(ARLA_SPECIAL_RX_ERROR_ADD-RX_INVALID_OPERATION)
#define ARLA_CALL_TIMEOUT	(ARLA_SPECIAL_RX_ERROR_ADD-RX_CALL_TIMEOUT)
#define ARLA_EOF		(ARLA_SPECIAL_RX_ERROR_ADD-RX_EOF)
#define ARLA_PROTOCOL_ERROR	(ARLA_SPECIAL_RX_ERROR_ADD-RX_PROTOCOL_ERROR)
#define ARLA_USER_ABORT		(ARLA_SPECIAL_RX_ERROR_ADD-RX_USER_ABORT)
#define ARLA_ADDRINUSE		(ARLA_SPECIAL_RX_ERROR_ADD-RX_ADDRINUSE)
#define ARLA_MSGSIZE		(ARLA_SPECIAL_RX_ERROR_ADD-RX_MSGSIZE)

static inline int __attribute__ ((unused))
conv_to_arla_errno(int error)
{
    if (error >= VICE_SPECIAL_ERRORS_MIN &&
	error <= VICE_SPECIAL_ERRORS_MAX)
	return error + ARLA_SPECIAL_ERROR_ADD;
    else if (error == VRESTARTING)
	return ARLA_VRESTARTING;
    else if (error <= RX_MIN_ERROR && error >= RX_MAX_ERROR)
	return ARLA_SPECIAL_RX_ERROR_ADD - error;    /* RX code are negative */
    else
	return uae_error_to_errno(error);
}

#endif /* _FS_ERRORS_H_ */
