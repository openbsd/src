# $OpenBSD: Makefile.inc,v 1.2 2002/11/03 20:36:43 marc Exp $

.PATH: ${LIBC_RSRCDIR}/thread

SRCS+=	thread_storage.c thread_malloc_lock.c
