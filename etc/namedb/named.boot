;	@(#)named.boot	5.1 (Berkeley) 6/30/90

; boot file for secondary name server
; Note that there should be one primary entry for each SOA record.

; NOTE: if you are not chroot'ing named, change directory to /var/named/namedb
;       OpenBSD chroot's named by default
;directory	/var/named/namedb
directory	/namedb

; type    domain		source host/file		backup file

cache     .							root.cache
primary   0.0.127.IN-ADDR.ARPA	localhost.rev
primary   0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.int. localhost.rev

; example secondary server config:
; secondary Berkeley.EDU	128.32.130.11 128.32.133.1	ucbhosts.bak
; secondary 32.128.IN-ADDR.ARPA	128.32.130.11 128.32.133.1	ucbhosts.rev.bak

; example primary server config:
; primary  Berkeley.EDU		ucbhosts
; primary  32.128.IN-ADDR.ARPA	ucbhosts.rev
