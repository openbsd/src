#	$OpenBSD: install.md,v 1.16 2002/12/04 13:11:23 miod Exp $
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
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
#        This product includes software developed by the NetBSD
#        Foundation, Inc. and its contributors.
# 4. Neither the name of The NetBSD Foundation nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
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

MDTERM=vt100
MDFSTYPE=msdos
ARCH=ARCH

md_set_term() {
}

md_installboot() {
	if [[ $disklabeltype = "HFS" ]]
	then
		echo "the 'ofwboot' program needs to be copied to the first HFS partition"
		echo "of the disk to allow booting of OpenBSD"
	elif [[ $disklabeltype = "MBR" ]]
	then
		echo "Installing boot in the msdos partition /dev/${1}i"
		if mount -t msdos /dev/${1}i /mnt2 ; then
			cp /usr/mdec/ofwboot /mnt2
			umount /mnt2
		else
			echo "Failed, you will not be able to boot from /dev/${1}."
		fi
	fi
}

md_init_mbr() {
	# $1 is the disk to init
	echo
	echo "You will now be asked if you want to initialize the disk with a 1MB"
	echo "MSDOS partition. This is the recommended setup and will allow you to"
	echo "store the boot and other interesting things here."
	echo
	echo "If you want to have a different setup, exit 'install' now and do"
	echo "the MBR initialization by hand using the 'fdisk' program."
	echo
	echo "If you choose to manually setup the MSDOS partition, "
	echo "consult your PowerPC OpenFirmware manual -and- the"
	echo "PowerPC OpenBSD Installation Guide for doing setup this way."
	echo
	ask "Do you want to init the MBR and the MSDOS partition?" y
	case "$resp" in
	n*|N*)	exit 0;;
	*)	echo
		echo "An MBR record with an OpenBSD usable partition table will now be copied"
		echo "to your disk. Unless you have special requirements you will not need"
		echo "to edit this MBR. After the MBR is copied an empty 1MB MSDOS partition"
		echo "will be created on the disk. You *MUST* setup the OpenBSD disklabel"
		echo "to have a partition include this MSDOS partition."
		echo "You will have an opportunity to do this shortly."
		echo
		echo "You will probably see a few '...: no disk label' messages"
		echo "It's completely normal. The disk has no label yet."
		echo "This will take a minute or two..."
		sleep 2
		echo -n "Creating Master Boot Record (MBR)..."
		fdisk -i -f /usr/mdec/mbr $1
		echo "..done."
		echo -n "Copying 1MB MSDOS partition to disk..."
		gunzip < /usr/mdec/msdos1mb.gz | dd of=/dev/r$1c bs=512 seek=1 >/dev/null 2>&1
		echo "..done."
		;;
	esac
}

md_init_hfs() {
	pdisk /dev/${1}c
}
md_checkfordisklabel() {
	# $1 is the disk to check
	local rval

	echo
	echo "Apple systems have two methods to label/partition a boot disk."
	echo "Either the disk can be partitioned with Apple HFS partition"
	echo "tools to contain an \"Unused\" partition, or without any"
	echo "MacOS tools, the disk can be labled using an MBR partition table"
	echo "If the HFS (DPME) partition table is used, after the disk is"
	echo "partitioned with the Apple software, the \"Unused\" section"
	echo "must be changed to type \"OpenBSD\" name \"OpenBSD\" using the"
	echo "pdisk tool contained on this ramdisk. The disklabel can"
	echo "then be edited normally"
	echo "WARNING: the MBR partitioning code will HAPPILY overwrite/destroy"
	echo "any HFS partitions on the disk, including the partition table."
	echo "Choose the MBR option carefully, knowing this fact."

	ask "Do you want to choose (H)FS labeling or (M)BR labeling?" H
	case "$resp" in
	m*|M*)	export disklabeltype=MBR
		md_checkforMBRdisklabel $1
		rval=$?
		;;
	*)	export disklabeltype=HFS
		md_init_hfs $1
		rval=$?
		;;
	esac
	return $rval
}
md_checkforMBRdisklabel() {

	echo "You have chosen to put a MBR disklabel on the disk."
	ask "Is this correct?" n
	case "$resp" in
	n*|N*)	echo "aborting install"
		exit 0;;
	esac

	ask "Have you initialized an MSDOS partition using OpenFirmware?" n
	case "$resp" in
	n*|N*)	md_init_mbr $1;;
	*)	echo
		echo "You may keep your current setup if you want to be able to use any"
		echo "already loaded OS. However you will be asked to prepare an empty"
		echo "partition for OpenBSD later. There must also be at least ~0.5MB free space"
		echo "in the boot partition to hold the OpenBSD bootloader."
		echo
		echo "Also note that the boot partition must be included as partition"
		echo "'i' in the OpenBSD disklabel."
		echo
		ask "Do you want to keep the current MSDOS partition setup?" y
		case "$resp" in
		n*|N*)	md_init_mbr $1;;
		esac
		;;
	esac

	disklabel -r $1 > /dev/null 2> /tmp/checkfordisklabel
	if grep "no disk label" /tmp/checkfordisklabel; then
		rval=1
	elif grep "disk label corrupted" /tmp/checkfordisklabel; then
		rval=2
	else
		rval=0
	fi

	rm -f /tmp/checkfordisklabel
	return $rval
}

