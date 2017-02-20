#	$OpenBSD: Makefile,v 1.58 2017/02/20 01:00:26 jsg Exp $
#	$NetBSD: Makefile,v 1.35 1996/04/03 21:20:52 chuck Exp $

.include <bsd.own.mk>

.if make(obj)
SUBDIR+=	cc clang gcc
.else
.  if ${COMPILER_VERSION:L} == "gcc3"
SUBDIR+=	gcc
.  elif ${COMPILER_VERSION:L} == "gcc4"
SUBDIR+=	cc
.  elif ${COMPILER_VERSION:L} == "clang"
SUBDIR+=	clang
.  endif
.endif

SUBDIR+=	cxxfilt cvs
# binutils-2.17 needs to build gdb in binutils at the moment
.if make(obj)
SUBDIR+=	binutils
.elif ${MACHINE_CPU} != "aarch64"
SUBDIR+=	binutils
.endif
SUBDIR+=	binutils-2.17

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
