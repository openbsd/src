# $OpenBSD: Makefile,v 1.2 2004/05/31 13:03:59 sturm Exp $

SRC=192.0.2.0
DST=192.0.2.1
SPI=1000
EK=eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee
AK=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

IPCOMP_ENABLE!=	sysctl -n net.inet.ipcomp.enable
ESP_ENABLE!=	sysctl -n net.inet.esp.enable
AH_ENABLE!=	sysctl -n net.inet.ah.enable

.ifmake !obj && !clean && !cleandir && !depend && !regress
.INTERRUPT:
	-@${SUDO} sysctl -q net.inet.ipcomp.enable=${IPCOMP_ENABLE}
	-@${SUDO} sysctl -q net.inet.esp.enable=${ESP_ENABLE}
	-@${SUDO} sysctl -q net.inet.ah.enable=${AH_ENABLE}

.END:
	-@${SUDO} sysctl -q net.inet.ipcomp.enable=${IPCOMP_ENABLE}
	-@${SUDO} sysctl -q net.inet.esp.enable=${ESP_ENABLE}
	-@${SUDO} sysctl -q net.inet.ah.enable=${AH_ENABLE}
.endif

ipcomp:
	@${SUDO} sysctl -q net.inet.ipcomp.enable=1
	${SUDO} ipsecadm ipcomp -cpi ${SPI} -dst ${DST} -comp deflate 
	${SUDO} ipsecadm delspi -spi ${SPI} -dst ${DST} -proto ipcomp

tcpmd5:
	${SUDO} ipsecadm tcpmd5 -spi ${SPI} -src ${SRC} -dst ${DST} -key deadbeef
	${SUDO} ipsecadm delspi -spi ${SPI} -dst ${DST} -proto tcpmd5

esp:
	@${SUDO} sysctl -q net.inet.esp.enable=1
	${SUDO} ipsecadm new esp -spi ${SPI} -src ${SRC} -dst ${DST} \
		-enc aes -key ${EK} -auth sha1 -authkey ${AK}
	${SUDO} ipsecadm delspi -spi ${SPI} -dst ${DST} -proto esp

ah:
	@${SUDO} sysctl -q net.inet.ah.enable=1
	${SUDO} ipsecadm new ah -spi ${SPI} -src ${SRC} -dst ${DST} \
		-key ${AK} -auth sha1
	${SUDO} ipsecadm delspi -spi ${SPI} -dst ${DST} -proto ah

REGRESS_TARGETS=ipcomp tcpmd5 esp ah
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