md_prep_fdisk()
{
	local _disk
	local _done

	_disk=$1
	echo
	echo "This disk has not previously been used with OpenBSD. You may share"
	echo "this disk with other operating systems. However, to be able to boot"
	echo "the system you will need a small DOS partition in the beginning of"
	echo "the disk to hold the kernel boot. OpenFirmware understands"
	echo "how to read an MSDOS style format from the disk."
	echo
	echo "This DOS style partitioning has been taken care of if"
	echo "you chose to do that initialization earlier in the install."
	echo
	echo "WARNING: Wrong information in the BIOS partition table might"
	echo "render the disk unusable."

	ask "Press [Enter] to continue"

	echo
	echo "Current partition information is:"
	fdisk ${_disk}
	ask "Press [Enter] to continue"

	_done=0
	while [ $_done = 0 ]; do
		echo
		cat << __EOT

An OpenBSD partition should have type (i.d.) of 166 (A6), and should be the
only partition marked as active. Also make sure that the size of the partition
to be used by OpenBSD is correct, otherwise OpenBSD disklabel installation
will fail. Furthermore, the partitions must NOT overlap each others.

The fdisk utility will be started update mode (interactive.)
You will be able to add / modify this information as needed.
If you make a mistake, simply exit fdisk without storing the new
information, and you will be allowed to start over.
__EOT
		echo
		ask "Press [Enter] to continue"

		fdisk -e ${_disk}

		echo
		echo "The new partition information is:"
		fdisk ${_disk}

		echo
		echo "(You will be permitted to edit this information again.)"
		echo "-------------------------------------------------------"
		ask "Is the above information correct?" n

		case "$resp" in
		n*|N*)	;;
		*)	_done=1 ;;
		esac
	done

	echo
	echo "Please take note of the offset and size of the OpenBSD partition"
	echo "*AND* the MSDOS partitions you may want to access from OpenBSD."
	echo "At least the MSDOS partition used for booting must be accessible"
	echo "by OpenBSD as partition 'i'. You may need this information to "
	echo "fill in the OpenBSD disk label later."
	ask "Press [Enter] to continue"
}

md_prep_disklabel()
{
	local _disk

	_disk=$1
	md_checkfordisklabel $_disk
	case $? in
	0)	ask "Do you wish to edit the disklabel on $_disk?" y
		;;
	1)	md_prep_fdisk ${_disk}
		echo "WARNING: Disk $_disk has no label"
		ask "Do you want to create one with the disklabel editor?" y
		;;
	2)	echo "WARNING: Label on disk $_disk is corrupted"
		ask "Do you want to try and repair the damage using the disklabel editor?" y
		;;
	esac

	case "$resp" in
	y*|Y*)	;;
	*)	return ;;
	esac

	# display example
	cat << __EOT

Disk partition sizes and offsets are in sector (most likely 512 bytes) units.
You may set these size/offset pairs on cylinder boundaries
     (the number of sector per cylinder is given in )
     (the 'sectors/cylinder' entry, which is not shown here)
Also, you *must* make sure that the 'i' partition points at the MSDOS
partition that will be used for booting. The 'c' partition shall start
at offset 0 and include the entire disk. This is most likely correct when
you see the default label in the editor.

Do not change any parameters except the partition layout and the label name.

   [Here is an example of what the partition information may look like.]
10 partitions:
#        size   offset    fstype   [fsize bsize   cpg]
  a:   120832    10240    4.2BSD     1024  8192    16   # (Cyl.   11*- 142*)
  b:   131072   131072      swap                        # (Cyl.  142*- 284*)
  c:  6265200        0    unused     1024  8192         # (Cyl.    0 - 6809)
  e:   781250   262144    4.2BSD     1024  8192    16   # (Cyl.  284*- 1134*)
  f:  1205000  1043394    4.2BSD     1024  8192    16   # (Cyl. 1134*- 2443*)
  g:  2008403  2248394    4.2BSD     1024  8192    16   # (Cyl. 2443*- 4626*)
  h:  2008403  4256797    4.2BSD     1024  8192    16   # (Cyl. 4626*- 6809*)
  i:    10208       32     MSDOS                        # (Cyl.    0*- 11*)
[End of example]
__EOT
	ask "Press [Enter] to continue"

	if [[ $disklabeltype = "HFS" ]]
	then
		disklabel -c -f /tmp/fstab.${_disk} -E ${_disk}
	elif [[ $disklabeltype = "MBR" ]]
	then
		disklabel -W ${_disk}
		disklabel ${_disk} >/tmp/label.$$
		disklabel -r -R ${_disk} /tmp/label.$$
		rm -f /tmp/label.$$
		disklabel -f /tmp/fstab.${_disk} -E ${_disk}
	else
		echo "unknown disk label type"
	fi
}

md_congrats() {
	cat << __EOT

Once the machine has rebooted use Open Firmware to boot into OpenBSD,
as described in the install document.
__EOT
}
