#	$OpenBSD: Makefile,v 1.2 2002/09/06 00:23:12 brad Exp $
#	$NetBSD: Makefile,v 1.5 1999/07/23 09:44:38 mrg Exp $

LIB=	usbhid
MAN=	usbhid.3

MLINKS=	usbhid.3 hid_dispose_report_desc.3 \
	usbhid.3 hid_end_parse.3 \
	usbhid.3 hid_get_data.3 \
	usbhid.3 hid_get_item.3 \
	usbhid.3 hid_get_report_desc.3 \
	usbhid.3 hid_init.3 \
	usbhid.3 hid_locate.3 \
	usbhid.3 hid_parse_usage_in_page.3 \
	usbhid.3 hid_parse_usage_page.3 \
	usbhid.3 hid_report_size.3 \
	usbhid.3 hid_set_data.3 \
	usbhid.3 hid_start_parse.3 \
	usbhid.3 hid_usage_in_page.3 \
	usbhid.3 hid_usage_page.3 \
	usbhid.3 hid_use_report_desc.3

SRCS=	descr.c parse.c usage.c data.c

CPPFLAGS+= -I${.CURDIR}

includes:
	@cd ${.CURDIR}; cmp -s usbhid.h ${DESTDIR}/usr/include/usbhid.h || \
	  ${INSTALL} ${INSTALL_COPY} -m 444 -o $(BINOWN) -g $(BINGRP) usbhid.h \
	  ${DESTDIR}/usr/include

.include <bsd.lib.mk>
