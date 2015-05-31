#       $OpenBSD: install.md,v 1.8 2015/05/31 21:21:10 rpe Exp $
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
#

MDTERM=vt100
NCPU=$(sysctl -n hw.ncpufound)

((NCPU > 1)) && { DEFAULTSETS="bsd bsd.rd bsd.mp"; SANESETS="bsd bsd.mp"; }

_mdnoautoinstallboot=n

md_installboot() {
	local _disk=$1

	if [[ $_mdnoautoinstallboot == y ]]; then
		cat << __EOT

Do you want to install the OpenBSD boot blocks on ${_disk}? If you intend
to share the disk with DG/UX, you are advised to keep the existing DG/UX
boot blocks and put the OpenBSD boot blocks on the DG/UX root partition.

If you no longer intend to boot DG/UX from this disk, answer `yes'.

__EOT
		ask_yn "Install OpenBSD boot blocks?" "yes"
		[[ $resp = n ]] &&  return
	fi
	/mnt/usr/mdec/installboot /mnt/usr/mdec/boot $_disk
}

# true if the device has a boot area initialized
md_has_boot_area () {
	/usr/mdec/vdmtool $1 2>&1 | grep -q "disk boot info"
}

# true if the device seems to have DG/UX VDM partitioning
md_has_vdm () {
	/usr/mdec/vdmtool $1 2>&1 | grep -q "^vdit entry"
}

# true if the device seems to have DG/UX LDM partitioning
md_has_ldm () {
	# until vdmtool can grok them...
	/usr/mdec/vdmtool $1 2>&1 | \
	    grep -q "vdmtool: unexpected block kind on sector 00000001: ff"
}

md_prep_disklabel() {
	local _disk=$1 _f=/tmp/fstab.$1 _shared=n

	if md_has_boot_area $_disk; then
		if md_has_ldm $_disk; then
			cat << __EOT

WARNING: there seem to be existing DG/UX LDM partitions on ${_disk}. These
partitions will NOT be recognized by OpenBSD and will be overwritten.

__EOT
			ask_yn "Are you *sure* you want to setup $_disk?"
			[[ $resp == n ]] && return 1
			_shared=y
		fi

		if md_has_vdm $_disk; then
			cat << __EOT

WARNING: there seem to existing DG/UX VDM partitions on ${_disk}. These
partitions will NOT be recognized by OpenBSD and will be overwritten,
unless a dedicated OpenBSD vdmpart has been set up in DG/UX as described
in the installation notes.

__EOT
			ask_yn "Are you *sure* you want to setup $_disk?"
			[[ $resp == n ]] && return 1
			_shared=y
		fi
	else
		# Initialize boot area for the root disk, before attempting
		# to label it, to make sure the OpenBSD boundary will not
		# contain the boot area.
		if [[ $_disk == $ROOTDISK ]]; then
			/usr/mdec/vdmtool -i $_disk
		fi
	fi

	disklabel_autolayout $_disk $_f || return
	if [[ -s $_f ]]; then
		_mdnoautoinstallboot=$_shared
		return
	fi

	cat <<__EOT
You will now create a OpenBSD disklabel on the disk.  The disklabel defines
how OpenBSD splits up the disk into OpenBSD partitions in which filesystems
and swap space are created.  You must provide each filesystem's mountpoint
in this program.

__EOT

	disklabel $FSTABFLAG $_f -E $_disk
}

md_congrats() {
}

md_consoleinfo() {
}
