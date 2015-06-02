#	$OpenBSD: install.md,v 1.10 2015/06/02 01:48:25 jsg Exp $
#
#
# Copyright (c) 1996 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Jason R. Thorpe.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
#
# machine dependent section of installation/upgrade script.
#

# This code runs when the script is initally sourced to set up 
# MDSETS, SANESETS and DEFAULTSETS 

dmesg | grep "^omap0 at mainbus0:" >/dev/null
if [[ $? == 0 ]]; then
        MDPLAT=OMAP
	LOADADDR=0x82800000
fi
dmesg | grep "^imx0 at mainbus0:" >/dev/null
if [[ $? == 0 ]]; then
        MDPLAT=IMX
	LOADADDR=0x18800000
fi
dmesg | grep "^sunxi0 at mainbus0:" >/dev/null
if [[ $? == 0 ]]; then
	MDPLAT=SUNXI
	LOADADDR=0x40200000
fi
BEAGLE=$(scan_dmesg '/^omap0 at mainbus0: TI OMAP3 \(BeagleBoard\).*/s//\1/p')
BEAGLEBONE=$(scan_dmesg '/^omap0 at mainbus0: TI AM335x \(BeagleBone\).*/s//\1/p')
PANDA=$(scan_dmesg '/^omap0 at mainbus0: TI OMAP4 \(PandaBoard\)/s//\1/p')
CUBOX=$(scan_dmesg '/^imx0 at mainbus0: \(SolidRun.*\)/s//CUBOX/p')
NITROGEN=$(scan_dmesg '/^imx0 at mainbus0: \(Freescale i.MX6 SABRE Lite.*\)/s//NITROGEN/p')

MDSETS="bsd.${MDPLAT}.umg bsd.rd.${MDPLAT}.umg"
SANESETS="bsd"
DEFAULTSETS="bsd bsd.rd ${MDSETS}"

NEWFSARGS_msdos="-F 16 -L boot"
NEWFSARGS_ext2fs="-v boot"

md_installboot() {
	local _disk=$1
	mount /dev/${_disk}i /mnt/mnt

        if [[ -f /mnt/bsd.${MDPLAT}.umg ]]; then
                mv /mnt/bsd.${MDPLAT}.umg /mnt/mnt/bsd.umg
        fi
        if [[ -f /mnt/bsd.rd.${MDPLAT}.umg ]]; then
                mv /mnt/bsd.rd.${MDPLAT}.umg /mnt/mnt/bsdrd.umg
        fi

	# extracted on all machines, so make snap works.
	tar -C /mnt/ -xf /usr/mdec/u-boots.tgz 

	if [[ ${MDPLAT} == "OMAP" ]]; then

		if [[ -n $BEAGLE ]]; then
			cp /mnt/usr/mdec/beagle/{mlo,u-boot.bin} /mnt/mnt/
		elif [[ -n $BEAGLEBONE ]]; then
			cp /mnt/usr/mdec/am335x/{mlo,u-boot.img} /mnt/mnt/
		elif [[ -n $PANDA ]]; then
			cp /mnt/usr/mdec/panda/{mlo,u-boot.bin} /mnt/mnt/
		fi
		cat > /mnt/mnt/uenv.txt<<__EOT
bootcmd=mmc rescan ; setenv loadaddr ${LOADADDR}; setenv bootargs sd0i:/bsd.umg ; fatload mmc \${mmcdev} \${loadaddr} bsd.umg ; bootm \${loadaddr} ;
uenvcmd=boot
__EOT
	elif [[ ${MDPLAT} == "IMX" ]]; then
		if [[ -n $CUBOX ]]; then
			cat > /tmp/boot.cmd<<__EOT
; setenv loadaddr ${LOADADDR} ; setenv bootargs sd0i:/bsd.umg ; for dtype in usb mmc ; do for disk in 0 1 ; do \${dtype} dev \${disk} ; for fs in fat ext2 ; do if \${fs}load \${dtype} \${disk}:1 \${loadaddr} bsd.umg ; then bootm \${loadaddr} ; fi ; done; done; done; echo; echo failed to load bsd.umg
			mkuboot -t script -a arm -o linux /tmp/boot.cmd /mnt/mnt/boot.scr
			dd if=/mnt/usr/mdec/cubox/SPL of=/dev/${_disk}c bs=1024 seek=1
			dd if=/mnt/usr/mdec/cubox/u-boot.img of=/dev/${_disk}c bs=1024 seek=42
		elif [[ -n $NITROGEN ]]; then
			cat > /tmp/6x_bootscript.scr<<__EOT
	; setenv loadaddr ${LOADADDR} ; setenv bootargs sd0i:/bsd.umg ; for dtype in sata mmc ; do for disk in 0 1 ; do \${dtype} dev \${disk} ; for fs in fat ext2 ; do if \${fs}load \${dtype} \${disk}:1 \${loadaddr} bsd.umg ; then bootm \${loadaddr} ; fi ; done; done; done; echo; echo failed to load bsd.umg 
	__EOT
			mkuboot -t script -a arm -o linux /tmp/6x_bootscript.scr /mnt/mnt/6x_bootscript
		fi
	elif [[ ${MDPLAT} == "SUNXI" ]]; then
		cat > /mnt/mnt/uenv.txt<<__EOT
bootargs=sd0i:/bsd
mmcboot=mmc rescan ; fatload mmc 0 ${LOADADDR} bsd.umg && bootm ${LOADADDR};
uenvcmd=run mmcboot;
__EOT
		cp /mnt/usr/mdec/cubie/u-boot-sunxi-with-spl.bin /mnt/mnt/
	fi
}

