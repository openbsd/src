# 	$OpenBSD: files.cats,v 1.7 2007/06/08 22:57:43 jasper Exp $
#	$NetBSD: files.cats,v 1.27 2003/10/21 08:15:40 skrll Exp $
#
# CATS-specific configuration info
#

maxpartitions 16	
maxusers 2 8 64

define todservice {}

#
# ISA and mixed ISA+EISA or ISA+PCI drivers
#
include "dev/isa/files.isa"
include "dev/isa/files.isapnp"

# Include arm32 footbridge
include "arch/arm/conf/files.footbridge"

#
# Machine-independent ATA drivers
#
include "dev/ata/files.ata"
major	{wd = 16}

#
# time of day clock
#
device	todclock
attach	todclock at todservice
file	arch/arm/footbridge/todclock.c			todclock needs-count

# ISA DMA glue
file	arch/arm/footbridge/isa/isadma_machdep.c	isadma

# Game adapter (joystick)
file	arch/arm/footbridge/isa/joy_timer.c		joy

major	{rd = 18}

# RAIDframe
major	{raid = 71}

#
# Machine-independent SCSI/ATAPI drivers
#

include "../../../scsi/files.scsi"
include "../../../dev/atapiscsi/files.atapiscsi"
major	{sd = 24}
major	{cd = 26}

file	arch/arm/arm/conf.c

# Generic MD files
file	arch/cats/cats/autoconf.c
file	arch/cats/cats/cats_machdep.c

# library functions

file	arch/arm/arm/disksubr.c				disk

# ISA Plug 'n Play autoconfiguration glue.
file	arch/arm/footbridge/isa/isapnp_machdep.c	isapnp

# ISA support.
file	arch/arm/footbridge/isa/isa_io.c		isa
file	arch/arm/footbridge/isa/isa_io_asm.S		isa

# CATS boards have an EBSA285 based core with an ISA bus
file	arch/arm/footbridge/isa/isa_machdep.c		isa

device ds1687rtc: todservice
attach ds1687rtc at isa
file	arch/arm/footbridge/isa/dsrtc.c			ds1687rtc

# Machine-independent I2O drivers.
include "dev/i2o/files.i2o"

# PCI devices

#
# Include PCI config
#
include "dev/mii/files.mii"
include "dev/pci/files.pci"

device	pcib: isabus
attach	pcib at pci
file	arch/cats/pci/pcib.c				pcib

file	arch/cats/pci/pciide_machdep.c			pciide

# Include USB stuff
include "dev/usb/files.usb"

# Bluetooth
include "dev/bluetooth/files.bluetooth"

# Include WSCONS stuff
include "dev/wscons/files.wscons"
include "dev/rasops/files.rasops"
include "dev/wsfont/files.wsfont"
include "dev/pckbc/files.pckbc"

#
# Machine-independent 1-Wire drivers
#
include "dev/onewire/files.onewire"
