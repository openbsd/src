#	$OpenBSD: Makefile,v 1.21 2002/12/31 16:22:22 miod Exp $

.if	make(obj)
SUBDIR= special alpha crunch hp300 i386 mac68k macppc mvme68k \
	mvme88k mvmeppc sparc sparc64 vax
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
