#	$OpenBSD: Makefile,v 1.39 2020/05/21 10:46:29 tb Exp $

SUBDIR += asn1
SUBDIR += buffer
SUBDIR += bytestring
SUBDIR += ciphers
SUBDIR += client
SUBDIR += handshake
SUBDIR += pqueue
SUBDIR += record
SUBDIR += record_layer
SUBDIR += server
SUBDIR += ssl
SUBDIR += tlsext
SUBDIR += key_schedule
SUBDIR += unit

# Things that take a long time should go below here. 
SUBDIR += interop
SUBDIR += tlsfuzzer

install:

.include <bsd.subdir.mk>
