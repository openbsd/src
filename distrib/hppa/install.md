#	$OpenBSD: install.md,v 1.5 2004/08/19 02:03:09 mickey Exp $
#
# machine dependent section of installation/upgrade script.
#

MDTERM=vt100
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

	if grep "disk label corrupted" /tmp/checkfordisklabel; then
		rval=2
	fi >/dev/null 2>&1

	rm -f /tmp/checkfordisklabel
	return $rval
}

md_prep_disklabel() {
	local _disk=$1

	md_checkfordisklabel $_disk
	case $? in
	2)	echo "WARNING: Label on disk $_disk is corrupted. You will be repairing it.\n"
		;;
	esac

	disklabel -W $_disk >/dev/null 2>&1
	disklabel -f /tmp/fstab.$_disk -E $_disk
}

md_congrats() {
}
