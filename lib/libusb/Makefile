#	$OpenBSD: Makefile,v 1.1 2001/09/02 17:50:40 pvalchev Exp $
#	$NetBSD: Makefile,v 1.5 1999/07/23 09:44:38 mrg Exp $

LIB=	usb
NOPIC=
NOPROFILE=
MAN=	usb.3

MLINKS=	usb.3 libusb.3 usb.3 hid_get_report_desc.3 \
	usb.3 hid_dispose_report_desc.3 \
	usb.3 hid_start_parse.3 usb.3 hid_end_parse.3 \
	usb.3 hid_get_item.3 usb.3 hid_report_size.3 usb.3 hid_locate.3 \
	usb.3 hid_usage_page.3 usb.3 hid_usage_in_page.3 usb.3 hid_init.3 \
	usb.3 hid_get_data.3 usb.3 hid_set_data.3

SRCS=	descr.c parse.c usage.c data.c

CPPFLAGS+= -I${.CURDIR}

includes:
	@cd ${.CURDIR}; cmp -s usb.h ${DESTDIR}/usr/include/usb.h || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) usb.h \
	  ${DESTDIR}/usr/include

.include <bsd.lib.mk>
