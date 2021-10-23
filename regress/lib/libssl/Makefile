#	$OpenBSD: Makefile,v 1.47 2021/10/23 14:34:10 jsing Exp $

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
