LIB=	form
SRCS=	fld_def.c fld_stat.c fld_type.c fld_user.c frm_data.c frm_def.c \
	frm_driver.c frm_hook.c frm_opts.c frm_user.c frm_win.c fty_alnum.c \
	fty_alpha.c fty_enum.c fty_int.c fty_num.c fty_regex.c
HDRS=	form.h
CFLAGS+=-I. -I${.CURDIR} -I${.CURDIR}/../libmenu
MAN=	form.3 form_field_userptr.3 form_cursor.3 form_field_validation.3 \
	form_data.3 form_fieldtype.3 form_driver.3 form_hook.3 form_field.3 \
	form_new.3 form_field_attributes.3 form_new_page.3 form_field_buffer.3 \
	form_opts.3 form_field_info.3 form_page.3 form_field_just.3 \
	form_post.3 form_field_new.3 form_userptr.3 form_field_opts.3 \
	form_win.3

includes:
	@cd ${.CURDIR}; for i in ${HDRS}; do \
	  cmp -s $$i ${DESTDIR}/usr/include/$$i || \
	  ${INSTALL} ${COPY} -m 444 -o $(BINOWN) -g $(BINGRP) $$i \
	  ${DESTDIR}/usr/include; done

.include <bsd.lib.mk>
