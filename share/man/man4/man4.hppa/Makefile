#	$OpenBSD: Makefile,v 1.28 2011/06/23 22:46:12 schwarze Exp $

MAN=	asp.4 astro.4 cpu.4 dino.4 elroy.4
MAN+=	gecko.4 gsc.4 gsckbc.4
MAN+=	harmony.4 ie.4 intro.4 io.4 lasi.4 lcd.4 mem.4 mongoose.4 pdc.4
MAN+=	phantomas.4 power.4 runway.4 ssio.4 uturn.4 wax.4
# tir.4 xbar.4 mcx.4
MLINKS+= mem.4 kmem.4
MANSUBDIR=hppa
CLEANFILES+= cpu.4

cpu.4: cpu.4tbl
	cp ${.ALLSRC} ${.TARGET}

.include <bsd.prog.mk>
