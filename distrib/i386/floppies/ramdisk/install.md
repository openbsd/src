#	$OpenBSD: install.md,v 1.18 1997/10/11 08:12:21 deraadt Exp $
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
	echo -n "Specify terminal type [pcvt25]: "
	getresp "pcvt25"
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
	cat /kern/msgbuf | egrep "^[sw]d[0-9] " | cut -d" " -f1 | sort -u
}

md_get_cddevs() {
	# return available CDROM devices
	cat /kern/msgbuf | egrep "^a?cd[0-9] " | cut -d" " -f1 | sort -u
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
	while [ $_done = 0 ]; do
		echo
		cat << \__md_prep_fdisk_1

A single OpenBSD partition with id "A6" should exist in the MBR.
It should be the only partition marked as active.  Furthermore, the
partitions must NOT overlap each others.  fdisk will be started in
edit mode, and you will be able to add this information as needed.
If you make a mistake, exit fdisk without storing the new information,
and you will be allowed to start over.
__md_prep_fdisk_1
		echo "Current partition information is:"
		fdisk ${_disk}

		fdisk -e ${_disk}

		echo
		echo "The new partition information is:"
		fdisk ${_disk}

		echo
		echo "Is this information correct (if not, you will be permitted to "
		echo -n "edit it again)? [n] "
		getresp "n"

		case "$resp" in
		n*|N*) ;;
		*) _done=1 ;;
		esac
	done

	echo "Please take note of the offset and size of the OpenBSD partition"
	echo "of the disk, as you will need that for the BSD disk label."
	echo -n "Press [Enter] to continue "
	getresp ""
}

md_prep_disklabel()
{
	local _disk

	_disk=$1
	md_prep_fdisk ${_disk}

	md_checkfordisklabel $_disk
	case $? in
	0)
		echo -n "Do you wish to edit the disklabel on $_disk? [y]"
		;;
	1)
		md_prep_fdisk ${_disk}
		echo "WARNING: Disk $_disk has no label"
		echo -n "Do you want to create one with the disklabel editor? [y]"
		;;
	2)
		echo "WARNING: Label on disk $_disk is corrupted"
		echo -n "Do you want to try and repair the damage using the disklabel editor? [y]"
		;;
	esac

	getresp "y"
	case "$resp" in
	y*|Y*) ;;
	*)	return ;;
	esac

	# display example
	cat << \__md_prep_disklabel_1

Here is an example of what the partition information will look like once
you have entered the disklabel editor. Disk partition sizes and offsets
are in sector (most likely 512 bytes) units. Make sure these size/offset
pairs are on cylinder boundaries (the number of sector per cylinder is
given in the `sectors/cylinder' entry, which is not shown here).

Also, if this disk is shared with other operating systems and have a BIOS
partition table, make sure all file systems reserved for OpenBSD are within
the offset and size specified in the BIOS partition table.

Do not change any parameters except the partition layout and the label name.

[Example]
16 partitions:
#        size   offset    fstype   [fsize bsize   cpg]
  a:    50176        0    4.2BSD     1024  8192    16   # (Cyl.    0 - 111)
  b:    64512    50176      swap                        # (Cyl.  112 - 255)
  c:   640192        0   unknown                        # (Cyl.    0 - 1428)
  d:   525504   114688    4.2BSD     1024  8192    16   # (Cyl.  256 - 1428)
[End of example]

__md_prep_disklabel_1
	echo -n "Press [Enter] to continue "
	getresp ""
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
		echo ""
		echo "Welcome to the OpenBSD/i386 ${VERSION} installation program."
		cat << \__welcome_banner_1

This program is designed to help you put OpenBSD on your disk,
in a simple and rational way.  You'll be asked several questions,
and it would probably be useful to have your disk's hardware
manual, the installation notes, and a calculator handy.
__welcome_banner_1

	else
		echo ""
		echo "Welcome to the OpenBSD/i386 ${VERSION} upgrade program."
		cat << \__welcome_banner_2

This program is designed to help you upgrade your OpenBSD system in a
simple and rational way.

As a reminder, installing the `etc' binary set is NOT recommended.
Once the rest of your system has been upgraded, you should manually
merge any changes to files in the `etc' set into those files which
already exist on your system.
__welcome_banner_2
	fi

cat << \__welcome_banner_3

As with anything which modifies your disk's contents, this
program can cause SIGNIFICANT data loss, and you are advised
to make sure your data is backed up before beginning the
installation process.

Default answers are displayed in brackets after the questions.
You can hit Control-C at any time to quit, but if you do so at a
prompt, you may have to hit return.  Also, quitting in the middle of
installation may leave your system in an inconsistent state.

__welcome_banner_3
} | more
}

md_not_going_to_install() {
	cat << \__not_going_to_install_1

OK, then.  Enter `halt' at the prompt to halt the machine.  Once the
machine has halted, power-cycle the system to load new boot code.

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

CONGRATULATIONS!  You have successfully $what OpenBSD!
To boot the installed system, enter halt at the command prompt. Once the
system has halted, reset the machine and boot from the disk.

__congratulations_1
}
