/*	$OpenBSD: fs_lib.c,v 1.1.1.1 1998/09/14 21:52:53 art Exp $	*/
/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
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

#include "appl_locl.h"
#include <kerberosIV/kafs.h>

RCSID("$KTH: fs_lib.c,v 1.4 1998/07/24 19:55:51 lha Exp $");

/*
 *
 */

const char *
fslib_version(void)
{
    return "$KTH: fs_lib.c,v 1.4 1998/07/24 19:55:51 lha Exp $";
}

/*
 * fserr, try to implement a generic function for fs error messages
 */

void
fserr(char *progname, int error, char *realpath)
{
    char *path = realpath ? realpath : "[unknown path]";

    switch(error) {
    case EACCES: 
	printf("%s: You don't have the required access rights on"
	       " '%s'\n",progname, path); return ;
	break;
    case EINVAL: 
	printf("%s: Invalid argument; it is possible that %s is"
	       " not in AFS.\n", progname, path); return ;
	break;
    case ENOENT: 
	printf("%s: '%s' doesn't exist\n",progname, path); 
	break;
    case EPERM: 
	printf("%s: You do not have the required rights to do"
	       " this operation\n", progname); 
	break;
    case ESRCH: 
	printf ("%s: Home cell information not available\n", progname); 
	break;
    case EDOM: 
	break;
    default: 
	printf("%s: error %s (%d) return from pioctl\n",
	       progname, koerr_gettext(error), error); 
	break;
    }
    return;
}

/*
 * fs_getfid, the the `fid' that `path' points on. 
 */

int
fs_getfid(char *path, VenusFid *fid)
{
    struct ViceIoctl a_params;

    if (path == NULL || fid == NULL)
	return EINVAL;

    a_params.in_size=0;
    a_params.out_size=sizeof(*fid);
    a_params.in=NULL;
    a_params.out=(void*) fid;
    
    if(k_pioctl(path,VIOCGETFID,&a_params,1) == -1)
	return errno;

    return 0;
}

/*
 * Do nothing
 */

int
fs_nop(void)
{
    struct ViceIoctl a_params;

    a_params.in_size=0;
    a_params.out_size=0;
    a_params.in=NULL;
    a_params.out=NULL;
    
    if (k_pioctl(NULL,VIOCNOP,&a_params,1) == -1) 
	return errno;

    return 0;
}

/*
 * Get the `cell' that the `path' ends up in
 */

int
fs_getfilecellname(char *path, char *cell, size_t len)
{
    struct ViceIoctl a_params;

    a_params.in_size=0;
    a_params.out_size=len;
    a_params.in=NULL;
    a_params.out=cell;
    
    if (k_pioctl(path,VIOC_FILE_CELL_NAME,&a_params,1) == -1) 
	return errno;

    return 0;
}

/*
 * set the level of crypt
 */

int
fs_setcrypt (u_int32_t n)
{
    struct ViceIoctl	a_params;

    a_params.in_size  = sizeof(n);
    a_params.out_size = 0;
    a_params.in	      = (char *)&n;
    a_params.out      = NULL;

    if (k_pioctl (NULL, VIOC_SETRXKCRYPT, &a_params, 0) == -1)
	return errno;

    return 0;
}

/*
 * get currernt level of crypt
 */

int
fs_getcrypt (u_int32_t *level)
{
    struct ViceIoctl	a_params;

    a_params.in_size  = 0;
    a_params.out_size = sizeof(*level);
    a_params.in	      = NULL;
    a_params.out      = (char *) level;

    if (k_pioctl (NULL, VIOC_GETRXKCRYPT, &a_params, 0) == -1) 
	return errno;
    
    return 0;
}

/*
 * get and set the connect-mode
 */

int
fs_connect(int32_t type, int32_t *flags)
{
    struct ViceIoctl   a_params;

    a_params.in_size = sizeof(type);
    a_params.out_size = sizeof (int32_t);
    a_params.in = (char *) &type;
    a_params.out = (char *) flags;

    if (k_pioctl (NULL, VIOCCONNECTMODE, &a_params, 0) == -1)
	return errno;

    return 0;
}

/*
 *
 */

int
fs_setfprio(VenusFid fid, int16_t prio)
{
    struct ViceIoctl   a_params;
    struct vioc_fprio  fprio;

    fprio.cmd = FPRIO_SET;
    fprio.Cell = fid.Cell;
    fprio.Volume = fid.fid.Volume;
    fprio.Vnode = fid.fid.Vnode;
    fprio.Unique = fid.fid.Unique;
    fprio.prio = prio;

    a_params.in_size = sizeof(fprio);
    a_params.out_size = 0;
    a_params.in = (char *) &fprio;
    a_params.out = NULL;

    if (k_pioctl (NULL, VIOC_FPRIOSTATUS , &a_params, 0) == -1)
	return errno;

    return 0;
}

int
fs_getfprio(VenusFid fid, int16_t *prio)
{
    struct ViceIoctl   a_params;
    struct vioc_fprio  fprio;

    fprio.cmd = FPRIO_GET;
    fprio.Cell = fid.Cell;
    fprio.Volume = fid.fid.Volume;
    fprio.Vnode = fid.fid.Vnode;
    fprio.Unique = fid.fid.Unique;

    a_params.in_size = sizeof(fprio);
    a_params.out_size = sizeof(*prio);
    a_params.in = (char *) &fprio;
    a_params.out = (char *) prio;

    if (k_pioctl (NULL, VIOC_FPRIOSTATUS , &a_params, 0) == -1)
	return errno;

    return 0;
}

int
fs_setmaxfprio(int16_t maxprio)
{
    struct ViceIoctl   a_params;
    struct vioc_fprio  fprio;

    fprio.cmd = FPRIO_SETMAX;
    fprio.prio = maxprio;

    a_params.in_size = sizeof(fprio);
    a_params.out_size = 0;
    a_params.in = (char *) &fprio;
    a_params.out = NULL;

    if (k_pioctl (NULL, VIOC_FPRIOSTATUS , &a_params, 0) == -1)
	return errno;

    return 0;
}

int
fs_getmaxfprio(int16_t *maxprio)
{
    struct ViceIoctl   a_params;
    struct vioc_fprio  fprio;

    fprio.cmd = FPRIO_GETMAX;

    a_params.in_size = sizeof(fprio);
    a_params.out_size = sizeof(*maxprio);
    a_params.in = (char *) &fprio;
    a_params.out = (char *) maxprio;

    if (k_pioctl (NULL, VIOC_FPRIOSTATUS , &a_params, 0) == -1)
	return errno;

    return 0;
}


