#	$OpenBSD: Makefile,v 1.18 2001/06/26 22:26:15 smurph Exp $

.if	make(obj)
SUBDIR= special alpha amiga crunch hp300 i386 mac68k mvme68k mvme88k\
	mvmeppc powerpc sparc sun3 vax
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
