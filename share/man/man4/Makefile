#	$OpenBSD: Makefile,v 1.424 2007/09/04 20:19:24 deraadt Exp $

MAN=	aac.4 ac97.4 acphy.4 \
	acpi.4 acpiac.4 acpibat.4 acpibtn.4 acpicpu.4 acpidock.4 \
	acpiec.4 acpihpet.4 acpimadt.4 acpiprt.4 acpitimer.4 acpitz.4 \
	acx.4 adc.4 addcom.4 admcts.4 admlc.4 admtemp.4 \
	admtm.4 admtmp.4 admtt.4 adt.4 adv.4 aha.4 ahb.4 ahc.4 ahci.4 ahd.4 \
	aic.4 akbd.4 alipm.4 amdiic.4 amdpm.4 ami.4 amphy.4 ams.4 \
	an.4 aps.4 arc.4 aria.4 art.4 \
	asbtm.4 ast.4 atalk.4 atapiscsi.4 ath.4 atu.4 atw.4 audio.4 aue.4 \
	auich.4 autri.4 auixp.4 auvia.4 awi.4 axe.4 az.4 azalia.4 \
	bce.4 bge.4 bha.4 \
	bio.4 bktr.4 bluetooth.4 bmtphy.4 bnx.4 boca.4 bpf.4 brgphy.4 bridge.4 \
	cac.4 cas.4 cardbus.4 carp.4 ccd.4 cd.4 cdce.4 cdcef.4 che.4 cfxga.4 \
	ch.4 ciphy.4 ciss.4 clcs.4 clct.4 cmpci.4 cnw.4 \
	com.4 crypto.4 cue.4 cy.4 cz.4 dc.4 dcphy.4 ddb.4 de.4 dpt.4 \
	drum.4 eap.4 ec.4 eephy.4 ef.4 eg.4 ehci.4 eisa.4 el.4 em.4 \
	emu.4 enc.4 ep.4 epic.4 esa.4 eso.4 ess.4 exphy.4 \
	faith.4 fd.4 fdc.4 fintek.4 fms.4 fpa.4 fxp.4 gdt.4 \
	gentbi.4 gem.4 gif.4 \
	glenv.4 gpio.4 gpioiic.4 gpioow.4 gpr.4 gre.4 gscsio.4 gtp.4 \
	hifn.4 hil.4 hilid.4 hilkbd.4 hilms.4 hme.4 hotplug.4 hsq.4 \
	ichiic.4 ichwdt.4 icmp.4 icmp6.4 icsphy.4 ifmedia.4 \
	iha.4 iic.4 inet.4 inet6.4 inphy.4 iop.4 iophy.4 ioprbs.4 \
	iopsp.4 ip.4 ip6.4 ipcomp.4 ipgphy.4 ipmi.4 ips.4 ipsec.4 ipw.4 \
	isa.4 isagpio.4 isapnp.4 ises.4 isp.4 it.4 iwi.4 ixgb.4 \
	jmb.4 \
	ksyms.4 kue.4 lc.4 lge.4 lkm.4 lm.4 \
	lmc.4 lmenv.4 lmn.4 lmtemp.4 lo.4 lofn.4 lpt.4 lxtphy.4 luphy.4 \
	maestro.4 mainbus.4 malo.4 maxds.4 maxtmp.4 mbg.4 midi.4 mii.4 mfi.4 \
	moscom.4 mpi.4 mpu.4 msk.4 mtd.4 mtdphy.4 multicast.4 mtio.4 myx.4 \
	ne.4 neo.4 netintro.4 nfe.4 nge.4 nmea.4 noct.4 nofn.4 nsclpcsio.4 \
	nsgphy.4 nsphy.4 nsphyter.4 null.4 nviic.4 ohci.4 opl.4 \
	options.4 onewire.4 oosiop.4 osiop.4 owid.4 owsbm.4 owtemp.4 \
	pcagpio.4 pcaled.4 pcdisplay.4 pchb.4 pci.4 pcib.4 pcfadc.4 \
	pcfiic.4 pciide.4 pckbc.4 pckbd.4 pcmcia.4 pcn.4 pcppi.4 pcscp.4 \
	pf.4 pflog.4 pfsync.4 pgt.4 piixpm.4 pim.4 \
	pms.4 ppb.4 ppp.4 pppoe.4 pty.4 puc.4 pwdog.4 qsphy.4 radio.4 raid.4 \
	ral.4 random.4 ray.4 rd.4 re.4 rgephy.4 ricohrtc.4 \
	rl.4 rln.4 rlphy.4 route.4 rt.4 rum.4 \
	rtfps.4 rtii.4 rtw.4 safe.4 safte.4 san.4 sbt.4 sbus.4 scsi.4 sd.4 \
	sdmmc.4 sdhc.4 ses.4 \
	sequencer.4 sf.4 sf2r.4 sfr.4 sili.4 siop.4 sis.4 sk.4 sl.4 sli.4 \
	sm.4 softraid.4 speaker.4 sppp.4 sqphy.4 ss.4 st.4 ste.4 stge.4 sti.4 \
	stp.4 sv.4 systrace.4 tcic.4 tcp.4 termios.4 tht.4 ti.4 tl.4 \
	tlphy.4 tqphy.4 trm.4 trunk.4 tsl.4 tty.4 tun.4 twe.4 txp.4 \
	txphy.4 uaudio.4 uark.4 uath.4 uberry.4 ubsa.4 ubsec.4 ubt.4 \
	ucom.4 ucycom.4 udav.4 udcf.4 udp.4 udsbr.4 \
	ueagle.4 uftdi.4 ugen.4 uhci.4 uhid.4 uhidev.4 uipaq.4 uk.4 ukbd.4 \
	ukphy.4 ulpt.4 umass.4 umct.4 umidi.4 umodem.4 ums.4 umsm.4 \
	unix.4 uow.4 upl.4 uplcom.4 urio.4 url.4 urlphy.4 usb.4 usbf.4 \
	uscanner.4 uslcom.4 usscanner.4 uts.4 uvisor.4 uvscom.4 uyap.4 vga.4 \
	vgafb.4 vge.4 viaenv.4 viapm.4 viasio.4 vic.4 vlan.4 vnd.4 vr.4 \
	watchdog.4 wb.4 wbenv.4 wd.4 wdc.4 we.4 wi.4tbl wpi.4 wscons.4 \
	wsdisplay.4 wskbd.4 wsmouse.4 wsmux.4 xe.4 xf86.4 xge.4 xl.4 \
	xmphy.4 yds.4 ym.4 zero.4 zyd.4

