/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999, 2000 Kungliga Tekniska Högskolan
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <atypes.h>

#include <kafs.h>

#include <fs.h>
#include <arlalib.h>

#include <err.h>
#include <roken.h>

#ifdef RCSID
RCSID("$KTH: create-stat.c,v 1.5 2000/10/03 00:33:33 lha Exp $");
#endif

#ifdef KERBEROS

static void
usage (int ret)
{
    fprintf (stderr, "%s file\n", __progname);
    exit (ret);
}

int
main(int argc, char **argv)
{
    char *file;
    int ret;
    struct stat sb;
    struct stat sb_new;
    struct stat sb_old;
    VenusFid fid;
    char *filename;
    ino_t afsfileid;

    set_progname (argv[0]);

    if (!k_hasafs())
	errx (1, "!k_hasafs");

    if (argc != 2)
	usage (1);

    file = argv[1];

    asprintf (&filename, "%s.new", file);

    ret = open (file, O_RDWR, 0600);
    if (ret < 0)
	err (1, "open");
    close (ret);

    ret = open (filename, O_RDWR|O_CREAT|O_EXCL, 0600);
    if (ret < 0) {
	unlink(file);
	err (1, "open");
    }
    close (ret);

    ret = stat (file, &sb);
    if (ret < 0) {
	unlink(filename);
	unlink(file);
	err (1, "stat");
    }

    ret = lstat (filename, &sb_new);
    if (ret < 0) {
	unlink(filename);
	unlink(file);
	err (1, "stat");
    }

    if (sb.st_ino == sb_new.st_ino)
	err (1, "sb.st_ino == sb_new.st_ino");

    ret = lstat (file, &sb_old);
    if (ret < 0) {
	unlink(filename);
	unlink(file);
	err (1, "stat");
    }

    if (sb_old.st_ino == sb_new.st_ino)
	err (1, "sb_old.st_ino == sb_new.st_ino");
    if (sb.st_ino == sb_new.st_ino)
	err (1, "sb.st_ino == sb_new.st_ino");
    if (sb_old.st_ino != sb.st_ino)
	err (1, "sb_old.st_ino != sb.st_ino");

    ret = fs_getfid (file, &fid);
    if (ret) {
	unlink(file);
	unlink(filename);
	err (1, "fs_getfid: %d", ret);
    }

    afsfileid = ((fid.fid.Volume & 0x7FFF) << 16 | (fid.fid.Vnode & 0xFFFFFFFF));
    if (sb.st_ino != afsfileid) {
	unlink(file);
	unlink(filename);
	errx (1, "sb.st_ino(%ld) != afsfileid(%ld) (%d.%d.%d.%d)",
	      (long)sb.st_ino, (long)afsfileid,
	      fid.Cell, fid.fid.Volume, fid.fid.Vnode, fid.fid.Unique);
    }

    unlink(filename);
    unlink(file);

    return 0;
}

#else /* !KERBEROS */

int
main (int argc, char **argv)
{
    set_progname (argv[0]);

    errx (1, "no kafs");
}

#endif
