#	$OpenBSD: install.md,v 1.28 1998/03/12 08:49:23 deraadt Exp $
#
#
# Copyright rc) 1996 The NetBSD Foundation, Inc.
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

# Machine-dependent install sets
MDSETS="kernel"

md_set_term() {
	if [ ! -z "$TERM" ]; then
		return
	fi
	echo -n "Specify terminal type ["$TERM"]: "
	getresp "$TERM"
	TERM="$resp"
	export TERM
}

md_makerootwritable() {
}

md_machine_arch() {
	cat /kern/machine
}

md_get_diskdevs() {
	# return available disk devices
	cat /kern/msgbuf | egrep "^[sw]d[0-9]+ " | cut -d" " -f1 | sort -u
}

md_get_cddevs() {
	# return available CDROM devices
	cat /kern/msgbuf | egrep "^a?cd[0-9]+ " | cut -d" " -f1 | sort -u
}

md_get_ifdevs() {
	# return available network devices
	cat /kern/msgbuf | egrep "^(e[dglp][0-9] |[dil]e[0-9] |f[ep]a[0-9] |fxp[0-9])" | cut -d" " -f1 | sort -u
}

md_get_partition_range() {
    # return range of valid partition letters
    echo "[a-p]"
}

md_installboot() {
	echo "Installing boot block..."
	cp /usr/mdec/boot /mnt/boot
	sync; sync; sync
	/usr/mdec/installboot -v /mnt/boot /usr/mdec/biosboot ${1}
}

md_native_fstype() {
    echo "msdos"
}

md_native_fsopts() {
    echo "ro"
}

md_checkfordisklabel() {
	# $1 is the disk to check
	local rval

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

	_done=0
	echo
	cat << \__md_prep_fdisk_1
A single OpenBSD partition with id 'A6' ('OpenBSD') should exist in the MBR.
All of your OpenBSD partitions will be contained _within_ this partition,
including your swap space.  In the normal case it should be the only partition
marked as active.  (Unless you are using a multiple-OS booter, but you can
adjust that later.)  Furthermore, the MBR partitions must NOT overlap each
other.  [If this is a new install, you are most likely going to want to type
the following fdisk commands: reinit, update, write, quit. Use the 'manual'
command to read a full description.]  The current partition information is:

__md_prep_fdisk_1
	fdisk ${_disk}
	echo
	fdisk -e ${_disk}

	echo "Here is the partition information you chose:"
	echo
	fdisk ${_disk}
	echo
}

md_prep_disklabel()
{
	local _disk

	_disk=$1
	md_prep_fdisk ${_disk}

	cat << \__md_prep_disklabel_1

Inside the BIOS 'A6' ('OpenBSD') partition you just created, there resides an
OpenBSD partition table which defines how this BIOS partition is to be split
up. This table declares the offsets and sizes of your / partition, your swap
space, and any other partitions you might create.  (NOTE: The OpenBSD disk
label offsets are absolute, ie. relative to the start of the disk... NOT
relative to the start of the BIOS 'A6' partition).

__md_prep_disklabel_1

	md_checkfordisklabel $_disk
	case $? in
	0)
		;;
	1)
		echo "WARNING: Disk $_disk has no label. You will be creating a new one."
		echo
		;;
	2)
		echo "WARNING: Label on disk $_disk is corrupted. You will be repairing."
		echo
		;;
	esac

	# display example
	cat << \__md_prep_disklabel_1
If this disk is shared with other operating systems, those operating systems
should have a BIOS partition entry that spans the space they occupy completely.
For safety, also make sure all OpenBSD file systems are within the offset and
size specified in the 'A6' BIOS partition table.  (By default, the disklabel
editor will try to enforce this).  If you are unsure of how to use multiple
partitions properly (ie. seperating /,  /usr, /tmp, /var, /usr/local, and other
things) just split the space into a root and swap partition for now.

__md_prep_disklabel_1
	disklabel -E ${_disk}
}

md_copy_kernel() {
	#echo -n "Copying kernel..."
	#cp -p /bsd /mnt/bsd
	#echo "done."
}

md_welcome_banner() {
{
	if [ "$MODE" = "install" ]; then
		echo "Welcome to the OpenBSD/i386 ${VERSION_MAJOR}.${VERSION_MINOR} installation program."
		cat << \__welcome_banner_1

This program is designed to help you put OpenBSD on your disk, in a simple
and rational way.  You'll be asked several questions, and it would probably
be useful to have your disk's hardware manual, the installation notes, and a
calculator handy.
__welcome_banner_1

	else
		echo ""
		echo "Welcome to the OpenBSD/i386 ${VERSION_MAJOR}.${VERSION_MINOR} upgrade program."
		cat << \__welcome_banner_2

This program is designed to help you upgrade your OpenBSD system in a simple
and rational way.  As a reminder, installing the `etc' binary set is NOT
recommended.  Once the rest of your system has been upgraded, you should
manually merge any changes to files in the `etc' set into those files which
already exist on your system.
__welcome_banner_2
	fi

cat << \__welcome_banner_3

As with anything which modifies your disk's contents, this program can cause
SIGNIFICANT data loss, and you are advised to make sure your data is backed
up before beginning the installation process.

Default answers are displayed in brackets after the questions.  You can hit
Control-C at any time to quit, but if you do so at a prompt, you may have
to hit return.  Also, quitting in the middle of installation may leave your
system in an inconsistent state.  If you hit Control-C and restart the
install, the install program will remember many of your old answers.

__welcome_banner_3
} | more
}

md_not_going_to_install() {
	cat << \__not_going_to_install_1

OK, then.  Enter `halt' at the prompt to halt the machine.  Once the machine
has halted, power-cycle the system to load new boot code.

__not_going_to_install_1
}

md_congrats() {
	local what;
	if [ "$MODE" = "install" ]; then
		what="installed";
	else
		what="upgraded";
	fi
	cat << __congratulations_1

CONGRATULATIONS!  You have successfully $what OpenBSD!  To boot the
installed system, enter halt at the command prompt. Once the system has
halted, reset the machine and boot from the disk.

__congratulations_1
}
