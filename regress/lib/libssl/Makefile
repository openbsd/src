#	$OpenBSD: Makefile,v 1.45 2021/08/30 17:34:01 tb Exp $

SUBDIR += asn1
SUBDIR += buffer
SUBDIR += bytestring
SUBDIR += ciphers
SUBDIR += client
SUBDIR += dtls
SUBDIR += handshake
SUBDIR += pqueue
SUBDIR += record
SUBDIR += record_layer
SUBDIR += server
SUBDIR += ssl
SUBDIR += tlsext
SUBDIR += tlslegacy
SUBDIR += key_schedule
SUBDIR += unit

# Things that take a long time should go below here. 
SUBDIR += openssl-ruby
SUBDIR += tlsfuzzer
SUBDIR += interop

install:

.include <bsd.subdir.mk>
