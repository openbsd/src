#	$OpenBSD: Makefile,v 1.6 1996/04/29 13:07:00 hvozda Exp $
#	$NetBSD: Makefile,v 1.28 1996/04/05 01:44:24 cgd Exp $
#	@(#)Makefile	8.5 (Berkeley) 3/31/94

# Not ported: XNSrouted enpload scsiformat startslip
# Missing: icheck ncheck

SUBDIR=	badsect disklabel ccdconfig dmesg fastboot ifconfig init ipf \
	ipfstat kbd mknod modload modunload mount mountd nfsd nfsiod nologin \
	ping quotacheck reboot route routed savecore shutdown slattach \
	swapon ttyflags umount

# support for various file systems
SUBDIR+= mount_ados
SUBDIR+= mount_cd9660
SUBDIR+= mount_fdesc
SUBDIR+= mount_ffs	newfs fsck_ffs fsdb dumpfs dump restore clri tunefs
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
SUBDIR+= bim config_slot edlabel fdisk pcmcia_cntrl
.elif ${MACHINE} == "i386"
SUBDIR+= config_slot
SUBDIR+= fdisk
SUBDIR+= pcmcia_cntrl
.elif ${MACHINE} == "pc532"
SUBDIR+= bim
.elif ${MACHINE} == "sun3"
SUBDIR+= edlabel
.endif

.include <bsd.subdir.mk>
