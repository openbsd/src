#	$OpenBSD: install.md,v 1.9 2009/04/10 23:11:17 krw Exp $
#
# machine dependent section of installation/upgrade script.
#

MDTERM=vt100
ARCH=ARCH

md_installboot() {
	echo -n "Installing boot block..."
	/sbin/disklabel -B $1
	echo "done."
}

md_prep_disklabel() {
	local _disk=$1 _f _op

	disklabel -W $_disk >/dev/null 2>&1
	_f=/tmp/fstab.$_disk
	if [[ $_disk == $ROOTDISK ]]; then
		while :; do
			echo "The auto-allocated layout for $_disk is:"
			disklabel -f $_f -p g -A $_disk | egrep "^#|^  [a-p]:"
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
	disklabel -f $_f -E $_disk
}

md_congrats() {
}

md_consoleinfo() {
}
