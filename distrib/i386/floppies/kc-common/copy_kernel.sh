#	$Id: copy_kernel.sh,v 1.2 1996/02/02 07:24:33 dm Exp $
#
#	Kernel copy script

DEFAULT_PARTITON=sd0a
MOUNT_POINT=/mnt
KERNEL_NAME=/bsd
#TEST=testfn

testfn() {
	echo	$*
	sleep	5
}

cancel() {
	echo	""
	echo	"Copy cancelled."
	exit 1
}

umountfs() {
	echo	"Unmounting filesystem; please wait."
	trap 2 3
	${TEST} umount ${MOUNT_POINT}
	case $? in
	0)
		;;
	*)
		echo	"Warning: Unmount of ${MOUNT_POINT} failed."
		;;
	esac	
}

warning() {
	echo	""
	echo	"Copy failed or was interrupted."
	echo	"Warning: Copied kernel my be corrupted!"
}

trap "cancel;" 2 3
echo	"NetBSD kernel copy program"
echo	""
echo	"Default answers are displayed in brackets.  You may hit Control-C"
echo	"at any time to cancel this operation (though if you hit Control-C at"
echo	"a prompt, you need to hit return for it to be noticed)."

echo	""
echo	"What disk partition should the kernel be installed on?"
echo	"(For example, \"sd0a\", \"wd0a\", etc.)"
echo	""
echo -n	"Partition? [${DEFAULT_PARTITON}] "
read diskpart
if [ "X${diskpart}" = "X" ]; then
	diskpart=${DEFAULT_PARTITON}
fi
rawdiskpart="r${diskpart}"

echo	""
echo -n	"Are you sure you want to copy a new kernel to ${diskpart}? [n] "
read reply
case ${reply} in
y*|Y*)
	;;
*)
	cancel
	;;
esac

echo	""
echo	"Checking ${diskpart} partition; please wait."
${TEST} fsck -p "/dev/${rawdiskpart}"
case $? in
0)
	;;
*)
	echo	"File system check failed or aborted!"
	cancel
	;;
esac

echo	"Mounting /dev/${diskpart} on ${MOUNT_POINT}."
trap "echo ''; umountfs; cancel;" 2 3
${TEST} mount "/dev/${diskpart}" ${MOUNT_POINT}
case $? in
0)
	;;
*)
	echo	"Mount failed!"
	cancel
	;;
esac

echo	"Copying kernel to ${MOUNT_POINT}."
trap "warning; umountfs; cancel;" 2 3
${TEST} cp ${KERNEL_NAME} ${MOUNT_POINT}
case $? in
0)
	;;
*)
	warning
	umountfs
	cancel
	;;
esac

umountfs

echo	""
echo	"Copy completed."
echo	""
echo	"Use \"halt\" to halt the system, then (when the system is halted)"
echo	"eject the floppy disk and hit any key to reboot from the hard disk."
exit 0
