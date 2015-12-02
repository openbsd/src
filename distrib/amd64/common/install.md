#	$OpenBSD: install.md,v 1.47 2015/12/02 21:17:16 krw Exp $
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

MDXAPERTURE=2
MDXDM=y
NCPU=$(sysctl -n hw.ncpufound)

if dmesg | grep -q 'efifb0 at mainbus0'; then
	MDEFI=y
fi

((NCPU > 1)) && { DEFAULTSETS="bsd bsd.rd bsd.mp"; SANESETS="bsd bsd.mp"; }

md_installboot() {
	if ! installboot -r /mnt ${1}; then
		echo "\nFailed to install bootblocks."
		echo "You will not be able to boot OpenBSD from ${1}."
		exit
	fi
}

md_prep_fdisk() {
	local _disk=$1 _q _d

	while :; do
		_d=whole
		_q="Use (W)hole disk MBR, whole disk (G)PT"

		[[ $MDEFI == y ]] && _d=gpt

		if fdisk $_disk | grep -q 'Signature: 0xAA55'; then
			fdisk $_disk
			if fdisk $_disk | grep -q '^..: A6 '; then
				_d=OpenBSD
			fi
		elif fdisk $_disk | grep -q "First usable LBA:"; then
			fdisk $_disk
			if fdisk $_disk | grep -q "^      OpenBSD "; then
				_d=OpenBSD
			fi
		else
			echo "No valid MBR or GPT."
		fi
		[[ $_d == OpenBSD ]] && _q="$_q, (O)penBSD area"

		ask "$_q or (E)dit?" "$_d"
		case $resp in
		w*|W*)
			echo -n "Setting OpenBSD MBR partition to whole $_disk..."
			fdisk -iy $_disk >/dev/null
			echo "done."
			return ;;
		g*|G*)
			if [[ $MDEFI != y ]]; then
				ask_yn "An EFI/GPT disk may not boot. Proceed?"
				[[ $resp == n ]] && continue
			fi

			echo -n "Setting OpenBSD GPT partition to whole $_disk..."
			fdisk -iy -g -b 960 $_disk >/dev/null
			echo "done."
			return ;;
		e*|E*)
			if fdisk $_disk | grep -q "First usable LBA:"; then
				# Manually configure the GPT.
				cat <<__EOT

You will now create two GPT partitions. The first must have an id
of 'EF' and be large enough to contain the OpenBSD boot programs,
at least 960 blocks. The second must have an id of 'A6' and will
contain your OpenBSD data. Neither may overlap other partitions.
Inside the fdisk command, the 'manual' command describes the fdisk
commands in detail.

$(fdisk $_disk)
__EOT
				fdisk -e $_disk
				_d=$(fdisk $_disk | grep "^      OpenBSD ")
				_q=$(fdisk $_disk | grep "^      EFI Sys ")
				if [[ -z $_d ]]; then
					echo -n "No OpenBSD partition in GPT,"
				elif [[ -z $_q ]]; then
					echo -n "No EFI Sys partition in GPT,"
				else
					return
				fi
			else
				# Manually configure the MBR.
				cat <<__EOT

You will now create a single MBR partition to contain your OpenBSD data. This
partition must have an id of 'A6'; must *NOT* overlap other partitions; and
must be marked as the only active partition.  Inside the fdisk command, the
'manual' command describes all the fdisk commands in detail.

$(fdisk $_disk)
__EOT
				fdisk -e $_disk
				fdisk $_disk | grep -q ' A6 ' && return
				echo -n "No OpenBSD partition in MBR,"
			fi
			echo "try again." ;;
		o*|O*)
			[[ $_d == OpenBSD ]] || continue
			_d=$(fdisk $_disk | grep "First usable LBA:")
			_q=$(fdisk $_disk | grep "^      EFI Sys ")
			if [[ -n $_d && -z $_q ]]; then
				echo "No EFI Sys partition in GPT, try again."
				continue
			fi
			return ;;
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

	disklabel -F $_f -E $_disk
}

md_congrats() {
}

md_consoleinfo() {
	local _u _d=com

	for _u in $(scan_dmesg "/^$_d\([0-9]\) .*/s//\1/p"); do
		if [[ $_d$_u == $CONSOLE || -z $CONSOLE ]]; then
			CDEV=$_d$_u
			CPROM=com$_u
			CTTY=tty0$_u
			return
		fi
	done
}
