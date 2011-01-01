#	$OpenBSD: install.md,v 1.18 2011/01/01 17:25:06 deraadt Exp $
#
# machine dependent section of installation/upgrade script.
#

MDTERM=vt100
NCPU=$(sysctl -n hw.ncpufound)

((NCPU > 1)) && { DEFAULTSETS="bsd bsd.rd bsd.mp" ; SANESETS="bsd bsd.mp" ; }

md_installboot() {
	if [[ -f /mnt/bsd.mp ]] && ((NCPU > 1)); then
		echo "Multiprocessor machine; using bsd.mp instead of bsd."
		mv /mnt/bsd /mnt/bsd.sp 2>/dev/null
		mv /mnt/bsd.mp /mnt/bsd
	fi

	/sbin/disklabel -B $1
}

md_prep_disklabel() {
	local _disk=$1 _f _op

	md_installboot $_disk

	disklabel -W $_disk >/dev/null 2>&1
	_f=/tmp/fstab.$_disk
	if [[ $_disk == $ROOTDISK ]]; then
		while :; do
			echo "The auto-allocated layout for $_disk is:"
			disklabel -h -A $_disk | egrep "^#  |^  [a-p]:"
			ask "Use (A)uto layout, (E)dit auto layout, or create (C)ustom layout?" a
			case $resp in
			a*|A*)	_op=-w ; AUTOROOT=y ;;
			e*|E*)	_op=-E ;;
			c*|C*)	break ;;
			*)	continue ;;
			esac
			disklabel -f $_f $_op -A $_disk
			return
		done
	fi
	cat <<__EOT
You will now create a OpenBSD disklabel on the disk.  The disklabel defines
how OpenBSD splits up the disk into OpenBSD partitions in which filesystems
and swap space are created.  You must provide each filesystem's mountpoint
in this program.

__EOT

	disklabel -f $_f -E $_disk
}

md_congrats() {
}

md_consoleinfo() {
}
