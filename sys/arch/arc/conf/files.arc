#	$OpenBSD: files.arc,v 1.1.1.1 1996/06/24 09:07:20 pefo Exp $
#
# maxpartitions must be first item in files.${ARCH}
#
maxpartitions 8

maxusers 2 8 64

#	Required files


file	arch/arc/arc/autoconf.c
file	arch/arc/arc/conf.c
file	arch/arc/arc/cpu_exec.c
file	arch/arc/arc/disksubr.c
file	arch/arc/dev/dma.c
file	arch/arc/arc/machdep.c
file	arch/arc/arc/minidebug.c
file	arch/arc/arc/mem.c
file	arch/arc/arc/pmap.c
file	arch/arc/arc/process_machdep.c
file	arch/arc/arc/sys_machdep.c
file	arch/arc/arc/trap.c
file	arch/arc/arc/vm_machdep.c

#
#	Machine-independent ATAPI drivers 
#

include "../../../dev/atapi/files.atapi"


#
#	System BUS types
#

define	mainbus {}
device	mainbus
attach	mainbus at root
file	arch/arc/arc/mainbus.c	mainbus

#	Our CPU configurator
device	cpu
attach	cpu at mainbus			# not optional
file arch/arc/arc/cpu.c			cpu

#
#	PICA bus autoconfiguration devices
#
device	pica {}
attach	pica at mainbus			# { slot = -1, offset = -1 }
file	arch/arc/pica/picabus.c		pica

#	Real time clock, must have one..
device	clock
attach	clock at pica
file	arch/arc/arc/clock.c		clock
file	arch/arc/arc/clock_mc.c		clock

#	Ethernet chip
device	sn
attach	sn at pica: ifnet, ether
file	arch/arc/dev/if_sn.c		sn	needs-count

#	Use machine independent SCSI driver routines
include	"../../../scsi/files.scsi"
major	{sd = 0}
major	{cd = 3}

#	Machine dependent SCSI interface driver
device	asc: scsi
attach	asc at pica
file	arch/arc/dev/asc.c		asc	needs-count

#	Console driver on PC-style graphics
device	pc: tty
attach	pc at pica
device	pms: tty
attach	pms at pica
file	arch/arc/dev/pccons.c		pc	needs-count

#	Floppy disk controller
device	fdc {drive = -1}
attach	fdc at pica
device	fd: disk
attach	fd at fdc
file	arch/arc/dev/fd.c		fdc	needs-flag
major	{fd = 7}


#
#	ISA
#
device	isabr {} : isabus
attach	isabr at mainbus
file	arch/arc/isa/isabus.c		isabr
file    arch/arc/isa/isadma.c		isadma needs-flag

#
#	Stock ISA bus support
#
define  pcmcia {}			# XXX dummy decl...
define  pci {}				# XXX dummy decl...

include	"../../../dev/isa/files.isa"

#	Serial driver for both ISA and LOCAL bus.
device  ace: tty
attach  ace at isa with ace_isa
attach  ace at commulti with ace_commulti
attach  ace at pica with ace_pica
file    arch/arc/dev/ace.c		ace & (ace_isa | ace_commulti | ace_pica) needs-flag 

#

file	dev/cons.c
file	dev/cninit.c
file	netinet/in_cksum.c
file	netns/ns_cksum.c			ns

file	compat/ultrix/ultrix_misc.c		compat_ultrix
file	compat/ultrix/ultrix_syscalls.c		compat_ultrix
file	compat/ultrix/ultrix_sysent.c		compat_ultrix

