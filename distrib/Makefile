#	$OpenBSD: Makefile,v 1.19 2001/09/01 17:00:40 drahn Exp $

.if	make(obj)
SUBDIR= special alpha amiga crunch hp300 i386 mac68k macppc mvme68k \
	mvme88k mvmeppc sparc sun3 vax
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
