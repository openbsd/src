#	$OpenBSD: install.md,v 1.2 2003/09/18 00:02:42 krw Exp $
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
