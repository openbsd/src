#	$OpenBSD: Makefile,v 1.266 2004/06/03 18:46:24 grange Exp $

MAN=	aac.4 ac97.4 acphy.4 addcom.4 adv.4 aha.4 ahb.4 ahc.4 \
	aic.4 amdpm.4 ami.4 amphy.4 an.4 aria.4 ast.4 atalk.4 \
	atapiscsi.4 audio.4 aue.4 auich.4 autri.4 auvia.4 awi.4 \
	axe.4 az.4 bce.4 bge.4 bha.4 bio.4 bktr.4 bmtphy.4 boca.4 bpf.4 \
	brgphy.4 bridge.4 cac.4 cardbus.4 carp.4 ccd.4 cd.4 \
	ch.4 clcs.4 clct.4 clnp.4 cltp.4 cmpci.4 cnw.4 com.4 \
	crypto.4 cue.4 cy.4 cz.4 dc.4 dcphy.4 ddb.4 de.4 dpt.4 \
	drum.4 eap.4 ec.4 eephy.4 ef.4 eg.4 ehci.4 eisa.4 el.4 em.4 \
	emu.4 enc.4 ep.4 esa.4 esis.4 eso.4 ess.4 exphy.4 \
	faith.4 fd.4 fdc.4 fms.4 fpa.4 fxp.4 gdt.4 gem.4 gif.4 \
	gpio.4 gpr.4 gre.4 gscsio.4 gtp.4 \
	hifn.4 hil.4 hilid.4 hilkbd.4 hilms.4 \
	hme.4 hotplug.4 hsq.4 icmp.4 icmp6.4 icsphy.4 idp.4 ifmedia.4 \
	iha.4 iic.4 inet.4 inet6.4 inphy.4 iop.4 iophy.4 ioprbs.4 \
	iopsp.4 ip.4 ip6.4 ipcomp.4 ipsec.4 isa.4 isapnp.4 \
	ises.4 iso.4 isp.4 it.4 ksyms.4 kue.4 lc.4 lge.4 lkm.4 \
	lm.4 lmc.4 lmtemp.4 lo.4 lofn.4 lpt.4 lxtphy.4 maestro.4 midi.4 \
	mii.4 mpt.4 mpu.4 mtd.4 mtdphy.4 mtio.4 ncr.4 ne.4 neo.4 \
	netintro.4 nge.4 noct.4 nofn.4 ns.4 nsclpcsio.4 nsgphy.4 \
	nsip.4 nsphy.4 nsphyter.4 null.4 ohci.4 opl.4 options.4 \
	oosiop.4 osiop.4 pcdisplay.4 pchb.4 pci.4 pcib.4 pciide.4 pckbc.4 \
	pckbd.4 pcmcia.4 pcppi.4 pcscp.4 pf.4 pflog.4 pfsync.4 \
	pms.4 ppb.4 ppp.4 pty.4 puc.4 qsphy.4 radio.4 raid.4 \
	random.4 ray.4 rd.4 rl.4 rln.4 rlphy.4 route.4 rt.4 \
	rtfps.4 rtii.4 safe.4 sbus.4 scsi.4 sd.4 ses.4 sf.4 \
	sf2r.4 sfr.4 siop.4 sis.4 sk.4 sl.4 sm.4 spp.4 sppp.4 \
	sqphy.4 ss.4 st.4 ste.4 stge.4 sti.4 stp.4 sv.4 \
	systrace.4 tb.4 tcic.4 tcp.4 termios.4 ti.4 tl.4 \
	tlphy.4 tp.4 tqphy.4 trm.4 tty.4 tun.4 twe.4 tx.4 txp.4 \
	txphy.4 uaudio.4 ubsa.4 ubsec.4 ucom.4 udp.4 udsbr.4 \
	uftdi.4 ugen.4 uhci.4 uhid.4 uhidev.4 uk.4 ukbd.4 \
	ukphy.4 ulpt.4 umass.4 umct.4 umidi.4 umodem.4 ums.4 \
	unix.4 upl.4 uplcom.4 urio.4 url.4 urlphy.4 usb.4 \
	uscanner.4 usscanner.4 uvisor.4 uvscom.4 uyap.4 vga.4 \
	viaenv.4 vlan.4 vnd.4 vr.4 wb.4 wd.4 wdc.4 we.4 wi.4tbl \
	wscons.4 wsdisplay.4 wskbd.4 wsmouse.4 wsmux.4 xe.4 \
	xf86.4 xl.4 xmphy.4 yds.4 ym.4 zero.4


MLINKS+=com.4 pccom.4
MLINKS+=adv.4 advansys.4
MLINKS+=adv.4 adw.4
MLINKS+=cardbus.4 cbb.4
MLINKS+=fd.4 stderr.4 fd.4 stdin.4 fd.4 stdout.4
MLINKS+=fpa.4 fea.4
MLINKS+=isapnp.4 pnp.4
MLINKS+=netintro.4 networking.4
MLINKS+=pcmcia.4 pcic.4
MLINKS+=pms.4 pmsi.4
MLINKS+=pty.4 ptm.4
MLINKS+=random.4 arandom.4
MLINKS+=random.4 srandom.4 random.4 urandom.4 random.4 prandom.4
MLINKS+=scsi.4 scsibus.4
MLINKS+=sk.4 skc.4
MLINKS+=tty.4 cua.4
MLINKS+=usb.4 uhub.4
MLINKS+=vnd.4 svnd.4

SUBDIR=	man4.alpha man4.amd64 man4.cats man4.hp300 man4.hppa man4.i386 \
	man4.luna88k man4.mac68k man4.macppc man4.mvme68k man4.mvme88k \
	man4.sparc man4.sparc64 man4.vax

.include <bsd.prog.mk>
