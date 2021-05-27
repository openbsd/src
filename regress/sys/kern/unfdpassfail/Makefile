# $OpenBSD: Makefile,v 1.1 2021/05/27 20:23:53 mvs Exp $

#
# This test enforces unp_internalize() to follow error path because
# we pass kqueue(2) descriptor. We have concurrent thread which closes
# socket descriptor passed to socket and if we are lucky this triggers
# FRELE(9).
#

PROG= unfdpassfail
LDFLAGS= -lpthread

.include <bsd.regress.mk>
