/*	$OpenBSD: defaults.h,v 1.1.1.1 1997/09/15 06:01:53 downsj Exp $	*/
/*
 * Header file defaults.h - assorted default values for character strings in
 * the volume descriptor.
 *
 * 	$From: defaults.h,v 1.4 1997/04/10 03:31:53 eric Rel $
 */

#define  PREPARER_DEFAULT 	NULL
#define  PUBLISHER_DEFAULT	NULL
#define  APPID_DEFAULT 		NULL
#define  COPYRIGHT_DEFAULT 	NULL
#define  BIBLIO_DEFAULT 	NULL
#define  ABSTRACT_DEFAULT 	NULL
#define  VOLSET_ID_DEFAULT 	NULL
#define  VOLUME_ID_DEFAULT 	"CDROM"
#define  BOOT_CATALOG_DEFAULT   "boot.catalog"
#define  BOOT_IMAGE_DEFAULT     NULL
#ifdef __QNX__
#define  SYSTEM_ID_DEFAULT 	"QNX"
#endif

#ifdef __osf__
#define  SYSTEM_ID_DEFAULT 	"OSF"
#endif

#ifdef __sun
#define  SYSTEM_ID_DEFAULT 	"Solaris"
#endif

#ifdef __hpux
#define  SYSTEM_ID_DEFAULT 	"HP-UX"
#endif

#ifdef __sgi
#define  SYSTEM_ID_DEFAULT 	"SGI"
#endif

#ifdef _AIX
#define  SYSTEM_ID_DEFAULT 	"AIX"
#endif

#ifndef SYSTEM_ID_DEFAULT
#define  SYSTEM_ID_DEFAULT 	"LINUX"
#endif
