#	$OpenBSD: install.md,v 1.18 2003/02/16 23:16:44 krw Exp $
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

MDFSTYPE=msdos
MDXAPERTURE=2
ARCH=ARCH

md_set_term() {
}

md_installboot() {
	local _disk=$1

	case $disklabeltype in
	HFS)	cat << __EOT
The 'ofwboot' program needs to be copied to the first HFS partition of $_disk
to allow booting of OpenBSD.
__EOT
		;;

	MBR)	echo "Installing boot in the msdos partition $_disk"
		if mount -t msdos /dev/$_disk /mnt2 ; then
			cp /usr/mdec/ofwboot /mnt2
			umount /mnt2
		else
			echo "Failed, you will not be able to boot from ${_disk}."
		fi
		;;
	esac
}

md_init_mbr() {
	local _disk=$1

	cat << __EOT

You will now be asked if you want to initialize $_disk with a 1MB MSDOS
partition. This is the recommended setup and will allow you to store the boot
and other interesting things here.

If you want to have a different setup, exit 'install' now and do the MBR
initialization by hand using the 'fdisk' program.

If you choose to manually setup the MSDOS partition, consult your PowerPC
OpenFirmware manual -and- the PowerPC OpenBSD Installation Guide for doing
setup this way.

__EOT
	ask "Do you want to initialize the MBR and the MSDOS partition?" y
	case $resp in
	n*|N*)	exit 0	;;
	*)		;;
	esac

	cat << __EOT
An MBR record with an OpenBSD usable partition table will now be copied to your
disk. Unless you have special requirements, you will not need to edit this MBR.
After the MBR is copied an empty 1MB MSDOS partition will be created on the
disk. You *MUST* setup the OpenBSD disklabel to have a partition include this
MSDOS partition. You will have an opportunity to do this shortly.

You will probably see a few '...: no disk label' messages. It's completely
normal. The disk has no label yet. This may take a minute or two...
__EOT
	sleep 2

	echo -n "Creating Master Boot Record (MBR)..."
	fdisk -i -f /usr/mdec/mbr $_disk
	echo "done."

	echo -n "Copying 1MB MSDOS partition to disk..."
	gunzip < /usr/mdec/msdos1mb.gz | dd of=/dev/r${_disk}c bs=512 seek=1 >/dev/null 2>&1
	echo "done."
}

md_checkfordisklabel() {
	local rval _disk=$1

	cat << __EOT

Apple systems have two methods to label/partition a boot disk.

Either the disk can be partitioned with Apple HFS partition tools to contain an
"Unused" partition, or without any MacOS tools, the disk can be labeled using
an MBR partition table.

If the HFS (DPME) partition table is used, after the disk is partitioned with
the Apple software, the "Unused" section must be changed to type "OpenBSD" name
"OpenBSD" using the pdisk tool contained on this ramdisk. The disklabel can
then be edited normally.

WARNING: the MBR partitioning code will HAPPILY overwrite/destroy any HFS
	 partitions on the disk, including the partition table. Choose the
         MBR option carefully, knowing this fact.
__EOT

	ask "Do you want to use (H)FS labeling or (M)BR labeling" H
	case $resp in
	m*|M*)	export disklabeltype=MBR
		md_checkforMBRdisklabel $_disk
		rval=$?
		;;
	*)	export disklabeltype=HFS
		pdisk /dev/${_disk}c
		rval=$?
		;;
	esac
	return $rval
}

md_checkforMBRdisklabel() {
	local _disk=$1

	ask "Are you *sure* you want to put a MBR disklabel on the disk?" n
	case $resp in
	n*|N*)	echo "aborting install"
		exit 0;;
	esac

	ask "Have you initialized an MSDOS partition using OpenFirmware?" n
	case $resp in
	n*|N*)	md_init_mbr $_disk;;
	*)	cat << __EOT
You may keep your current setup if you want to be able to use any already
loaded OS. However you will be asked to prepare an empty partition for OpenBSD
later. There must also be at least ~0.5MB free space in the boot partition to
hold the OpenBSD bootloader.

