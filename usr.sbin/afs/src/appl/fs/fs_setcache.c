/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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

#include "fs_local.h"

RCSID("$arla: fs_setcache.c,v 1.1 2001/09/24 23:50:20 mattiasa Exp $");

static void
setcache_usage()
{
    fprintf(stderr, "Usage: setcachesize <lowvnodes> [<highvnodes> "
	    "<lowbytes> <highbytes>]\n");

}

static int
afs_setcache(int lv, int hv, int lb, int hb)
{
    int ret;

    ret = fs_setcache (lv, hv, lb, hb);
    if (ret)
	fserr(PROGNAME, ret, ".");
    return ret;
}

int
setcache_cmd (int argc, char **argv)
{
    int lv, hv, lb, hb;

    argc--;
    argv++;

    if (argc != 1 && argc != 4) {
	setcache_usage();
	return 0;
    }
    
    if (argc == 4) {
	if (sscanf(argv[0], "%d", &lv) &&
	    sscanf(argv[1], "%d", &hv) &&
	    sscanf(argv[2], "%d", &lb) &&
	    sscanf(argv[3], "%d", &hb))
	    afs_setcache(lv, hv, lb, hb);
	else
	    setcache_usage();
    } else {
	if (sscanf(argv[0], "%d", &lv))
	    afs_setcache(lv, 0, 0, 0);
	else 
	    setcache_usage();
    }

    return 0;
}
