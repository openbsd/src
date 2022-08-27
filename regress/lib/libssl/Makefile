#	$OpenBSD: Makefile,v 1.49 2022/08/27 09:23:17 jsing Exp $

SUBDIR += api
SUBDIR += asn1
SUBDIR += buffer
SUBDIR += bytestring
SUBDIR += ciphers
SUBDIR += client
SUBDIR += dtls
SUBDIR += handshake
SUBDIR += pqueue
SUBDIR += quic
SUBDIR += record
SUBDIR += record_layer
SUBDIR += server
SUBDIR += ssl
SUBDIR += tls
SUBDIR += tlsext
SUBDIR += tlslegacy
SUBDIR += key_schedule
SUBDIR += unit
SUBDIR += verify

# Things that take a long time should go below here. 
SUBDIR += openssl-ruby
SUBDIR += tlsfuzzer
SUBDIR += interop

install:

.include <bsd.subdir.mk>
