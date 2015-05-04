#	$OpenBSD: install.md,v 1.28 2015/05/04 19:55:26 rpe Exp $
#
# machine dependent section of installation/upgrade script.
#

MDTERM=vt100
NCPU=$(sysctl -n hw.ncpufound)

((NCPU > 1)) && { DEFAULTSETS="bsd bsd.rd bsd.mp"; SANESETS="bsd bsd.mp"; }

md_installboot() {
	if ! installboot -r /mnt ${1}; then
		echo "\nFailed to install bootblocks."
		echo "You will not be able to boot OpenBSD from ${1}."
		exit
	fi
}

md_prep_disklabel() {
	local _disk=$1 _f _op

	installboot $_disk

	_f=/tmp/fstab.$_disk
	if [[ $_disk == $ROOTDISK ]]; then
		if $AUTO && get_disklabel_template; then
			disklabel -T /disklabel.auto $FSTABFLAG $_f -w -A $_disk && return
			echo "Autopartitioning failed"
			exit 1
		fi
		while :; do
			echo "The auto-allocated layout for $_disk is:"
			disklabel -h -A $_disk | egrep "^#  |^  [a-p]:"
			ask "Use (A)uto layout, (E)dit auto layout, or create (C)ustom layout?" a
			case $resp in
			a*|A*)	_op=-w ;;
			e*|E*)	_op=-E ;;
			c*|C*)	break ;;
			*)	continue ;;
			esac
			disklabel $FSTABFLAG $_f $_op -A $_disk
			return
		done
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