MLINKS+=adv.4 advansys.4 adv.4 adw.4
MLINKS+=art.4 musycc.4
MLINKS+=audio.4 mixer.4
MLINKS+=cardbus.4 cbb.4
MLINKS+=com.4 pccom.4
MLINKS+=fd.4 stderr.4 fd.4 stdin.4 fd.4 stdout.4
MLINKS+=fpa.4 fea.4 fpa.4 fta.4
MLINKS+=isapnp.4 pnp.4
MLINKS+=netintro.4 networking.4
MLINKS+=pcmcia.4 pcic.4
MLINKS+=pms.4 pmsi.4
MLINKS+=pty.4 ptm.4
MLINKS+=ral.4 ural.4
MLINKS+=random.4 arandom.4
MLINKS+=random.4 srandom.4 random.4 urandom.4 random.4 prandom.4
MLINKS+=scsi.4 scsibus.4
MLINKS+=sequencer.4 music.4
MLINKS+=sk.4 skc.4 msk.4 mskc.4
MLINKS+=speaker.4 spkr.4
MLINKS+=tht.4 thtc.4
MLINKS+=tty.4 cua.4
MLINKS+=usb.4 uhub.4
MLINKS+=vnd.4 svnd.4

SUBDIR=	man4.alpha man4.amd64 man4.aviion man4.hp300 man4.hppa \
	man4.i386 man4.landisk man4.luna88k man4.mac68k man4.macppc \
	man4.mvme68k man4.mvme88k man4.sgi man4.sparc man4.sparc64 man4.vax \
	man4.zaurus

.include <bsd.prog.mk>
