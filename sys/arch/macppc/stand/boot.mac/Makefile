#	$OpenBSD: Makefile,v 1.1 2001/09/01 15:36:37 drahn Exp $
#       $NetBSD: Makefile,v 1.1 1996/09/30 16:35:05 ws Exp $

R=              ..
.PATH:          $(.CURDIR)/$(R)
RELOC=		6c0000
ENTRY=		_entry
PROG=           boot.mac
SRCS=           Locore.c boot.c ofdev.c net.c netif_of.c alloc.c cache.c hfs.c
CFLAGS+=        -DDEBUG -DNETIF_DEBUG
NOMAN=
INSTALL_STRIP=
BINDIR=         /usr/mdec
OBJCOPY?=	objcopy
OBJCOPY_ARGS=	-O aixcoff-rs6000 -R .stab -R .stabstr -R .comment
LDFLAGS=	-X -Ttext ${RELOC} -e $(ENTRY) -T ${.CURDIR}/ld.script

CPPFLAGS+=	-I. -I${.CURDIR}/../../.. -I${.CURDIR}/../../../..
CPPFLAGS+=	-DRELOC=0x${RELOC}
CPPFLAGS+=	-DFIRMWORKSBUGS
CPPFLAGS+=	-DPOWERPC_BOOT_ELF
CPPFLAGS+=	-DXCOFF_GLUE		# for booting PCI Powermacs


LIBS!=          cd $(.CURDIR)/$(R); $(MAKE) libdep
CLEANFILES=	hack-coff

$(PROG):        $(OBJS) $(LIBS) hack-coff
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o ${.TARGET}
	${OBJCOPY} ${OBJCOPY_ARGS} ${.TARGET}
	${.OBJDIR}/hack-coff ${.TARGET}

hack-coff:	hack-coff.c
	${HOSTCC} -I${.CURDIR} -o hack-coff ${.CURDIR}/hack-coff.c

.include <bsd.prog.mk>
