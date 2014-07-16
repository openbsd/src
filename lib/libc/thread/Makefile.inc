#	$OpenBSD: Makefile.inc,v 1.8 2014/07/16 20:02:17 okan Exp $

.PATH: ${LIBCSRCDIR}/thread

SRCS+=	unithread_malloc_lock.c unithread_mutex.c unithread_tag.c
