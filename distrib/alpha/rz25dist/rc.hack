#	$NetBSD: rc.hack,v 1.1.2.1 1996/06/15 04:02:44 cgd Exp $

# Hackish /etc/rc to do basic setup for a distribution rz25 image to
# make it slightly easier to use.

PATH=/sbin:/usr/sbin:/bin:/usr/bin:/alphadist
export PATH

single_user()
{
	echo "Returning to single-user mode..."
	exit 1
}

root_dev=`(sysctl -n machdep.root_device) 2> /dev/null`
#echo "root_dev is '$root_dev'"

if [ X"${root_dev}" = X'' ] || [ X"${root_dev}" = X'??' ]; then
	echo "Can't figure out root device!"
	single_user
fi

root_dev_base=`expr $root_dev : '\([a-z][a-z]*\)[0-9]*[a-z]'`
root_dev_unit=`expr $root_dev : '[a-z][a-z]*\([0-9]*\)[a-z]'`
root_dev_part=`expr $root_dev : '[a-z][a-z]*[0-9]*\([a-z]\)'`

#echo $root_dev_base
#echo $root_dev_unit
#echo $root_dev_part

usr_dev="${root_dev_base}${root_dev_unit}d"
#echo usr_dev = $usr_dev

case $root_dev_base in
cd|sd)
	;;

*)
	echo "Unexpected root device type '$root_dev_base'."
	single_user
	;;
esac

# If /tmp is already writeable, we've already been run...  punt!
if [ -w /tmp ]; then
	echo ""
	case $root_dev_base in
	cd)
		echo "Can't boot multi-user from a CD-ROM, because"
		echo "the system can't have been configured."
		echo ""
		echo "Install on a real disk!"
		echo ""
		single_user
		;;

	sd)
		echo "If you want to boot multi-user, make sure that you've"
		echo "configured the system properly, then run the"
		echo "following:"
		echo ""
		echo "        /bin/rm -f /etc/rc"
		echo "        /bin/cp -p /alphadist/rc.real /etc/rc"
		echo ""
		echo "from single-user mode then exit the single-user shell."
		echo ""
		single_user
		;;
	esac
fi

case $root_dev_base in
cd)
	echo -n "Remounting root device..."
	mount -u -o ro /dev/$root_dev /
	echo ""

	echo -n "Mounting /usr..."
	mount -o ro /dev/$usr_dev /usr
	echo ""

	echo -n "Preparing temporary file systems..."
	mount -t mfs -o -s=6144 swap /tmp
	mount -t mfs -o -s=6144 swap /var/tmp
	mkdir /var/tmp/vi.recover
	chmod -R 1777 /tmp /var/tmp
	echo ""
	;;

sd)
	echo "Checking root and /usr..."
	fsck -n /dev/r$root_dev /dev/r$usr_dev > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		echo "File system check failed!"
		single_user
	fi

	echo -n "Remounting root device..."
	mount -u /dev/$root_dev /
	echo ""

	echo -n "Mounting /usr..."
	mount /dev/$usr_dev /usr
	echo ""
	;;
esac

single_user
