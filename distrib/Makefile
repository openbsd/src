#	$OpenBSD: Makefile,v 1.24 2004/02/07 21:16:45 deraadt Exp $

.if	make(obj)
SUBDIR= special alpha crunch hp300 hppa i386 mac68k macppc mvme68k \
	mvme88k mvmeppc pegasos sparc sparc64 vax amd64 cats
.elif	!make(install)
SUBDIR= special
.endif
.if	make(clean) || make(cleandir)
SUBDIR+=crunch
.endif
.if exists(${MACHINE})
SUBDIR+= ${MACHINE}
.endif
SUBDIR+= notes

#all: crunch-tools _SUBDIRUSE

crunch-tools:
	(cd $(.CURDIR)/crunch; $(MAKE) obj; \
	 $(MAKE) depend && ${MAKE} && ${SUDO} ${MAKE} DESTDIR= install)

.include <bsd.subdir.mk>
