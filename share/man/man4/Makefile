#	$OpenBSD: Makefile,v 1.38 1998/07/13 02:39:45 deraadt Exp $
#	$NetBSD: Makefile,v 1.22.4.2 1996/07/18 00:51:10 jtc Exp $

MAN=	atalk.4 audio.4 acd.4 ahc.4 bpf.4 ccd.4 cd.4 ch.4 clnp.4 cltp.4 ddb.4 \
	de.4 drum.4 eap.4 esis.4 fd.4 ffs_softupdates.4 fpa.4 fxp.4 icmp.4 \
	idp.4 imp.4 inet.4 ip.4 ipl.4 ipsec.4 isapnp.4 iso.4 isp.4 lkm.4 lo.4 \
	mtio.4 ncr.4 netintro.4 ns.4 \
	nsip.4 null.4 options.4 pty.4 random.4 route.4 scsi.4 sd.4 sl.4 \
	spp.4 st.4 \
	tb.4 tcp.4 termios.4 tty.4 tp.4 tun.4 udp.4 uk.4 unix.4 vnd.4 wd.4
MLINKS+=fd.4 stderr.4 fd.4 stdin.4 fd.4 stdout.4
MLINKS+=netintro.4 networking.4
MLINKS+=random.4 srandom.4 random.4 urandom.4 random.4 prandom.4 
MLINKS+=random.4 arandom.4
MLINKS+=scsi.4 scsibus.4
MLINKS+=tty.4 cua.4
MLINKS+=vnd.4 svnd.4
MLINKS+=isapnp.4 pnp.4
SUBDIR=	man4.alpha man4.amiga man4.arc man4.arm32 man4.atari man4.hp300 \
	man4.i386 man4.mac68k man4.pc532 man4.pmax man4.sparc man4.sun3 \
	man4.vax

.include <bsd.prog.mk>
