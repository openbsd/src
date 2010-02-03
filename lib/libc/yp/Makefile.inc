#	$OpenBSD: Makefile.inc,v 1.7 2010/02/03 20:49:00 miod Exp $

# yp sources
.PATH: ${LIBCSRCDIR}/arch/${MACHINE_CPU}/yp ${LIBCSRCDIR}/yp

SRCS+=	ypmatch_cache.c yp_bind.c yp_get_default_domain.c \
	yp_first.c yp_all.c yp_order.c \
	yp_master.c yp_maplist.c yperr_string.c ypprot_err.c _yp_check.c \
	ypexclude.c \
	xdr_domainname.c xdr_keydat.c xdr_mapname.c xdr_peername.c \
	xdr_valdat.c xdr_ypbind_binding.c xdr_ypbind_resp.c \
	xdr_ypbind_resptype.c xdr_ypbind_setdom.c xdr_ypmaplist.c \
	xdr_ypreq_key.c xdr_ypreq_nokey.c xdr_ypresp_all.c \
	xdr_ypresp_key_val.c xdr_ypresp_maplist.c xdr_ypresp_master.c \
	xdr_ypresp_order.c xdr_ypresp_val.c xdr_ypstat.c
MAN+=	ypclnt.3

MLINKS+=ypclnt.3 yp_all.3 ypclnt.3 yp_bind.3 ypclnt.3 yp_first.3 \
	ypclnt.3 yp_get_default_domain.3 ypclnt.3 yp_maplist.3 \
	ypclnt.3 yp_master.3 ypclnt.3 yp_match.3 ypclnt.3 yp_next.3 \
	ypclnt.3 yp_order.3 ypclnt.3 yp_unbind.3 \
	ypclnt.3 yperr_string.3 ypclnt.3 ypprot_err.3
