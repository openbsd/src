/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
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
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <rx/rx.h>
#include <rx/rx_null.h>

#include <aafs/aafs_security.h>
#include <aafs/aafs_cell.h>
#include <aafs/aafs_private.h>

struct aafs_security {
    struct aafs_object obj;
    aafs_sec_type type;
    struct rx_securityClass *secobj;
    int secidx;
};

static void
sec_destruct(void *ptr, const char *name)
{
    struct aafs_security *s = ptr;
    switch(s->type) {
    case AAFS_SEC_ANY:
	break;
    default:
	exit(-1);
    }
}


struct aafs_security *
aafs_security_create(aafs_sec_type type, const char *cell)
{
    struct aafs_security *sec;
    struct rx_securityClass *secobj;
    int secidx;

    switch(type) {
    case AAFS_SEC_NULL:
    case AAFS_SEC_ANY:
	secidx = 0;
	secobj = rxnull_NewClientSecurityObject();
	type = AAFS_SEC_NULL;
	break;
    case AAFS_SEC_KAD:
    case AAFS_SEC_GSS:
	return NULL;
    }

    sec = aafs_object_create(sizeof(*sec), "sec", sec_destruct);

    sec->type = type;
    sec->secidx = secidx;
    sec->secobj = secobj;

    return sec;
}

int
aafs_security_rx_secclass(struct aafs_security *sec,
			  void *secobj, int *secidx)
{
    struct rx_securityClass **rxsecobj = secobj;

    *rxsecobj = sec->secobj;
    *secidx = sec->secidx;

    return 0;
}
