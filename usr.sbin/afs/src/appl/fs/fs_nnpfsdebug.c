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

RCSID("$arla: fs_nnpfsdebug.c,v 1.2 2002/09/07 10:42:18 lha Exp $");

static const unsigned all = (0
#ifdef HAVE_XDEBDEV
			     | XDEBDEV
#endif
#ifdef HAVE_XDEBMSG
			     | XDEBMSG
#endif
#ifdef HAVE_XDEBDNLC
			     | XDEBDNLC
#endif
#ifdef HAVE_XDEBNODE
			     | XDEBNODE
#endif
#ifdef HAVE_XDEBVNOPS
			     | XDEBVNOPS
#endif
#ifdef HAVE_XDEBVFOPS
			     | XDEBVFOPS
#endif
#ifdef HAVE_XDEBLKM
			     | XDEBLKM
#endif
#ifdef HAVE_XDEBSYS
			     | XDEBSYS
#endif
#ifdef HAVE_XDEBMEM
			     | XDEBMEM
#endif
#ifdef HAVE_XDEBREADDIR
			     | XDEBREADDIR
#endif
#ifdef HAVE_XDEBLOCK
			     | XDEBLOCK
#endif
#ifdef HAVE_XDEBCACHE
			     | XDEBCACHE
#endif
    );

static const unsigned almost_all_mask = (0
#ifdef HAVE_XDEBDEV
					 | XDEBDEV
#endif
#ifdef HAVE_XDEBMEM
					 | XDEBMEM
#endif	       
#ifdef HAVE_XDEBREADDIR
					 | XDEBREADDIR
#endif
    );

static struct units nnpfsdebug_units[] = {
    {"all",		0},
    {"almost-all",  0},
#ifdef HAVE_XDEBCACHE
    {"cache",	XDEBCACHE},
#endif
#ifdef HAVE_XDEBLOCK
    {"lock",	XDEBLOCK},
#endif
#ifdef HAVE_XDEBREADDIR
    {"readdir",	XDEBREADDIR},
#endif
#ifdef HAVE_XDEBMEM
    {"mem",		XDEBMEM},
#endif
#ifdef HAVE_XDEBSYS
    {"sys",		XDEBSYS},
#endif
#ifdef HAVE_XDEBLKM
    {"lkm",		XDEBLKM},
#endif
#ifdef HAVE_XDEBVFOPS
    {"vfsops",	XDEBVFOPS},
#endif
#ifdef HAVE_XDEBVNOPS
    {"vnops",	XDEBVNOPS},
#endif
#ifdef HAVE_XDEBNODE
    {"node",	XDEBNODE},
#endif
#ifdef HAVE_XDEBDNLC
    {"dnlc",	XDEBDNLC},
#endif
#ifdef HAVE_XDEBMSG
    {"msg",		XDEBMSG},
#endif
#ifdef HAVE_XDEBDEV
    {"dev",		XDEBDEV},
#endif
    {"none",	0 },
    { NULL, 0 }
};

int
nnpfsdebug_cmd (int argc, char **argv)
{
    int ret;
    int flags;

    nnpfsdebug_units[0].mult = all;
    nnpfsdebug_units[1].mult = all & ~almost_all_mask;

    if ((argc > 1 && strncmp(argv[1], "-h", 2) == 0) || argc > 2) {
	fprintf (stderr, "usage: nnpfsdebug [-h] [");
	print_flags_table (nnpfsdebug_units, stderr);
	fprintf (stderr, "]\n");
	return 0;
    }

    ret = nnpfs_debug (-1, &flags);
    if (ret) {
	fprintf (stderr, "nnpfs_debug: %s\n", strerror(ret));
	return 0;
    }

    if (argc == 1) {
	char buf[1024];

	unparse_flags (flags, nnpfsdebug_units, buf, sizeof(buf));
	printf("nnpfsdebug is: %s\n", buf);
    } else if (argc == 2) {
	char *textflags;
	
	textflags = argv[1];
    
	ret = parse_flags (textflags, nnpfsdebug_units, flags);
	
	if (ret < 0) {
	    fprintf (stderr, "nnpfsdebug: unknown/bad flags `%s'\n",
		     textflags);
	    return 0;
	}
	
	flags = ret;
	ret = nnpfs_debug(flags, NULL);
	if (ret)
	    fprintf (stderr, "nnpfs_debug: %s\n", strerror(ret));
    }
    return 0;
}

int
nnpfsdebug_print_cmd (int argc, char **argv)
{
    int ret;
    int flags = 0;
    char *textflags;

    nnpfsdebug_units[0].mult = all;
    nnpfsdebug_units[1].mult = all & ~almost_all_mask;

    if (argc != 2 && argc != 3) {
	fprintf (stderr, "usage: nnpfsprint <");
	print_flags_table (nnpfsdebug_units, stderr);
	fprintf (stderr, "> [filename]\n");
	return 0;
    }

    textflags = argv[1];
    
    ret = parse_flags (textflags, nnpfsdebug_units, flags);
    
    if (ret < 0) {
	fprintf (stderr, "nnpfsprint: unknown/bad flags `%s'\n",
		 textflags);
	return 0;
    }
    
    flags = ret;
    ret = nnpfs_debug_print(flags, argv[2]);
    if (ret)
	fprintf (stderr, "nnpfsprint: %s\n", strerror(ret));
    return 0;
}

