#	$OpenBSD: Makefile,v 1.50 2000/04/03 01:10:05 mickey Exp $

# Not ported: XNSrouted enpload scsiformat startslip
# Missing: icheck

SUBDIR= atactl badsect brconfig ccdconfig disklabel dmesg fsck ifconfig init \
	ipf ipfstat ipnat ipsecadm isakmpd kbd mknod modload modunload \
	mount mountd ncheck_ffs nfsd nfsiod nologin photurisd ping \
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

# IPv6
SUBDIR+= ping6 rtsol

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+= bim edlabel fdisk ancontrol wicontrol
.elif ${MACHINE} == "i386"
SUBDIR+= fdisk
SUBDIR+= ancontrol wicontrol
.elif (${MACHINE} == "arc") || (${MACHINE} == "powerpc") || (${MACHINE} == "alpha")
SUBDIR+= fdisk
.elif ${MACHINE} == "pc532"
SUBDIR+= bim
.elif (${MACHINE} == "sun3") || (${MACHINE} == "atari") || \
      (${MACHINE} == "sparc")
SUBDIR+= edlabel
.endif

.include <bsd.subdir.mk>
