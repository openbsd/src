/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "krb_locl.h"

RCSID("$KTH: dest_tkt.c,v 1.11.14.1 2000/06/28 19:36:40 assar Exp $");

#ifndef O_BINARY
#define O_BINARY 0
#endif

/*
 * dest_tkt() is used to destroy the ticket store upon logout.
 * If the ticket file does not exist, dest_tkt() returns RET_TKFIL.
 * Otherwise the function returns RET_OK on success, KFAILURE on
 * failure.
 *
 * The ticket file (TKT_FILE) is defined in "krb.h".
 */

int
dest_tkt(void)
{
    const char *filename = TKT_FILE;
    int i, fd;
    struct stat sb1, sb2;
    char buf[BUFSIZ];

    errno = 0;
    if (lstat (filename, &sb1) < 0)
	goto out;

    fd = open (filename, O_RDWR | O_BINARY);
    if (fd < 0)
	goto out;

    if (unlink (filename) < 0) {
	int save_errno = errno;

	close(fd);
	errno = save_errno;
	goto out;
    }

    if (fstat (fd, &sb2) < 0) {
	int save_errno = errno;

	close(fd);
	errno = save_errno;
	goto out;
    }

    if (sb1.st_dev != sb2.st_dev || sb1.st_ino != sb2.st_ino) {
	close (fd);
	errno = EPERM;
	goto out;
    }

    if (sb2.st_nlink != 0) {
	close (fd);
	errno = EPERM;
	goto out;
    }

    for (i = 0; i < sb2.st_size; i += sizeof(buf))
	if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
	    fsync(fd);
	    close(fd);
	    goto out;
	}

    fsync(fd);
    close(fd);
    
out:
    if (errno == ENOENT)
	return RET_TKFIL;
    else if (errno != 0)
	return KFAILURE;
    else
	return(KSUCCESS);
}
