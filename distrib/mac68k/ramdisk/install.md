#!/bin/sh
#
#	$OpenBSD: install.md,v 1.2 1999/08/15 10:05:05 millert Exp $
#	$NetBSD: install.md,v 1.1.2.4 1996/08/26 15:45:14 gwr Exp $
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
# machine dependent section of installation/upgrade script
#

# Machine-dependent install sets
MDSETS="kernel"

TMPWRITEABLE=/tmp/writeable
KERNFSMOUNTED=/tmp/kernfsmounted

md_set_term() {
	echo -n "Specify terminal type [vt220]: "
	getresp "vt220"
	TERM="$resp"
	export TERM
	# set screensize (i.e., for an xterm)
	rows=`stty -a | grep rows | cutword 4`
	columns=`stty -a | grep columns | cutword 6`
	if [ "$rows" -eq 0 -o "$columns" -eq 0 ]; then
		echo -n "Specify terminal rows [25]: "
		getresp "25"
		rows="$resp"

		echo -n "Specify terminal columns [80]: "
		getresp "80"
		columns="$resp"

		stty rows "$rows" columns "$columns"
	fi
}

md_makerootwritable() {

	if [ -e ${TMPWRITEABLE} ]
	then
		md_mountkernfs
		return
	fi
	if ! mount -t ffs  -u /dev/rd0a / ; then
		cat << \__rd0_failed_1

FATAL ERROR: Can't mount the ram filesystem.

__rd0_failed_1
		exit
	fi

	# Bleh.  Give mount_mfs a chance to DTRT.
	sleep 2
	> ${TMPWRITEABLE}

	md_mountkernfs
}

md_mountkernfs() {
	if [ -e ${KERNFSMOUNTED} ]
	then
		return
	fi
	if ! mount -t kernfs /kern /kern
	then
		cat << \__kernfs_failed_1
FATAL ERROR: Can't mount kernfs filesystem
__kernfs_failed_1
		exit
	fi
	> ${KERNFSMOUNTED} 
}

md_machine_arch() {
	cat /kern/machine
}

md_get_diskdevs() {
	# return available disk devices
	cat /kern/msgbuf | egrep "^sd[0-9]+ " | cutword 1 | sort -u
}

md_get_cddevs() {
	# return available CD-ROM devices
	cat /kern/msgbuf | egrep "^cd[0-9]+ " | cutword 1 | sort -u
}

md_get_partition_range() {
	# return range of valid partition letters
	echo "[a-p]"
}

md_installboot() {
	:
}

md_checkfordisklabel() {
	# We don't have disklabels.
	return 0
}

md_prep_disklabel()
{
	# We don't have disklabels.
}

# Note, while they might not seem machine-dependent, the
# welcome banner and the punt message may contain information
# and/or instructions specific to the type of machine.

md_welcome_banner() {
(
	if [ "$MODE" = "install" ]; then
		echo "Welcome to the OpenBSD/mac68k ${VERSION_MAJOR}.${VERSION_MINOR} installation program."
		cat << \__welcome_banner_1

This program is designed to help you put OpenBSD on your system in a
simple and rational way.
__welcome_banner_1

	else
		echo "Welcome to the OpenBSD/mac68k ${VERSION_MAJOR}.${VERSION_MINOR} upgrade program."
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

As with anything which modifies your disk's contents, this program can
cause SIGNIFICANT data loss, and you are advised to make sure your
data is backed up before beginning the installation process.

Default answers are displayed in brackets after the questions.  You
can hit Control-C at any time to quit, but if you do so at a prompt,
you may have to hit return.  Also, quitting in the middle of
installation may leave your system in an inconsistent state.

__welcome_banner_3
) | less -E
}

md_not_going_to_install() {
		cat << \__not_going_to_install_1

OK, then.  Enter 'halt' at the prompt to halt the machine.  Once the
machine has halted, reset the system in order to reboot.

__not_going_to_install_1
}

md_congrats() {
	local what;
	if [ "$MODE" = install ]; then
		what=installed
	else
		what=upgraded
	fi
	cat << \__congratulations_1

CONGRATULATIONS!  You have successfully $what OpenBSD!  To boot the
installed system, enter halt at the command prompt.  Once the system has
halted, reset the machine and boot from the disk.

__congratulations_1
}

md_native_fstype() {
	# Nothing to do.
}

md_native_fsopts() {
	# Nothing to do.
}
