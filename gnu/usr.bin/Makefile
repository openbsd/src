#	$NetBSD: Makefile,v 1.27 1995/12/07 07:26:32 phil Exp $

SUBDIR+=	bc cpio dc diff diff3 gawk grep groff
SUBDIR+=	gzip rcs sdiff send-pr sort tar

# some ports don't use the tool chain in /usr/src
.if (${MACHINE_ARCH} != "alpha") && (${MACHINE_ARCH} != "mips") && \
	(${MACHINE_ARCH} != "vax") && (${MACHINE_ARCH} != "ns32k")
SUBDIR+=	gas gcc2 gdb ld
.endif

# ns32k and vax wants newer gcc
.if (${MACHINE_ARCH} == "ns32k") || (${MACHINE_ARCH} == "vax")
SUBDIR+=        gas gcc gdb ld
.endif

# XXX alpha still needs the /usr/bin/cpp script.
.if (${MACHINE_ARCH} == "alpha")
afterinstall:
	install -c -o $(BINOWN) -g $(BINGRP) -m $(BINMODE) \
	    $(.CURDIR)/gcc2/cpp/usr.bin.cpp.sh $(DESTDIR)/usr/bin/cpp
.endif

.include <bsd.subdir.mk>