Also note that the boot partition must be included as partition 'i' in the
OpenBSD disklabel.

__EOT
		ask "Do you want to keep the current MSDOS partition setup?" y
		case $resp in
		n*|N*)	md_init_mbr $_disk;;
		esac
	;;
	esac

	disklabel -r $_disk > /dev/null 2> /tmp/checkfordisklabel
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

md_prep_fdisk() {
	local _disk=$1

	cat << __EOT

This disk has not previously been used with OpenBSD. You may share this disk
with other operating systems. However, to be able to boot the system you will
need a small DOS partition in the beginning of the disk to hold the kernel
boot. OpenFirmware understands how to read an MSDOS style format from the disk.

This DOS style partitioning has been taken care of if you chose to do that
initialization earlier in the install.

WARNING: Wrong information in the BIOS partition table might render the disk
         unusable.
__EOT

	ask "Press [Enter] to continue"

	echo "\nCurrent partition information is:"
	fdisk $_disk
	ask "Press [Enter] to continue"

	while : ; do
		cat << __EOT

An OpenBSD partition should have a type (id) of 166 (A6), and should be the
only partition marked as active. Also make sure that the size of the partition
to be used by OpenBSD is correct, otherwise OpenBSD disklabel installation will
fail. Furthermore, the partitions must NOT overlap each others.

The fdisk utility will be started update mode (interactive.) You will be able
to add / modify this information as needed. If you make a mistake, simply exit
fdisk without storing the new information, and you will be allowed to start
over.

__EOT
		ask "Press [Enter] to continue"

		fdisk -e $_disk
		cat << __EOT

The new partition information is:

$(fdisk $_disk)

(You will be permitted to edit this information again.)
-------------------------------------------------------
__EOT
		ask "Is the above information correct?" n

		case $resp in
		n*|N*)	;;
		*)	break ;;
		esac
	done

	cat << __EOT

Please take note of the offset and size of the OpenBSD partition *AND* the
MSDOS partitions you may want to access from OpenBSD. At least the MSDOS
partition used for booting must be accessible by OpenBSD as partition 'i'. You
may need this information to fill in the OpenBSD disklabel later.

__EOT
	ask "Press [Enter] to continue"
}

md_prep_disklabel() {
	local _disk=$1

	md_checkfordisklabel $_disk
	case $? in
	0)	ask "Do you wish to edit the existing disklabel on $_disk?" y
		;;
	1)	md_prep_fdisk $_disk
		echo "WARNING: $_disk has no label"
		ask "Do you want to create one with the disklabel editor?" y
		;;
	2)	echo "WARNING: The disklabel on $_disk is invalid."
		ask "Do you want to try and repair the damage using the disklabel editor?" y
		;;

	esac

	case $resp in
	y*|Y*)	;;
	*)	return ;;
	esac

	# display example
	cat << __EOT

Disk partition sizes and offsets are in sector (most likely 512 bytes) units.
You may set these size/offset pairs on cylinder boundaries
     (the number of sector per cylinder is given in )
     (the 'sectors/cylinder' entry, which is not shown here)
Also, you *must* make sure that the 'i' partition points at the MSDOS partition
that will be used for booting. The 'c' partition shall start at offset 0 and
include the entire disk. This is most likely correct when you see the default
label in the editor.

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

	if [[ $disklabeltype == HFS ]]; then
		disklabel -c -f /tmp/fstab.$_disk -E $_disk
	elif [[ $disklabeltype == MBR ]]; then
		disklabel -W $_disk
		disklabel $_disk >/tmp/label.$$
		disklabel -r -R $_disk /tmp/label.$$
		rm -f /tmp/label.$$
		disklabel -f /tmp/fstab.$_disk -E $_disk
	else
		echo "unknown disk label type"
	fi
}

md_congrats() {
	cat << __EOT

Once the machine has rebooted use Open Firmware to boot into OpenBSD, as
described in the install document.
__EOT
}
