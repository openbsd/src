#	$NetBSD: Makefile.inc,v 1.1 1996/09/30 16:34:59 ws Exp $

.if !defined(__stand_makefile_inc)
__stand_makefile_inc=1

KERN_AS=	library

S=$(.CURDIR)/../../../$(R)

.if !make(libdep) && !make(sadep) && !make(salibdir) && !make(kernlibdir) && !make(obj)
.BEGIN:
	@([ -h machine ] || ln -s $(S)/arch/$(MACHINE)/include machine)
.endif

# for now
RELOC?=		4000
LOADADDR?=	100000
#
EXTRACFLAGS=	-msoft-float
REAL_VIRT?=	-v
ENTRY?=	_start

INCLUDES+=	-I. -I$(.CURDIR)/.. -I$(S)/arch -I$(S) -I$(S)/lib/libsa
DEFS+=		-DSTANDALONE -DRELOC=0x$(RELOC) -DLOADADDR=0x$(LOADADDR) \
		-DFIREPOWERBUGS
CFLAGS+=	$(INCLUDES) $(DEFS) $(EXTRACFLAGS)
LDFLAGS=	-X -N -Ttext $(RELOC) -e $(ENTRY)

cleandir:
	rm -rf lib machine

.endif
