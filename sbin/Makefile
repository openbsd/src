#	$OpenBSD: Makefile,v 1.19 1996/09/02 11:30:09 pefo Exp $
#	$NetBSD: Makefile,v 1.29 1996/05/14 17:39:21 ws Exp $

# Not ported: XNSrouted enpload scsiformat startslip
# Missing: icheck ncheck

SUBDIR=	badsect disklabel ccdconfig dmesg fastboot ifconfig init ipf ipfstat \
	ipnat kbd mknod modload modunload mount mountd ncheck_ffs nfsd nfsiod \
	nologin ping quotacheck rdisc reboot route routed savecore scsi \
	shutdown slattach swapon ttyflags umount

# support for various file systems
SUBDIR+= mount_ados
SUBDIR+= mount_cd9660
SUBDIR+= mount_fdesc
SUBDIR+= mount_ffs	newfs fsck_ffs fsdb dumpfs dump restore clri tunefs
SUBDIR+= mount_kernfs
SUBDIR+= mount_lfs	newlfs dumplfs
# mount_mfs -> newfs
SUBDIR+= mount_msdos	newfs_msdos fsck_msdos
SUBDIR+= mount_nfs
SUBDIR+= mount_null
SUBDIR+= mount_portal
SUBDIR+= mount_procfs
SUBDIR+= mount_umap
SUBDIR+= mount_union
SUBDIR+= mount_ext2fs

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+= bim config_slot edlabel fdisk pcmcia_cntrl
.elif ${MACHINE} == "i386"
SUBDIR+= config_slot
SUBDIR+= fdisk
SUBDIR+= pcmcia_cntrl
.elif ${MACHINE} == "arc"
SUBDIR+= fdisk
.elif ${MACHINE} == "pc532"
SUBDIR+= bim
.elif (${MACHINE} == "sun3") || (${MACHINE} == "atari") || (${MACHINE} == "sparc")
SUBDIR+= edlabel
.endif

.include <bsd.subdir.mk>
