#	$OpenBSD: Makefile,v 1.70 2005/02/21 13:33:09 damien Exp $

# Not ported: XNSrouted enpload scsiformat startslip
# Missing: icheck

SUBDIR= atactl badsect brconfig ccdconfig disklabel dmesg dhclient \
	fsck growfs ifconfig init iopctl ipsecadm isakmpd kbd lmccontrol \
	mknod modload modunload mount mountd ncheck_ffs nfsd nologin \
	pfctl pflogd ping quotacheck raidctl reboot route routed \
	savecore scan_ffs scsi shutdown slattach swapctl sysctl \
	ttyflags umount

# support for various file systems
SUBDIR+= mount_ados
SUBDIR+= mount_cd9660
SUBDIR+= mount_fdesc
SUBDIR+= mount_ffs	newfs fsck_ffs fsdb dumpfs dump restore clri tunefs fsirand
SUBDIR+= mount_kernfs
# mount_mfs -> newfs
SUBDIR+= mount_msdos	newfs_msdos fsck_msdos
SUBDIR+= mount_nfs
SUBDIR+= mount_ntfs
SUBDIR+= mount_null
SUBDIR+= mount_portal
SUBDIR+= mount_procfs
SUBDIR+= mount_umap
SUBDIR+= mount_union
SUBDIR+= mount_ext2fs fsck_ext2fs
SUBDIR+= mount_xfs

# IPv6
SUBDIR+= ping6 rtsol

# Man pages (and manpages to do)
SUBDIR+= fdisk pdisk ancontrol wicontrol wsconsctl

.include <bsd.subdir.mk>
