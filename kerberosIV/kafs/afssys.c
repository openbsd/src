/*	$OpenBSD: afssys.c,v 1.6 1998/09/18 00:58:26 art Exp $	*/
/*	$KTH: afssys.c,v 1.57 1998/05/09 17:19:03 joda Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

#include "kafs_locl.h"

int _kafs_debug;

int
k_pioctl(char *a_path,
	 int o_opcode,
	 struct ViceIoctl *a_paramsP,
	 int a_followSymlinks)
{
    return xfspioctl(AFSCALL_PIOCTL, a_path, o_opcode, a_paramsP,
		     a_followSymlinks);
}

int
k_afs_cell_of_file(const char *path, char *cell, int len)
{
    struct ViceIoctl parms;

    parms.in = NULL;
    parms.in_size = 0;
    parms.out = cell;
    parms.out_size = len;

    return k_pioctl((char*)path, VIOC_FILE_CELL_NAME, &parms, 1);
}

int
k_unlog(void)
{
    struct ViceIoctl parms;

    memset(&parms, 0, sizeof(parms));

    return k_pioctl(0, VIOCUNLOG, &parms, 0);
}

int
k_setpag(void)
{
    return xfspioctl(AFSCALL_SETPAG, NULL, 0, NULL, 0);
}

int
k_hasafs(void)
{
    return xfspioctl(AFSCALL_PROBE, NULL, 0, NULL, 0) == 1;
}
