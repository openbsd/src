# $OpenBSD: Makefile.inc,v 1.4 2002/11/05 22:19:55 marc Exp $

.PATH: ${LIBC_RSRCDIR}/thread

SRCS+=	thread_storage.c thread_malloc_lock.c
