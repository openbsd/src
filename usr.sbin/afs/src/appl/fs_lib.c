/*	$OpenBSD: fs_lib.c,v 1.2 1999/04/30 01:59:04 art Exp $	*/
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
#include <kafs.h>

RCSID("$KTH: fs_lib.c,v 1.22 1999/01/10 02:46:10 assar Exp $");

/*
 *
 */

const char *
fslib_version(void)
{
    return "$KTH: fs_lib.c,v 1.22 1999/01/10 02:46:10 assar Exp $";
}

/*
 * fserr, try to implement a generic function for fs error messages
 */

void
fserr(const char *progname, int error, const char *realpath)
{
    const char *path = realpath ? realpath : "[unknown path]";

    switch(error) {
    case EACCES: 
	fprintf(stderr, "%s: You don't have the required access rights on"
		" '%s'\n", progname, path);
	break;
    case EINVAL: 
	fprintf(stderr, "%s: Invalid argument; it is possible that %s is"
		" not in AFS.\n", progname, path);
	break;
    case ENOENT: 
	fprintf(stderr, "%s: '%s' doesn't exist\n", progname, path);
	break;
    case EPERM: 
	fprintf(stderr, "%s: You do not have the required rights to do"
		" this operation\n", progname); 
	break;
    case ESRCH: 
	fprintf (stderr, "%s: Home cell information not available\n",
		 progname); 
	break;
    case EDOM: 
    default: 
	fprintf(stderr, "%s: error %s (%d) return from pioctl\n",
		progname, koerr_gettext(error), error);
	break;
    }
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

#ifdef VIOC_SETRXKCRYPT
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
#endif

/*
 * get currernt level of crypt
 */

#ifdef VIOC_GETRXKCRYPT
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
#endif

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

#ifdef VIOC_FPRIOSTATUS
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
#endif

#ifdef VIOC_FPRIOSTATUS
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
#endif

#ifdef VIOC_FPRIOSTATUS
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
#endif

#ifdef VIOC_FPRIOSTATUS
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
#endif

/*
 *
 */

int
fs_getfilecachestats(u_int32_t *max_kbytes,
		     u_int32_t *used_kbytes,
		     u_int32_t *max_vnodes,
		     u_int32_t *used_vnodes)
{
    u_int32_t parms[16];
    struct ViceIoctl a_params;

    a_params.in_size  = 0;
    a_params.out_size = sizeof(parms);
    a_params.in       = NULL;
    a_params.out      = (char *) parms;

    memset (parms, 0, sizeof(parms));

    if (k_pioctl (NULL, VIOCGETCACHEPARAMS , &a_params, 0) == -1)
	return errno;

    if (max_kbytes)
	*max_kbytes = parms[0];
    if (used_kbytes)
	*used_kbytes = parms[1];
    if (max_vnodes)
	*max_vnodes = parms[2];
    if (used_vnodes)
	*used_vnodes = parms[3];

    return 0;
}


/*
 *
 */

#ifdef VIOC_AVIATOR
int
fs_getaviatorstats(u_int32_t *max_workers,
		   u_int32_t *used_workers)
{
    u_int32_t parms[16];
    struct ViceIoctl a_params;

    a_params.in_size = 0;
    a_params.out_size = sizeof(parms);
    a_params.in = NULL;
    a_params.out = (char *) parms;

    if (k_pioctl (NULL, VIOC_AVIATOR , &a_params, 0) == -1)
	return errno;

    if (max_workers)
	*max_workers = parms[0];
    if (used_workers)
	*used_workers = parms[1];

    return 0;
}
#endif

/*
 *
 */

#ifdef VIOC_GCPAGS
int
fs_gcpags(void)
{
    struct ViceIoctl a_params;

    a_params.in_size  = 0;
    a_params.out_size = 0;
    a_params.in       = NULL;
    a_params.out      = NULL;


    if (k_pioctl(NULL, VIOC_GCPAGS, &a_params, 0) != 0)
	return errno;
    
    return 0;
}
#endif

/*
 * Get/set debug levels with pioctl_cmd.
 *
 * inflags == -1 -> don't change
 * outflags == NULL -> don't return
 */

static int
debug (int pioctl_cmd, int inflags, int *outflags)
{
    struct ViceIoctl   a_params;

    int32_t rinflags = inflags;
    int32_t routflags;

    if (inflags != -1) {
	a_params.in_size = sizeof(rinflags);
	a_params.in = (char *) &rinflags;
    } else {
	a_params.in_size = 0;
	a_params.in = NULL;
    }
	
    if (outflags) {
	a_params.out_size = sizeof(routflags);
	a_params.out = (char *)  &routflags;
    } else {
	a_params.out_size = 0;
	a_params.out = NULL;
    }

    if (k_pioctl (NULL, pioctl_cmd, &a_params, 0) == -1)
	return errno;
    
    if (outflags)
	*outflags = routflags;

    return 0;
}

/*
 * xfs_debug
 */

#ifdef VIOC_XFSDEBUG
int
xfs_debug(int inflags, int *outflags)
{
    return debug (VIOC_XFSDEBUG, inflags, outflags);
}
#endif

/*
 * xfs_debug_print
 */

#ifdef VIOC_XFSDEBUG_PRINT
int
xfs_debug_print(int inflags)
{
    return debug (VIOC_XFSDEBUG_PRINT, inflags, NULL);
}
#endif

/*
 * arla_debug
 */

#ifdef VIOC_ARLADEBUG
int
arla_debug (int inflags, int *outflags)
{
    return debug (VIOC_ARLADEBUG, inflags, outflags);
}
#endif

/*
 * checkservers
 *
 *   flags is the same flags as in CKSERV flags
 *
 */

int
fs_checkservers(char *cell, int32_t flags, u_int32_t *hosts, int numhosts)
{
    struct ViceIoctl a_params;
    char *in = NULL;
    int ret;
    size_t insize;

    if (cell != NULL) {
	insize = strlen(cell) + sizeof(int32_t) + 1;
	in = malloc (insize);
	if (in == NULL)
	    errx (1, "malloc");

	memcpy (in, &flags, sizeof(flags));

	memcpy (in + sizeof(int32_t), cell, strlen(cell));
	in[sizeof(int32_t) + strlen(cell)] = '\0';
	
	a_params.in_size = insize;
	a_params.in = in;
    } else {
	a_params.in_size = sizeof(flags);
	a_params.in = (caddr_t )&flags;
    }

    a_params.out_size = numhosts * sizeof(u_int32_t);
    a_params.out = (caddr_t)hosts;

    ret = 0;

    if (k_pioctl (NULL, VIOCCKSERV, &a_params, 0) == -1)
	ret = errno;
    
    if (in)
	free(in);

    return ret;
}

/*
 * return current sysname in `sys' (of max length `sys_sz')
 */

int
fs_get_sysname (char *sys, size_t sys_sz)
{
    struct ViceIoctl a_params;
    int32_t set = 0;
    char *buf;

    buf = malloc (sys_sz + 4);
    if (buf == NULL)
	return ENOMEM;

    a_params.in       = (caddr_t)&set;
    a_params.in_size  = sizeof(set);
    a_params.out      = buf;
    a_params.out_size = sys_sz + 4;

    if(k_pioctl (NULL, VIOC_AFS_SYSNAME, &a_params, 1) < 0)
	return errno;
    else {
	strncpy (sys, buf + 4, sys_sz - 1);
	sys[sys_sz - 1] = '\0';
	return 0;
    }
}

/*
 * set current sysname to `sys'
 */

int
fs_set_sysname (const char *sys)
{
    struct ViceIoctl a_params;
    int32_t set = 1;

    a_params.in_size  = sizeof(set) + strlen(sys) + 1;
    a_params.in       = malloc(a_params.in_size);
    if (a_params.in == NULL)
	return ENOMEM;
    a_params.out      = NULL;
    a_params.out_size = 0;
    memcpy (a_params.in, &set, sizeof(set));
    strcpy (a_params.in + sizeof(set), sys);

    if(k_pioctl (NULL, VIOC_AFS_SYSNAME, &a_params, 1) < 0)
	return errno;
    else
	return 0;
}

/*
 *
 */

int
fs_setcache(int lv, int hv, int lb, int hb)
{
    struct ViceIoctl a_params;
    u_int32_t s[4];

    s[0] = lv;
    s[1] = hv;
    s[2] = lb;
    s[3] = hb;

    a_params.in_size  = ((hv == 0) ? 1 : 4) * sizeof(u_int32_t);
    a_params.out_size = 0;
    a_params.in       = (void *)s;
    a_params.out      = NULL;

    if (k_pioctl(NULL, VIOCSETCACHESIZE, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}

/*
 * return the local cell in `cell' (of size `cell_sz').
 */

int
fs_wscell (char *cell, size_t cell_sz)
{
    struct ViceIoctl a_params;

    a_params.in_size  = 0;
    a_params.in       = NULL;
    a_params.out_size = cell_sz;
    a_params.out      = cell;

    if (k_pioctl (NULL, VIOC_GET_WS_CELL, &a_params, 0) < 0)
	return errno;
    return 0;
}

/*
 * Flush the contents of the volume pointed to by `path'.
 */

int
fs_flushvolume (const char *path)
{
    struct ViceIoctl a_params;

    a_params.in_size  = 0;
    a_params.out_size = 0;
    a_params.in       = NULL;
    a_params.out      = NULL;

    if (k_pioctl ((char *)path, VIOC_FLUSHVOLUME, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}

/*
 * Flush the file `path' from the cache.
 */

int
fs_flush (const char *path)
{
    struct ViceIoctl a_params;

    a_params.in_size  = 0;
    a_params.out_size = 0;
    a_params.in       = NULL;
    a_params.out      = NULL;

    if (k_pioctl ((char *)path, VIOCFLUSH, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}

/*
 *
 */

int
fs_venuslog (void)
{
    struct ViceIoctl a_params;
    int32_t status = 0;   /* XXX not really right, but anyway */

    a_params.in_size  = sizeof(int32_t);
    a_params.out_size = 0;
    a_params.in       = (caddr_t) &status;
    a_params.out      = NULL;

    if (k_pioctl (NULL, VIOC_VENUSLOG, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}

/*
 * Create a new cell (or change servers for an existing one), with
 * name `cell' and `nservers' servers in `servers'.
 */

int
fs_newcell (const char *cell, int nservers, char **servers)
{
    struct ViceIoctl a_params;
    int len;
    char *buf;
    int i;
    u_int32_t *hp;

    nservers = min (nservers, 8);

    len = 8 * sizeof(u_int32_t) + strlen(cell) + 1;
    buf = malloc (len);
    if (buf == NULL)
	return errno;

    memset (buf, 0, len);
    strcpy (buf + 8 * sizeof(u_int32_t), cell);
    hp = (u_int32_t *)buf;
    for (i = 0; i < nservers; ++i) {
	struct in_addr addr;

	if (ipgetaddr (servers[i], &addr) == NULL) {
	    free (buf);
	    return EINVAL;
	}
	hp[i] = addr.s_addr;
    }

    a_params.in_size  = len;
    a_params.out_size = 0;
    a_params.in       = (caddr_t)buf;
    a_params.out      = NULL;

    if (k_pioctl (NULL, VIOCNEWCELL, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}

/*
 * Fetch cell status for cell `num', and put the ip-numbers to the servers
 * in the array `server' of length `server_sz'. `Cell' is the name
 * of the cell and has length `sell_sz'.
 */

int
fs_getcells (int32_t num, u_int32_t *server, size_t server_sz,
	     char *cell, size_t cell_sz)
{
    struct ViceIoctl   a_params;
    int32_t *server_list;
    int i;
    
    if (server == NULL && server_sz != 0)
	return EINVAL;

#define GETCELL_MAXSERVER 8
    a_params.in_size = sizeof (num);
    a_params.out_size = sizeof (u_int32_t) * GETCELL_MAXSERVER + cell_sz;
    a_params.in = (char *) &num;
    a_params.out = malloc (a_params.out_size);
    
    if (a_params.out == NULL)
	return ENOMEM;
    
    server_list = (int32_t *) a_params.out;

    if (k_pioctl (NULL, VIOCGETCELL, &a_params, 0) != 0)
	return errno;
    
    for (i = 0 ; i < server_sz; i++)
	server[i] = server_list[i];

    i = a_params.out_size - GETCELL_MAXSERVER * sizeof(u_int32_t);

    strncpy (cell, 
	     (char *) a_params.out +  GETCELL_MAXSERVER * sizeof(u_int32_t),
	     i);
    cell[i-1] = '\0';
    
    return 0;
}

/*
 * Get status for `cell' and put the flags in `flags'.
 */

int
fs_getcellstatus (char *cellname, u_int32_t *flags)
{
    struct ViceIoctl a_params;

    a_params.in_size  = strlen (cellname) + 1;
    a_params.out_size = sizeof (u_int32_t);
    a_params.in       = cellname;
    a_params.out      = (caddr_t) flags;

    if (k_pioctl (NULL, VIOC_GETCELLSTATUS, &a_params, 0) < 0)
	return errno;
    else
	return 0;
}

