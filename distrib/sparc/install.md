#	$NetBSD: install.md,v 1.1 1996/01/06 22:42:13 pk Exp $
#
# Copyright (c) 1995 Jason R. Thorpe.
# All rights reserved.
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
#	This product includes software developed for the NetBSD Project
#	by Jason R. Thorpe.
# 4. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

#
# machine dependent section of installation/upgrade script.
#

md_set_term() {
	if [ ! -z "$TERM" ]; then
		return
	fi
	echo -n "Specify terminal type [sun]: "
	getresp "sun"
	TERM="$resp"
	export TERM
}

md_get_diskdevs() {
	# return available disk devices
	dmesg | egrep "(^sd[0-9] |^x[dy][0-9] )" | cut -d" " -f1 | sort -u
}

md_get_cddevs() {
	# return available CDROM devices
	dmesg | grep "^cd[0-9] " | cut -d" " -f1 | sort -u
}

md_get_ifdevs() {
	# return available network devices
	dmesg | egrep "(^le[0-9] |^ie[0-9] )" | cut -d" " -f1 | sort -u
}

md_installboot() {
	echo "Installing boot block..."
	/usr/mdec/binstall -v ffs /mnt
}

md_checkfordisklabel() {
	# $1 is the disk to check

	disklabel $1 > /dev/null 2> /tmp/checkfordisklabel
	if grep "no disk label" /tmp/checkfordisklabel; then
		rval="1"
	elif grep "disk label corrupted" /tmp/checkfordisklabel; then
		rval="2"
	else
		rval="0"
	fi

	rm -f /tmp/checkfordisklabel
}

md_prep_disklabel()
{
	# display example
	cat << \__md_prep_disklabel_1
Here is an example of what the partition information will look like once
you have entered the disklabel editor. Disk partition sizes and offsets
are in sector (most likely 512 bytes) units. Make sure these size/offset
pairs are on cylinder boundaries (the number of sector per cylinder is
given in the `sectors/cylinder' entry, which is not shown here).

Do not change any parameters except the partition layout and the label name.
It's probably also wisest not to touch the `8 partitions:' line, even
in case you have defined less than eight partitions.

[Example]
8 partitions:
#        size   offset    fstype   [fsize bsize   cpg]
  a:    50176        0    4.2BSD     1024  8192    16   # (Cyl.    0 - 111)
  b:    64512    50176      swap                        # (Cyl.  112 - 255)
  c:   640192        0   unknown                        # (Cyl.    0 - 1428)
  d:   525504   114688    4.2BSD     1024  8192    16   # (Cyl.  256 - 1428)
[End of example]

__md_prep_disklabel_1
}

md_welcome_banner() {
{
	if [ "$MODE" = "install" ]; then
		echo ""
		echo "Welcome to the NetBSD/sparc ${VERSION} installation program."
		cat << \__welcome_banner_1

This program is designed to help you put NetBSD on your disk,
in a simple and rational way.  You'll be asked several questions,
and it would probably be useful to have your disk's hardware
manual, the installation notes, and a calculator handy.
__welcome_banner_1

	else
		echo ""
		echo "Welcome to the NetBSD/sparc ${VERSION} upgrade program."
		cat << \__welcome_banner_2

This program is designed to help you upgrade your NetBSD system in a
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

Default answers are displyed in brackets after the questions.
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

CONGRATULATIONS!  You have successfully $what NetBSD!
To boot the installed system, enter halt at the command prompt. Once the
system has halted, reset the machine and boot from the disk.

__congratulations_1
}
