#!/bin/sh
#
# Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2001-2003  Internet Software Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

# $ISC: genzone.sh,v 1.3.202.4 2004/03/08 04:04:33 marka Exp $

#
# Set up a test zone
#
# Usage: genzone.sh master-server-number slave-server-number...
#
# e.g., "genzone.sh 2 3 4" means ns2 is the master and ns3, ns4
# are slaves.
#

master="$1"

cat <<EOF
\$TTL 3600

@		86400	IN SOA	ns${master} hostmaster (
					1397051952 ; "SER0"
					5
					5
					1814400
					3600 )
EOF

for n
do
	cat <<EOF
@			NS	ns${n}
ns${n}			A	10.53.0.${n}
EOF
done

cat <<\EOF

; type 1
a01			A	0.0.0.0
a02			A	255.255.255.255

; type 2
; see NS records at top of file

; type 3
; md01			MD	madname
; 			MD	.

; type 4
; mf01			MF	madname
; mf01			MF	.

; type 5
cname01			CNAME	cname-target.
cname02			CNAME	cname-target
cname03			CNAME	.

; type 6
; see SOA record at top of file

; type 7
mb01			MG	madname
mb02			MG	.

; type 8
mg01			MG	mgmname
mg02			MG	.

; type 9
mr01			MR	mrname
mr02			MR	.

; type 10
; NULL RRs are not allowed in master files per RFC1035.
;null01			NULL

; type 11
wks01			WKS	10.0.0.1 tcp telnet ftp 0 1 2
wks02			WKS	10.0.0.1 udp domain 0 1 2
wks03			WKS	10.0.0.2 tcp 65535

; type 12
ptr01			PTR	@

; type 13
hinfo01			HINFO	"Generic PC clone" "NetBSD-1.4"
hinfo02			HINFO	PC NetBSD

; type 14
minfo01			MINFO	rmailbx emailbx
minfo02			MINFO	. . 

; type 15
mx01			MX	10 mail
mx02			MX	10 .

; type 16
txt01			TXT	"foo"
txt02			TXT	"foo" "bar"
txt03			TXT	foo
txt04			TXT	foo bar
txt05			TXT	"foo bar"
txt06			TXT	"foo\032bar"
txt07			TXT	foo\032bar
txt08			TXT	"foo\010bar"
txt09			TXT	foo\010bar
txt10			TXT	foo\ bar
txt11			TXT	"\"foo\""
txt12			TXT	\"foo\"

; type 17
rp01			RP	mbox-dname txt-dname
rp02			RP	. . 

; type 18
afsdb01			AFSDB	0 hostname
afsdb02			AFSDB	65535 .

; type 19
x2501			X25	123456789
;x2502			X25	"123456789"

; type 20
isdn01			ISDN	"isdn-address"
isdn02			ISDN	"isdn-address" "subaddress"
isdn03			ISDN	isdn-address
isdn04			ISDN	isdn-address subaddress

; type 21
rt01			RT	0 intermediate-host
rt02			RT	65535 .

; type 22
nsap01			NSAP	(
	0x47.0005.80.005a00.0000.0001.e133.ffffff000161.00 )
nsap02			NSAP	(
	0x47.0005.80.005a00.0000.0001.e133.ffffff000161.00. )
;nsap03			NSAP	0x

; type 23
nsap-ptr01		NSAP-PTR foo.
nsap-ptr01		NSAP-PTR .

