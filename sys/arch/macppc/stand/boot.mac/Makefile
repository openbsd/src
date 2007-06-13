#	$OpenBSD: Makefile,v 1.3 2007/06/13 02:17:32 drahn Exp $
#       $NetBSD: Makefile,v 1.1 1996/09/30 16:35:05 ws Exp $

S=      ${.CURDIR}/../../../..
R=              ..
.PATH:          $(.CURDIR)/$(R)
RELOC=		E00000				
ENTRY=		_entry
PROG=           boot.mac
SRCS=           Locore.c main.c ofdev.c net.c netif_of.c alloc.c cache.c hfs.c
SRCS+=          boot.c conf.c cmd.c vars.c ctime.c strtol.c

NOMAN=
INSTALL_STRIP=
BINDIR=         /usr/mdec
OBJCOPY?=	objcopy
OBJCOPY_ARGS=	-O aixcoff-rs6000 -R .comment -R .note
LDFLAGS=	-X -Ttext ${RELOC} -e $(ENTRY) -T ${.CURDIR}/elf32_powerpc_merge.x -Bstatic

.PATH:          ${S}/stand/boot
.PATH:          ${S}/lib/libsa

CPPFLAGS+=	-I. -I${.CURDIR}/../../.. -I${.CURDIR}/../../../..
CPPFLAGS+=	-DRELOC=0x${RELOC} -DCONSPEED=57600
CPPFLAGS+=	-DXCOFF_GLUE		# for booting PCI Powermacs

LIBS!=          cd $(.CURDIR)/$(R); $(MAKE) libdep
CLEANFILES=	fixcoff

$(PROG):        $(OBJS) $(LIBS) fixcoff 
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o ${.TARGET}
	${OBJCOPY} ${OBJCOPY_ARGS} ${.TARGET}
	${.OBJDIR}/fixcoff ${.TARGET}

fixcoff:	fixcoff.c
	${HOSTCC} -I${.CURDIR} -o fixcoff ${.CURDIR}/fixcoff.c

.include <bsd.prog.mk>
