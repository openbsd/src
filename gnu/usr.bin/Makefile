#	$OpenBSD: Makefile,v 1.55 2014/07/16 01:23:39 deraadt Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

.include <bsd.own.mk>

.if make(obj)
SUBDIR+=	cc gcc
.else
.  if ${COMPILER_VERSION:L} == "gcc3"
SUBDIR+=	gcc
.  else
SUBDIR+=	cc
.  endif
.endif

SUBDIR+=	cxxfilt cvs
.if make(obj)
SUBDIR+=	binutils binutils-2.17
.else
# binutils-2.17 needs to build gdb in binutils at the moment
.if ${BINUTILS_VERSION} == "binutils-2.17"
SUBDIR+=	binutils
.endif
SUBDIR+=	${BINUTILS_VERSION}
.endif

# Do these last; texinfo builds the info 'dir' file, perl must be after binutils
SUBDIR+=	perl texinfo

.if ${COMPILER_VERSION:L} == "gcc4"
maninstall:
	${INSTALL} ${INSTALL_COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		gcc/protoize.1 ${DESTDIR}${MANDIR}1/protoize.1
	@rm -f ${DESTDIR}${MANDIR}1/unprotoize.1
	ln ${DESTDIR}${MANDIR}1/protoize.1 ${DESTDIR}${MANDIR}1/unprotoize.1
.endif

.include <bsd.subdir.mk>
