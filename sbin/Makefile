#	$NetBSD: Makefile,v 1.25 1995/10/10 21:42:00 gwr Exp $
#	@(#)Makefile	8.5 (Berkeley) 3/31/94

# Not ported: XNSrouted enpload scsiformat startslip
# Missing: icheck ncheck

SUBDIR=	badsect disklabel ccdconfig dmesg fastboot ifconfig init mknod \
	modload modunload mount mountd nfsd nfsiod nologin ping quotacheck \
	reboot route routed savecore shutdown slattach swapon ttyflags umount

# support for various file systems
SUBDIR+= mount_ados
SUBDIR+= mount_cd9660
SUBDIR+= mount_fdesc
SUBDIR+= mount_ffs	newfs fsck fsdb dumpfs dump restore clri tunefs
SUBDIR+= mount_kernfs
SUBDIR+= mount_lfs	newlfs dumplfs
# mount_mfs -> newfs
SUBDIR+= mount_msdos
SUBDIR+= mount_nfs
SUBDIR+= mount_null
SUBDIR+= mount_portal
SUBDIR+= mount_procfs
SUBDIR+= mount_umap
SUBDIR+= mount_union

.if   make(clean) || make(cleandir)
SUBDIR+= bim edlabel fdisk
.elif ${MACHINE} == "i386"
SUBDIR+= fdisk
.elif ${MACHINE} == "pc532"
SUBDIR+= bim
.elif ${MACHINE} == "sun3"
SUBDIR+= edlabel
.endif

.include <bsd.subdir.mk>
