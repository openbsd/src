#	$OpenBSD: Makefile,v 1.76 2005/12/06 20:18:57 pedro Exp $

# Not ported: XNSrouted enpload scsiformat startslip
# Missing: icheck

SUBDIR= atactl badsect bioctl brconfig ccdconfig disklabel dmesg dhclient \
	fsck growfs ifconfig init iopctl ipsecadm ipsecctl isakmpd kbd \
	lmccontrol mknod modload modunload mount mountd ncheck_ffs nfsd \
	nologin pfctl pflogd ping quotacheck raidctl reboot route routed \
	savecore scan_ffs scsi shutdown slattach swapctl sysctl \
	ttyflags umount

# support for various file systems
SUBDIR+= mount_ados
SUBDIR+= mount_cd9660
SUBDIR+= mount_ffs	newfs fsck_ffs fsdb dumpfs dump restore clri tunefs fsirand
# mount_mfs -> newfs
SUBDIR+= mount_msdos	newfs_msdos fsck_msdos
SUBDIR+= mount_nfs
SUBDIR+= mount_ntfs
SUBDIR+= mount_portal
SUBDIR+= mount_procfs
SUBDIR+= mount_udf
SUBDIR+= mount_ext2fs fsck_ext2fs
SUBDIR+= mount_xfs

# IPv6
SUBDIR+= ping6 rtsol

# Man pages (and manpages to do)
SUBDIR+= fdisk pdisk ancontrol wicontrol wsconsctl

.include <bsd.subdir.mk>
