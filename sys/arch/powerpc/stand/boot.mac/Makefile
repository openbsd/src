#	$OpenBSD: Makefile,v 1.2 1997/04/01 21:01:48 rahnds Exp $
#       $NetBSD: Makefile,v 1.1 1996/09/30 16:35:05 ws Exp $

R=              ..
.PATH:          $(.CURDIR)/$(R)
RELOC=		4000
ENTRY=		__start
LOADADDR=	4000
PROG=           boot.mac
SRCS=           Locore.c boot.c ofdev.c net.c netif_of.c alloc.c
#CFLAGS+=       -DDEBUG -DNETIF_DEBUG
CFLAGS+=	-DEXEC_ELF
NOMAN=
STRIP=
MAKEELF=        makeelf
BINDIR=         /usr/mdec
OBJCOPY?=	objcopy
OBJCOPY_ARGS=	-O aixcoff-rs6000 -R .stab -R .stabstr -R .comment
LDFLAGS=	-X -Ttext $(RELOC) -e $(ENTRY) -T ${.CURDIR}/ld.script

LIBS!=          cd $(.CURDIR)/$(R); $(MAKE) libdep

$(PROG):        $(OBJS) $(LIBS) hack-coff
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o ${.TARGET}
	${OBJCOPY} ${OBJCOPY_ARGS} ${.TARGET}
	${.OBJDIR}/hack-coff ${.TARGET}
#	${MAKEELF} $(REAL_VIRT) a.out $(.TARGET)

hack-coff:	hack-coff.c
	${HOSTCC} -o hack-coff ${.CURDIR}/hack-coff.c

.include <bsd.prog.mk>
