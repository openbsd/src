#	$OpenBSD: files.arc,v 1.18 1998/03/16 09:38:38 pefo Exp $
#
# maxpartitions must be first item in files.${ARCH}
#
maxpartitions 16

maxusers 2 8 64

#	Required files

file	arch/arc/arc/autoconf.c
file	arch/arc/arc/conf.c
file	arch/arc/dev/dma.c
file	arch/arc/arc/machdep.c
file	arch/arc/arc/pmap.c
file	arch/arc/arc/trap.c

file	arch/mips/mips/arcbios.c

#
#	Machine-independent ATAPI drivers 
#
include "../../../dev/atapi/files.atapi"
major	{ acd = 5 }

#
#	System BUS types
#
define	mainbus {}
device	mainbus
attach	mainbus at root
file	arch/mips/mips/mainbus.c	mainbus

#	Our CPU configurator
device	cpu
attach	cpu at mainbus			# not optional
file arch/mips/mips/cpu.c		cpu

#
#	PICA bus autoconfiguration devices
#
device	pica {}
attach	pica at mainbus			# optional
file	arch/arc/pica/picabus.c		pica

#
#	ALGOR bus autoconfiguration devices
#
device	algor {}
attach	algor at mainbus		# optional
file	arch/arc/algor/algorbus.c	algor

#
#	ISA Bus bridge
#
device	isabr {} : isabus
attach	isabr at mainbus		# optional
file	arch/arc/isa/isabus.c		isabr

#
#	PCI Bus bridge
#
device	pbcpcibr {} : pcibus
attach	pbcpcibr at mainbus		# optional
file	arch/arc/pci/pbcpcibus.c	pbcpcibr

#	Ethernet chip on PICA bus
device	sn
attach	sn at pica: ifnet, ether
file	arch/arc/dev/if_sn.c		sn	needs-count

#	Use machine independent SCSI driver routines
include	"../../../scsi/files.scsi"
major	{sd = 0}
major	{cd = 3}

#	Symbios 53C94 SCSI interface driver on PICA bus
device	asc: scsi
attach	asc at pica
file	arch/arc/dev/asc.c		asc	needs-count

#	Floppy disk controller on PICA bus
device	fdc {drive = -1}
attach	fdc at pica
device	fd: disk
attach	fd at fdc
file	arch/arc/dev/fd.c		fdc	needs-flag
major	{fd = 7}

#
#	Stock ISA bus support
#
define  pcmcia {}			# XXX dummy decl...

include	"../../../dev/pci/files.pci"
include	"../../../dev/isa/files.isa"
major	{ wd = 4 }

#	Real time clock, must have one..
device	clock
attach	clock at pica with clock_pica
attach	clock at isa with clock_isa
attach	clock at algor with clock_algor
file	arch/arc/arc/clock_mc.c	clock & (clock_isa | clock_pica | clock_algor) needs-flag

#	Console driver on PC-style graphics
device	pc: tty
attach	pc at pica with pc_pica
attach	pc at isa with pc_isa
device	pms: tty
attach	pms at pica
file	arch/arc/dev/pccons.c		pc & (pc_pica | pc_isa)	needs-flag

# BusLogic BT-445C VLB SCSI Controller. Special on TYNE local bus.
device  btl: scsi
attach  btl at isa
file    arch/arc/dti/btl.c              btl needs-count

# 8250/16[45]50-based "com" ports
attach	com at pica with com_pica
attach	com at algor with com_algor
file	arch/arc/dev/com_lbus.c		com & (com_pica | com_algor)


# National Semiconductor DS8390/WD83C690-based boards
# (WD/SMC 80x3 family, SMC Ultra [8216], 3Com 3C503, NE[12]000, and clones)
# XXX conflicts with other ports; can't be in files.isa
device  ed: ether, ifnet
attach  ed at isa with ed_isa
attach  ed at pcmcia with ed_pcmcia
file    dev/isa/if_ed.c                 ed & (ed_isa | ed_pcmcia) needs-flag

# PC parallel ports (XXX what chip?)
attach	lpt at pica with lpt_pica
attach	lpt at algor with lpt_algor
file	arch/arc/dev/lpt_lbus.c		lpt & (lpt_pica | lpt_algor)


#
#	PCI Bus support
#

# PCI VGA display driver
device	pcivga: tty
attach	pcivga at pci
file	arch/arc/pci/pci_vga.c		pcivga

#
# ISA PnP
#

include "../../../dev/isa/files.isapnp"
file	arch/arc/isa/isapnp_machdep.c	isapnp

#
# Specials.
#
# RAM disk for boot tape
pseudo-device rd
file dev/ramdisk.c			rd needs-flag
file arch/arc/dev/rd_root.c		ramdisk_hooks
major {rd = 8}

#
#	Common files
#

file	dev/cons.c
file	dev/cninit.c
file	netinet/in_cksum.c
file	netns/ns_cksum.c			ns

file	compat/ultrix/ultrix_misc.c		compat_ultrix
file	compat/ultrix/ultrix_syscalls.c		compat_ultrix
file	compat/ultrix/ultrix_sysent.c		compat_ultrix

