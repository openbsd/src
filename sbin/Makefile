#	$OpenBSD: Makefile,v 1.3 1996/03/21 00:15:18 niklas Exp $
#	$NetBSD: Makefile,v 1.26 1996/03/16 10:29:36 leo Exp $
#	@(#)Makefile	8.5 (Berkeley) 3/31/94

# Not ported: XNSrouted enpload scsiformat startslip
# Missing: icheck ncheck

SUBDIR=	badsect disklabel ccdconfig dmesg fastboot ifconfig init ipf \
	ipfstat mknod modload modunload mount mountd nfsd nfsiod nologin \
	ping quotacheck reboot route routed shutdown slattach swapon \
	ttyflags umount

# XXX Temporarely until all ports are able to use savecore (leo)
.if	(${MACHINE} == "atari")
SUBDIR+=        savecore
.else
SUBDIR+=        savecore.old
.endif

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
