#	$OpenBSD: install.md,v 1.17 2004/08/06 22:30:02 pefo Exp $
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

# Machine-dependent install sets
MDSETS=kernel
MDTERM=vt220
ARCH=ARCH

md_set_term() {
}

md_installboot() {
	echo "Installing boot TBD, only netboot for now"
	return
	echo "Installing boot on /dev/${1}a"
	cp /usr/mdec/boot /mnt/boot
	/usr/mdec/installboot /mnt/boot /usr/mdec/bootxx /dev/r${1}a
}

md_checkfordisklabel() {
	# $1 is the disk to check
	local rval

	disklabel $1 > /dev/null 2> /tmp/checkfordisklabel
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

md_prep_disklabel()
{
	local _disk

	_disk=$1
	md_checkfordisklabel $_disk
	case $? in
	0)	ask "Do you wish to edit the disklabel on $_disk?" y
		;;
	1)	echo "WARNING: Disk $_disk has no label"
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

	disklabel -W ${_disk}
	disklabel ${_disk} >/tmp/label.$$
	disklabel -r -R ${_disk} /tmp/label.$$
	rm -f /tmp/label.$$
	disklabel -f /tmp/fstab.${_disk} -E ${_disk}
}

md_congrats() {
	cat << __EOT

Once the machine has rebooted use netbooting to boot into OpenBSD,
as described in the install document.
__EOT
}