; type 24
;sig01			SIG	NXT 1 3 ( 3600 20000102030405
;				19961211100908 2143 foo.nil. 
;				MxFcby9k/yvedMfQgKzhH5er0Mu/vILz45I
;				kskceFGgiWCn/GxHhai6VAuHAoNUz4YoU1t
;				VfSCSqQYn6//11U6Nld80jEeC8aTrO+KKmCaY= )

; type 25
;key01			KEY	512 ( 255 1 AQMFD5raczCJHViKtLYhWGz8hMY
;				9UGRuniJDBzC7w0aRyzWZriO6i2odGWWQVucZqKV
;				sENW91IOW4vqudngPZsY3GvQ/xVA8/7pyFj6b7Esg
;				a60zyGW6LFe9r8n6paHrlG5ojqf0BaqHT+8= )

; type 26
px01			PX	65535 foo. bar.
px02			PX	65535 . .

; type 27
gpos01			GPOS    -22.6882 116.8652 250.0
gpos02			GPOS    "" "" ""

; type 29
loc01			LOC	60 9 N 24 39 E 10 20 2000 20
loc02			LOC 	60 09 00.000 N 24 39 00.000 E 10.00m 20.00m (
				  2000.00m 20.00m )

; type 30
;nxt01			NXT	a.secure.nil. ( NS SOA MX RRSIG KEY LOC NXT )
;nxt02			NXT	. NXT NSAP-PTR
;nxt03			NXT	. 1
;nxt04			NXT	. 127

; type 33
srv01			SRV 0 0 0 .
srv02			SRV 65535 65535 65535  old-slow-box

; type 35
naptr01			NAPTR   0 0 "" "" "" . 
naptr02			NAPTR   65535 65535 blurgh blorf blegh foo.
naptr02			NAPTR   65535 65535 "blurgh" "blorf" "blegh" foo.

; type 36
kx01			KX	10 kdc
kx02			KX	10 .

; type 37
cert01			CERT	65534 65535 254 ( 
				MxFcby9k/yvedMfQgKzhH5er0Mu/vILz45I
				kskceFGgiWCn/GxHhai6VAuHAoNUz4YoU1t
				VfSCSqQYn6//11U6Nld80jEeC8aTrO+KKmCaY= )
; type 38
a601			A6	0 ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff
a601			A6	64 ::ffff:ffff:ffff:ffff foo.
a601			A6	127 ::1 foo.
a601			A6	128 .

; type 39
dname01			DNAME	dname-target.
dname02			DNAME	dname-target
dname03			DNAME	.

; type 41
; OPT is a meta-type and should never occur in master files.

; type 46
rrsig01			RRSIG	NSEC 1 3 ( 3600 20000102030405
				19961211100908 2143 foo.nil. 
				MxFcby9k/yvedMfQgKzhH5er0Mu/vILz45I
				kskceFGgiWCn/GxHhai6VAuHAoNUz4YoU1t
				VfSCSqQYn6//11U6Nld80jEeC8aTrO+KKmCaY= )

; type 47
nsec01			NSEC	a.secure.nil. ( NS SOA MX RRSIG DNSKEY LOC NSEC )
nsec02			NSEC	. NSEC NSAP-PTR
nsec03			NSEC	. TYPE1
nsec04			NSEC	. TYPE127

; type 48
dnskey01		DNSKEY	512 ( 255 1 AQMFD5raczCJHViKtLYhWGz8hMY
				9UGRuniJDBzC7w0aRyzWZriO6i2odGWWQVucZqKV
				sENW91IOW4vqudngPZsY3GvQ/xVA8/7pyFj6b7Esg
				a60zyGW6LFe9r8n6paHrlG5ojqf0BaqHT+8= )

; type 249
; TKEY is a meta-type and should never occur in master files.
; The text representation is not specified in the draft.
; This example was written based on the bind9 RR parsing code.
;tkey01			TKEY	928321914 928321915 (
;				255		; algorithm
;				65535 		; mode
;				0		; error
;				3 		; key size
;				aaaa		; key data
;				3 		; other size
;				bbbb		; other data
;				)
;; A TKEY with empty "other data"
;tkey02			TKEY	928321914 928321915 (
;				255		; algorithm
;				65535 		; mode
;				0		; error
;				3 		; key size
;				aaaa		; key data
;				0 		; other size
;						; other data
;				)

; type 255
; TSIG is a meta-type and should never occur in master files.
EOF
