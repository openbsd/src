#	$OpenBSD: Makefile,v 1.1 2019/05/30 18:11:06 deraadt Exp $

# The regression test binary must be run on a wxallowed filesystem
# to verify correct behaviour (it should crash)
# Doing that isn't simple.  Place the binary temporarily in /usr/local
# if it has wxneeded?  Maybe make a vnd with a filesystem, and mount that?

LDFLAGS+=-z wxneeded
PROG=	wx_syscall

.include <bsd.regress.mk>
