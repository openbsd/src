#	$OpenBSD: Makefile,v 1.6 2000/04/27 12:36:29 rahnds Exp $
#       $NetBSD: Makefile,v 1.1 1996/09/30 16:35:05 ws Exp $

R=              ..
.PATH:          $(.CURDIR)/$(R)
RELOC=		6c0000
ENTRY=		_entry
PROG=           boot.mac
SRCS=           Locore.c boot.c ofdev.c net.c netif_of.c alloc.c
CFLAGS+=        -DDEBUG -DNETIF_DEBUG
CFLAGS+=	-DXCOFF_GLUE
NOMAN=
INSTALL_STRIP=
BINDIR=         /usr/mdec
OBJCOPY?=	objcopy
OBJCOPY_ARGS=	-O aixcoff-rs6000 -R .stab -R .stabstr -R .comment
LDFLAGS=	-X -Ttext ${RELOC} -e $(ENTRY) -T ${.CURDIR}/ld.script
CPPFLAGS+=	-DPOWERPC_BOOT_ELF


LIBS!=          cd $(.CURDIR)/$(R); $(MAKE) libdep
CLEANFILES=	hack-coff

$(PROG):        $(OBJS) $(LIBS) hack-coff
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o ${.TARGET}
	${OBJCOPY} ${OBJCOPY_ARGS} ${.TARGET}
	${.OBJDIR}/hack-coff ${.TARGET}

hack-coff:	hack-coff.c
	${HOSTCC} -I${.CURDIR} -o hack-coff ${.CURDIR}/hack-coff.c

.include <bsd.prog.mk>
