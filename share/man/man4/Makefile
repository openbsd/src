#	$OpenBSD: Makefile,v 1.100 2000/02/22 23:35:27 mjacob Exp $
#	$NetBSD: Makefile,v 1.22.4.2 1996/07/18 00:51:10 jtc Exp $

MAN=	ac97.4 atalk.4 atapiscsi.4 audio.4 adv.4 ahc.4 al.4 ax.4 bpf.4 \
	bridge.4 ccd.4 cd.4 ch.4 clnp.4 cltp.4 cnw.4 dc.4 ddb.4 de.4 dpt.4 \
	drum.4 eap.4 ec.4 ef.4 enc.4 ep.4 esis.4 eso.4 ess.4 exphy.4 fd.4 \
	ffs_softupdates.4 fpa.4 fxp.4 gdt.4 gre.4 icmp.4 icsphy.4 idp.4 \
	ifmedia.4 inet.4 inphy.4 iophy.4 ip.4 ipl.4 ipsec.4 \
	isapnp.4 iso.4 isp.4 ksyms.4 lkm.4 lmc.4 lo.4 lxtphy.4 midi.4 mii.4 \
	mtdphy.4 mtio.4 ncr.4 ne.4 netintro.4 ns.4 nsip.4 nsphy.4 null.4 \
	opl.4 options.4 pciide.4 pcmcia.4 pty.4 qsphy.4 raid.4 random.4 \
	rl.4 rln.4 rlphy.4 route.4 scsi.4 sd.4 ses.4 sf.4 sis.4 sk.4 sl.4 sm.4 \
	spp.4 sppp.4 sqphy.4 ss.4 st.4 ste.4 sv.4 tb.4 tcp.4 termios.4 ti.4 \
	tl.4 tlphy.4 tp.4 tqphy.4 tty.4 tun.4 tx.4 txphy.4 uaudio.4 udp.4 \
	ugen.4 uhid.4 uk.4 ukphy.4 unix.4 usb.4 vnd.4 vr.4 wb.4 wd.4 we.4 \
	wx.4 xl.4 ym.4 zero.4

# IPv6
MAN+=	faith.4 gif.4 icmp6.4 inet6.4 ip6.4

MLINKS+=fd.4 stderr.4 fd.4 stdin.4 fd.4 stdout.4
MLINKS+=netintro.4 networking.4
MLINKS+=random.4 srandom.4 random.4 urandom.4 random.4 prandom.4 
MLINKS+=random.4 arandom.4
MLINKS+=scsi.4 scsibus.4
MLINKS+=tty.4 cua.4
MLINKS+=vnd.4 svnd.4
MLINKS+=isapnp.4 pnp.4
MLINKS+=adv.4 adw.4
MLINKS+=adv.4 advansys.4
MLINKS+=pcmcia.4 pcic.4
SUBDIR=	man4.alpha man4.amiga man4.arc man4.arm32 man4.atari man4.hp300 \
	man4.i386 man4.mac68k man4.mvme68k man4.pc532 man4.pmax man4.powerpc man4.sparc \
	man4.sun3 man4.vax

.include <bsd.prog.mk>
