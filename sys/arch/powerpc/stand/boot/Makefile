#	$OpenBSD: Makefile,v 1.6 1997/05/05 16:49:52 rahnds Exp $
#       $NetBSD: Makefile,v 1.1 1996/09/30 16:35:05 ws Exp $

R=              ..
.PATH:          $(.CURDIR)/$(R)
PROG=           boot.ppc
SRCS=           Locore.c boot.c ofdev.c net.c netif_of.c alloc.c
#CFLAGS+=       -DDEBUG -DNETIF_DEBUG
CFLAGS+=       -DEXEC_ELF
NOMAN=
INSTALL_STRIP=
MAKEELF=        makeelf
BINDIR=         /usr/mdec
#OBJS+=		note.o
LDFLAGS=	-X -N -Ttext $(RELOC) -e $(ENTRY)

LIBS!=          cd $(.CURDIR)/$(R); $(MAKE) libdep

$(PROG):        $(OBJS) $(LIBS)
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o ${.TARGET}
#	${MAKEELF} $(REAL_VIRT) a.out $(.TARGET)

.include <bsd.prog.mk>
