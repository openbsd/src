#	$OpenBSD: Makefile,v 1.40 2020/09/21 15:13:24 beck Exp $

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
SUBDIR += tlsfuzzer
SUBDIR += interop

install:

.include <bsd.subdir.mk>
