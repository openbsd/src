/* $OpenBSD: misc.h,v 1.2 2001/09/19 10:58:08 mpech Exp $ */

/*
 * Miscellaneous syscall wrappers.
 */

#ifndef _POP_MISC_H
#define _POP_MISC_H

/*
 * A select(2)-based sleep() equivalent: no more problems with SIGALRM,
 * subsecond precision.
 */
extern int sleep_select(int sec, int usec);

/*
 * Obtain or remove a lock.
 */
extern int lock_fd(int fd, int shared);
extern int unlock_fd(int fd);

/*
 * Attempts to write all the supplied data. Returns the number of bytes
 * written. Any value that differs from the requested count means that
 * an error has occurred; if the value is -1, errno is set appropriately.
 */
extern int write_loop(int fd, char *buffer, int count);

#endif