md_prep_fdisk() {
	local _disk=$1 _q _d

	local bootparttype="C"
	local bootsectorstart="64"
	local bootsectorsize="32768"
	local bootsectorend
	local bootfstype="msdos"
	local newfs_args=${NEWFSARGS_msdos}

	# imx needs an ext2fs filesystem
	if [[ ${MDPLAT} == "IMX" ]]; then
		bootparttype="83"
		bootfstype="ext2fs"
		newfs_args=${NEWFSARGS_ext2fs}
	fi
	if [[ -n $CUBOX ]]; then
		bootsectorstart="2048"
	fi
	bootsectorend=`expr $bootsectorstart + $bootsectorsize`

	while :; do
		_d=whole
		if [[ -n $(fdisk $_disk | grep 'Signature: 0xAA55') ]]; then
			fdisk $_disk
		else
			echo "MBR has invalid signature; not showing it."
		fi
		ask "Use (W)hole disk$_q or (E)dit the MBR?" "$_d"
		case $resp in
		w*|W*)
			echo -n "Creating a ${bootfstype} partition and an OpenBSD partition for rest of $_disk..."
			fdisk -e ${_disk} <<__EOT >/dev/null
reinit
e 0
${bootparttype}
n
${bootsectorstart}
${bootsectorsize}
f 0
e 3
A6
n
${bootsectorend}

write
quit
__EOT
			echo "done."
			disklabel $_disk 2>/dev/null | grep -q "^  i:" || disklabel -w -d $_disk
			newfs -t ${bootfstype} ${newfs_args} ${_disk}i
			return ;;
		e*|E*)
			# Manually configure the MBR.
			cat <<__EOT

You will now create one MBR partition to contain your OpenBSD data
and one MBR partition on which kernels are located which are loaded
by U-Boot. Neither partition will overlap any other partition.

The OpenBSD MBR partition will have an id of 'A6' and the boot MBR
partition will have an id of '${bootparttype}' (${bootfstype}). The boot partition will be
at least 16MB and be the first 'MSDOS' partition on the disk.

$(fdisk ${_disk})
__EOT
			fdisk -e ${_disk}
			[[ -n $(fdisk $_disk | grep ' A6 ') ]] && return
			echo No OpenBSD partition in MBR, try again. ;;
		o*|O*)	return ;;
		esac
	done
}

md_prep_disklabel() {
	local _disk=$1 _f=/tmp/fstab.$1

	md_prep_fdisk $_disk

	disklabel_autolayout $_disk $_f || return
	[[ -s $_f ]] && return

	cat <<__EOT

You will now create an OpenBSD disklabel inside the OpenBSD MBR
partition. The disklabel defines how OpenBSD splits up the MBR partition
into OpenBSD partitions in which filesystems and swap space are created.
You must provide each filesystem's mountpoint in this program.

The offsets used in the disklabel are ABSOLUTE, i.e. relative to the
start of the disk, NOT the start of the OpenBSD MBR partition.

__EOT

	disklabel $FSTABFLAG $_f -E $_disk
}

md_congrats() {
}

md_consoleinfo() {
}
