#	$OpenBSD: Makefile.inc,v 1.9 2015/04/07 01:27:07 guenther Exp $

.PATH: ${LIBCSRCDIR}/thread

SRCS+=	unithread_malloc_lock.c unithread_mutex.c unithread_tag.c atfork.c
