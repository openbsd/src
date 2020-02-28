#	$OpenBSD: Makefile,v 1.77 2020/02/28 04:59:06 deraadt Exp $
#	$NetBSD: Makefile,v 1.20.4.1 1996/06/14 17:22:38 cgd Exp $

SUBDIR=	csu libarch libc libcbor libcompiler_rt libcrypto libcurses \
	libcxxabi libcxx libedit libelf libevent libexpat \
	libform libfuse libkeynote libkvm libl libm libmenu \
	libossaudio libpanel libpcap libradius librthread \
	librpcsvc libskey libsndio libssl libtls libusbhid \
	libutil liby libz

# Build these last because of library dependencies.
SUBDIR+=libfido2

.include <bsd.subdir.mk>
