#	$OpenBSD: Makefile,v 1.55 2000/10/13 16:42:50 chris Exp $

# Not ported: XNSrouted enpload scsiformat startslip
# Missing: icheck

SUBDIR= atactl badsect brconfig ccdconfig disklabel dmesg fsck ifconfig init \
	ipf ipfstat ipnat ipsecadm isakmpd kbd lmccontrol mknod modload \
	modunload mount mountd ncheck_ffs nfsd nologin photurisd ping \
	quotacheck raidctl reboot route routed savecore scan_ffs scsi \
	shutdown slattach startkey swapctl sysctl ttyflags umount

# support for various file systems
SUBDIR+= mount_ados
SUBDIR+= mount_cd9660
SUBDIR+= mount_fdesc
SUBDIR+= mount_ffs	newfs fsck_ffs fsdb dumpfs dump restore clri tunefs fsirand
SUBDIR+= mount_kernfs
# mount_mfs -> newfs
SUBDIR+= mount_msdos	newfs_msdos fsck_msdos
SUBDIR+= mount_nfs
SUBDIR+= mount_null
SUBDIR+= mount_portal
SUBDIR+= mount_procfs
SUBDIR+= mount_umap
SUBDIR+= mount_union
SUBDIR+= mount_ext2fs fsck_ext2fs
SUBDIR+= mount_xfs
SUBDIR+= mount_tcfs

# IPv6
SUBDIR+= ping6 rtsol

# Man pages (and manpages to do)
SUBDIR+= fdisk ancontrol wicontrol edlabel wsconsctl

# Add bim if we resurrect pc532

.include <bsd.subdir.mk>
