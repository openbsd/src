/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	OpenBSD: giodevs,v 1.4 2012/07/18 20:10:12 miod Exp 
 */

/*	$NetBSD: giodevs,v 1.8 2007/02/19 04:46:33 rumble Exp $	*/

/* devices with 8-bit identification registers */

struct gio_knowndev {
	int productid;
	const char *product;
};

struct gio_knowndev gio_knowndevs[] = {
	{ GIO_PRODUCT_XPI, "XPI low cost FDDI" },
	{ GIO_PRODUCT_GTR, "GTR TokenRing" },
	{ GIO_PRODUCT_ISDN, "Synchronous ISDN" },
	{ GIO_PRODUCT_CANON, "Canon Interface" },
	{ GIO_PRODUCT_JPEG_D, "JPEG (Double Wide)" },
	{ GIO_PRODUCT_JPEG_S, "JPEG (Single Wide)" },
	{ GIO_PRODUCT_XPI_M0, "XPI mez. FDDI device 0" },
	{ GIO_PRODUCT_XPI_M1, "XPI mez. FDDI device 1" },
	{ GIO_PRODUCT_EP, "E-Plex 8-port Ethernet" },
	{ GIO_PRODUCT_IVAS, "Lyon Lamb IVAS" },
	{ GIO_PRODUCT_SETENG_GFE, "Set Engineering GFE 10/100 Ethernet" },
	{ GIO_PRODUCT_FORE_ATM, "FORE Systems GIA-200 ATM" },
	{ GIO_PRODUCT_GIOCLC, "Cyclone Colorbus" },
	{ GIO_PRODUCT_ATM, "ATM board" },
	{ GIO_PRODUCT_SCSI, "16 bit SCSI Card" },
	{ GIO_PRODUCT_SMPTE, "SMPTE 259M Video" },
	{ GIO_PRODUCT_BABBLE, "Babblefish Compression" },
	{ GIO_PRODUCT_IMPACT, "Impact" },
	{ GIO_PRODUCT_PHOBOS_G160, "Phobos G160 10/100 Ethernet" },
	{ GIO_PRODUCT_PHOBOS_G130, "Phobos G130 10/100 Ethernet" },
	{ GIO_PRODUCT_PHOBOS_G100, "Phobos G100 100baseTX Fast Ethernet" },
	{ 0, NULL }
};
