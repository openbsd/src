#	$OpenBSD: install.md,v 1.3 2003/09/21 02:11:42 krw Exp $
#
# machine dependent section of installation/upgrade script.
#

MDTERM=vt100
MDDISKDEVS='/^sd[0-9] /s/ .*//p'
MDCDDEVS='/^cd[0-9] /s/ .*//p'
ARCH=ARCH

md_set_term() {
}

md_installboot() {
	echo -n "Installing boot block..."
	/sbin/disklabel -B $1
	echo "done."
}

# $1 is the disk to check
md_checkfordisklabel() {
	local rval=0

	disklabel $1 >/dev/null 2>/tmp/checkfordisklabel

	if grep "no disk label" /tmp/checkfordisklabel; then
		rval=1
	elif grep "disk label corrupted" /tmp/checkfordisklabel; then
		rval=2
	fi >/dev/null 2>&1

	rm -f /tmp/checkfordisklabel
	return $rval
}

md_prep_disklabel()
{
	local _disk=$1

	md_checkfordisklabel $_disk
	case $? in
	0)	;;
	1)	echo WARNING: Disk $_disk has no label. You will be creating a new one.
		echo
		;;
	2)	echo WARNING: Label on disk $_disk is corrupted. You will be repairing.
		echo
		;;
	esac

	disklabel -W ${_disk}
	disklabel -f /tmp/fstab.${_disk} -E ${_disk}
}

md_congrats() {
}
