# $OpenBSD: pf.os,v 1.3 2003/08/27 13:55:40 avsm Exp $
# passive OS fingerprinting
# -------------------------
#
# SYN signatures. Those signatures work for SYN packets only (duh!). 
#
# (C) Copyright 2000-2003 by Michal Zalewski <lcamtuf@coredump.cx>
# (C) Copyright 2003 by Mike Frantzen <frantzen@w4g.org>
#
#  Permission to use, copy, modify, and distribute this software for any
#  purpose with or without fee is hereby granted, provided that the above
#  copyright notice and this permission notice appear in all copies.
#
#  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
#  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
#  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
#  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
#
# This fingerprint database is adapted from Michal Zalewski's p0f passive
# operating system package.
#
#
# Each line in this file specifies a single fingerprint. Please read the
# information below carefully before attempting to append any signatures
# reported as UNKNOWN to this file to avoid mistakes.
#
# We use the following set metrics for fingerprinting:
#
# - Window size (WSS) - a highly OS dependent setting used for TCP/IP
#   performance control (max. amount of data to be sent without ACK).
#   Some systems use a fixed value for initial packets. On other
#   systems, it is a multiple of MSS or MTU (MSS+40). In some rare
#   cases, the value is just arbitrary.
#
#   NEW SIGNATURE: if p0f reported a special value of 'Snn', the number
#   appears to be a multiple of MSS (MSS*nn); a special value of 'Tnn' 
#   means it is a multiple of MTU ((MSS+40)*nn). Unless you notice the
#   value of nn is not fixed (unlikely), just copy the Snn or Tnn token
#   literally. If you know this device has a simple stack and a fixed
#   MTU, you can however multiply S value by MSS, or T value by MSS+40,
#   and put it instead of Snn or Tnn.
#
#   If WSS otherwise looks like a fixed value (for example a multiple
#   of two), or if you can confirm the value is fixed, please quote
#   it literaly. If there's no apparent pattern in WSS chosen, you
#   should consider wildcarding this value.
#
# - Overall packet size - a function of all IP and TCP options and bugs.
#
#   NEW SIGNATURE: Copy this value literally.
#
# - Initial TTL - We check the actual TTL of a received packet. It can't
#   be higher than the initial TTL, and also shouldn't be dramatically
#   lower (maximum distance is defined as 40 hops). 
#
#   NEW SIGNATURE: *Never* copy TTL from a p0f-reported signature literally.
#   You need to determine the initial TTL. The best way to do it is to
#   check the documentation for a remote system, or check its settings.
#   A fairly good method is to simply round the observed TTL up to
#   32, 64, 128, or 255, but it should be noted that some obscure devices
#   might not use round TTLs (in particular, some shoddy appliances use
#   "original" initial TTL settings). If not sure, you can see how many
#   hops you're away from the remote party with traceroute or mtr.
#
# - Don't fragment flag (DF) - some modern OSes set this to implement PMTU
#   discovery. Others do not bother.
#
#   NEW SIGNATURE: Copy this value literally.
#
# - Maximum segment size (MSS) - this setting is usually link-dependent. P0f
#   uses it to determine link type of the remote host.
#
#   NEW SIGNATURE: Always wildcard this value, except for rare cases when
#   you have an appliance with a fixed value, know the system supports only
#   a very limited number of network interface types, or know the system
#   is using a value it pulled out of nowhere.  Specific unique MSS
#   can be used to tell Google crawlbots from the rest of the population.
#
# - Window scaling (WSCALE) - this feature is used to scale WSS.
#   It extends the size of a TCP/IP window to 32 bits. Some modern
#   systems implement this feature. 
#
#   NEW SIGNATURE: Observe several signatures. Initial WSCALE is often set
#   to zero or other low value. There's usually no need to wildcard this
#   parameter.
#
# - Timestamp - some systems that implement timestamps set them to
#   zero in the initial SYN. This case is detected and handled appropriately.
#
# - Selective ACK permitted - a flag set by systems that implement 
#   selective ACK functionality.
#
# - The sequence of TCP all options (MSS, window scaling, selective ACK
#   permitted, timestamp, NOP). Other than the options previously
#   discussed, p0f also checks for timestamp option (a silly
#   extension to broadcast your uptime ;-), NOP options (used for
#   header padding) and sackOK option (selective ACK feature).
#
#   NEW SIGNATURE: Copy the sequence literally.
#
# To wildcard any value (except for initial TTL or TCP options), replace
# it with '*'. You can also use a modulo operator to match any values 
# that divide by nnn - '%nnn'.
#
# Fingerprint entry format:
#
# wwww:ttt:D:ss:OOO...:OS:Version:Subtype:Details
#
# wwww     - window size (can be *, %nnn, Snn or Tnn).  The special values
#            "S" and "T" which are a multiple of MSS or a multiple of MTU
#            respectively.
# ttt      - initial TTL 
# D        - don't fragment bit (0 - not set, 1 - set)
# ss       - overall SYN packet size
# OOO      - option value and order specification (see below)
# OS       - OS genre (Linux, Solaris, Windows)
# Version  - OS Version (2.0.27 on x86, etc)
# Subtype  - OS subtype or patchlevel (SP3, lo0)
# details  - Generic OS details
#
# If OS genre starts with '*', p0f will not show distance, link type
# and timestamp data. It is useful for userland TCP/IP stacks of
# network scanners and so on, where many settings are randomized or
# bogus.
#
# If OS genre starts with @, it denotes an approximate hit for a group
# of operating systems (signature reporting still enabled in this case). 
# Use this feature at the end of this file to catch cases for which
# you don't have a precise match, but can tell it's Windows or FreeBSD
# or whatnot by looking at, say, flag layout alone.
#
# Option block description is a list of comma or space separated
# options in the order they appear in the packet:
#
# N	   - NOP option
# Wnnn	   - window scaling option, value nnn (or * or %nnn)
# Mnnn	   - maximum segment size option, value nnn (or * or %nnn)
# S	   - selective ACK OK
# T 	   - timestamp
# T0 	   - timestamp with a zero value
#
# To denote no TCP options, use a single '.'.
#
# Please report any additions to this file, or any inaccuracies or
# problems spotted, to the maintainers: lcamtuf@coredump.cx,
# frantzen@openbsd.org and bugs@openbsd.org with a tcpdump packet
# capture of the relevant SYN packet(s)
#
# WARNING WARNING WARNING
# -----------------------
#
# Do not add a system X as OS Y just because NMAP says so. It is often
# the case that X is a NAT firewall. While nmap is talking to the 
# device itself, p0f is fingerprinting the guy behind the firewall
# instead.
#
# When in doubt, use common sense, don't add something that looks like
# a completely different system as Linux or FreeBSD or LinkSys router.
# Check DNS name, establish a connection to the remote host and look
# at SYN+ACK - does it look similar?
#
# Some users tweak their TCP/IP settings - enable or disable RFC1323
# functionality, enable or disable timestamps or selective ACK,
# disable PMTU discovery, change MTU and so on. Always compare a new rule
# to other fingerprints for this system, and verify the system isn't
# "customized" before adding it. It is OK to add signature variants
# caused by a commonly used software (personal firewalls, security
# packages, etc), but it makes no sense to try to add every single
# possible /proc/sys/net/ipv4 tweak on Linux or so.
#
# KEEP IN MIND: Some packet firewalls configured to normalize outgoing
# traffic (OpenBSD pf with "scrub" enabled, for example) will, well,
# normalize packets. Signatures will not correspond to the originating
# system (and probably not quite to the firewall either).
#
# NOTE: Try to keep this file in some reasonable order, from most to
# least likely systems. This will speed up operation. Also keep most
# generic and broad rules near the end.
#

##########################
# Standard OS signatures #
##########################

# ----------------- Linux -------------------

512:64:0:44:M*:			Linux:2.0:3x:Linux 2.0.3x
16384:64:0:44:M*:		Linux:2.0:3x:Linux 2.0.3x

5440:64:1:60:M1360,S,T,N,W0:	Linux:google::Linux (Google crawlbot)

S3:64:1:60:M*,S,T,N,W0:		Linux:2.4:18-21:Linux 2.4.18 and newer
S4:64:1:60:M*,S,T,N,W0:		Linux:2.4::Linux 2.4
S3:64:1:60:M*,S,T,N,W1:		Linux:2.5::Linux 2.5 (newer)
S4:64:1:60:M*,S,T,N,W1:		Linux:2.5::Linux 2.5

# That's quite stupid, but happens. The WSS is a multiplier of
# MSS, but not that MSS - the default ethernet MSS instead ;-)
5840:64:1:60:M*,S,T,N,W0:	Linux:2.4::Linux 2.4 (NAT or snafu)

S20:64:1:60:M*,S,T,N,W0:	Linux:2.2:20-25:Linux 2.2.20 and newer
S22:64:1:60:M*,S,T,N,W0:	Linux:2.2::Linux 2.2

# This happens only over loopback:
# 32767:64:1:60:M*,S,T,N,W0:Linux:2.4 (local)
# S8:64:1:60:M*,S,T,N,W0:Linux:2.2 (local)
# Some fairly common mods:
# S4:64:1:52:M*,N,N,S,N,W0:	Linux:2.4:ts:Linux 2.4 w/o timestamps

# ----------------- FreeBSD -----------------
# 4.6 - 5.0 is a bit of a guesswork at the moment.
# Need more data before the final release.

16384:64:1:44:M*:		FreeBSD:2.0-2.2::FreeBSD 2.0-4.1
16384:64:1:44:M*:		FreeBSD:3.0-3.5::FreeBSD 2.0-4.1
16384:64:1:44:M*:		FreeBSD:4.0-4.1::FreeBSD 2.0-4.1

1024:64:1:60:M*,N,W0,N,N,T:	FreeBSD:4.4::FreeBSD 4.4
16384:64:1:60:M*,N,W0,N,N,T:	FreeBSD:4.4::FreeBSD 4.4

S:64:1:60:M*,N,W0,N,N,T:	FreeBSD:4.6::FreeBSD 4.6

57344:64:1:44:M*:		FreeBSD:4.6-4.8:noRFC1323:FreeBSD 4.6-4.8 (no RFC1323)
57344:64:1:60:M*,N,W0,N,N,T:	FreeBSD:4.6-4.8::FreeBSD 4.6-4.8

65535:64:1:60:M*,N,W0,N,N,T:	FreeBSD:4.8-4.9::FreeBSD 4.8-5.0
65535:64:1:60:M*,N,W0,N,N,T:	FreeBSD:5.0::FreeBSD 4.8-5.0
32768:64:1:60:M*,N,W0,N,N,T:	FreeBSD:4.8-4.9::FreeBSD 4.8-5.0 (or MacOS X)
32768:64:1:60:M*,N,W0,N,N,T:	FreeBSD:5.0::FreeBSD 4.8-5.0 (or MacOS X)

65535:48:1:60:M*,N,W1,N,N,T:	FreeBSD:5.1::FreeBSD 5.1

# ----------------- NetBSD ------------------

16384:64:0:60:M*,N,W0,N,N,T0:	NetBSD:1.6::NetBSD 1.6
16384:64:1:60:M*,N,W0,N,N,T0:	NetBSD:1.6:df:NetBSD 1.6 (DF)
16384:64:0:60:M*,N,W0,N,N,T:	NetBSD:1.3::NetBSD 1.3 (or OpenBSD 2.6)

# ----------------- OpenBSD -----------------

16384:64:0:60:M*,N,W0,N,N,T:		OpenBSD:2.6::NetBSD 1.3 (or OpenBSD 2.6)
16384:64:1:64:M*,N,N,S,N,W0,N,N,T:	OpenBSD:3.0-3.4::OpenBSD 3.0-3.4
16384:64:0:64:M*,N,N,S,N,W0,N,N,T:	OpenBSD:3.0-3.4:no-df:OpenBSD 3.0-3.4 (scrub no-df)
57344:64:1:64:M*,N,N,S,N,W0,N,N,T:	OpenBSD:3.3-3.4::OpenBSD 3.3-3.4
57344:64:0:64:M*,N,N,S,N,W0,N,N,T:	OpenBSD:3.3-3.4:no-df:OpenBSD 3.3-3.4 (scrub no-df)

# ----------------- Solaris -----------------
# Splitting 8/9 into two cases, we'll see if there
# are any complaints...

S17:64:1:64:N,W3,N,N,T0,N,N,S,M*:	Solaris:8:RFC1323:Solaris 8 RFC1323
S17:64:1:48:N,N,S,M*:			Solaris:8::Solaris 8
S34:64:1:48:M1460,N,N,S:		Solaris:9::Solaris 9

S17:255:1:44:M*:			Solaris:2.5-2.7::Solaris 2.5 to 7
S6:255:1:44:M*:				Solaris:2.6::Solaris 2.6

# ----------------- IRIX --------------------

61440:64:0:44:M*:			IRIX:6.2-6.5::IRIX 6.2-6.5
49152:64:0:52:M*,N,W2,N,N,S:		IRIX:6.5:RFC1323:IRIX 6.5 (RFC1323)
61440:64:0:48:M*,N,N,S:			IRIX:6.5:14m:IRIX 6.5.14m
49152:64:0:48:M*,N,N,S:			IRIX:6.5:19:IRIX 6.5.19

# ----------------- Tru64 -------------------

32768:64:1:48:M*,N,W0:			Tru64:4.0f::Tru64 4.0f
61440:64:0:48:M*,N,W0:			Tru64:5.1a:JP4:Tru64 v5.1a JP4

# ----------------- OpenVMS -----------------

6144:64:1:60:M*,N,W0,N,N,T:		OpenVMS:7.2::OpenVMS 7.2 (Multinet 4.4 stack)

# ----------------- AIX ---------------------

32768:64:0:60:M*,N,W0,N,N,T:		AIX:4.3:3:AIX 4.3.3

# ----------------- MacOS -------------------

32768:255:1:48:M*,W0,N:			MacOS:9.1-9.2::MacOS 9.1/9.2

# ----------------- Windows -----------------

S44:64:1:48:N,N,S,M*:			Windows:98:SE:Windows 98SE
8192:128:1:48:M*,N,N,S:			Windows:98::Windows 98
8192:128:1:44:M*:			Windows:NT::Windows old NT (?)

%8192:128:1:48:M*,N,N,S:		Windows:XP::Windows XP/2000
%8192:128:1:48:M*,N,N,S:		Windows:2000P::Windows XP/2000
65535:128:1:48:M*,N,N,S:		Windows:2000:SP4:Windows 2000 SP4
S44:128:1:48:M*,N,N,S:			Windows:XP::Windows XP or 2000 SP3
S44:128:1:48:M*,N,N,S:			Windows:2000:SP3:Windows XP or 2000 SP3
S6:128:1:48:M*,N,N,S:			Windows:XP::Windows XP or 2000 SP3
S6:128:1:48:M*,N,N,S:			Windows:2000:SP3:Windows XP or 2000 SP3

S45:128:1:48:M*,N,N,S:			@Windows:XP::Windows XP
S46:128:1:48:M*,N,N,S:			@Windows:XP::Windows XP

# The same stuff w/o DF - happens quite often:
%8192:128:0:48:M*,N,N,S:		Windows:XP::Windows XP/2000
%8192:128:0:48:M*,N,N,S:		Windows:2000P::Windows XP/2000
65535:128:0:48:M*,N,N,S:		Windows:2000:SP4:Windows 2000 SP4
S44:128:0:48:M*,N,N,S:			Windows:XP::Windows XP or 2000 SP3
S44:128:0:48:M*,N,N,S:			Windows:2000:SP3:Windows XP or 2000 SP3
S6:128:0:48:M*,N,N,S:			Windows:XP::Windows XP or 2000 SP3
S6:128:0:48:M*,N,N,S:			Windows:2000:SP3:Windows XP or 2000 SP3

S45:128:0:48:M*,N,N,S:			@Windows:XP:firewalled:Windows XP (firewalled)
S46:128:0:48:M*,N,N,S:			@Windows:XP:firewalled:Windows XP (firewalled)

# I'm not sure what this is, but one report suggests NT. 'll see...
32767:128:1:52:M*,N,W0,N,N,S:		Windows:NT:4:Windows NT4
6144:128:1:52:M*,N,W0,N,N,S:		Windows:NT:4:Windows NT4
S45:128:1:52:M*,N,W0,N,N,S:		Windows:NT:4:Windows NT4

*:128:1:64:M*,N,W2,N,N,T0,N,N,S:	Windows:2000:SP4:Windows 2000 SP4 (RFC1323) or PalmPC

# Odds and ends...
58944:64:1:52:M*,N,W2,N,N,S:		Windows:XP:SP2:Windows XP SP2 IPv6 System Mechanic tuned

# ----------------- HP/UX -------------------

32768:64:1:44:M1460:			HP-UX:B.10.20::HP/UX B.10.20
32768:64:0:48:M1448,W0,N:		HP-UX:11.0::HP/UX 11.0
0:64:0:48:M1460,W0,N:			HP-UX:B.11.00::HP/UX B.11.0 A (RFC1323)


# ----------------- SCO ------------------
S17:64:1:44:M1460:			SCO:Unixware:7.0:SCO Unixware 7.0.0 or OpenServer 5.0.4-5.06
S17:64:1:44:M1460:			SCO:OpenServer:5.0:SCO Unixware 7.0.0 or OpenServer 5.0.4-5.06

# ----------------- RiscOS ------------------

# We don't yet support the ?12 TCP option
#16384:64:1:68:M1460,N,W0,N,N,T,N,N,?12:	RISCOS:3.70::RISC OS 3.70

# ----------------- BSD/OS ------------------

8192:64:1:60:M1460,N,W0,N,N,T:		BSD/OS:3.1::BSD/OS 3.1-4.3
8192:64:1:60:M1460,N,W0,N,N,T:		BSD/OS:4.0-4.3::BSD/OS 3.1-4.3


################################
# Appliance / other signatures #
################################

# ---------- Firewalls / routers ------------

S12:64:1:44:M1460:			@Checkpoint:::Checkpoint (rnknown 1)
S12:64:1:48:N,N,S,M1460:		@Checkpoint:::Checkpoint (unknown 2)

# ------- Switches and other stuff ----------

4128:255:0:44:M*:			Cisco:::Cisco Catalyst 3500, 7500 etc

# ---------- Caches and whatnots ------------

5840:64:1:52:M1460,N,N,S,N,W0:		AOL:web cache::AOL web cache

32850:64:1:64:N,W1,N,N,T,N,N,S,M*:	NetApp:5.x::NetApp Data OnTap 5.x
16384:64:1:64:M1460,N,N,S,N,W0,N:	NetApp:5.3:1:NetApp NetCache 5.3.1
65535:64:0:60:M1460,N,W0,N,N,T:		NetApp:CacheFlow::NetApp CacheFlow
8192:64:1:64:M1460,N,N,S,N,W0,N,N,T:	NetApp:5.2:1:NetApp NetCache 5.2.1

5840:64:0:48:M1460,N,N,S:		Cisco:Content Engine::Cisco Content Engine

27085:128:0:40:.:			Dell:PowerApp cache::Dell PowerApp (Linux-based)

60352:128:1:64:M1460,N,W2,N,N,T,N,N,S:	Alteon:ACEswitch::Alteon ACEswitch

65535:255:1:48:N,W1,M1460:		Inktomi:crawler::Inktomi crawler

16384:255:0:40:.:			Proxyblocker:::Proxyblocker (what's this?)

# ----------- Embedded systems --------------

S9:255:0:44:M536:			PalmOS:Tungsten:C:PalmOS Tungsten C(Win95 based)
S5:255:0:44:M536:			PalmOS:3::PalmOS 3 (Win95 based)


####################
# Fancy signatures #
####################

1024:64:0:40:.:				*NMAP:syn scan:1:NMAP syn scan (1)
2048:64:0:40:.:				*NMAP:syn scan:2:NMAP syn scan (2)
3072:64:0:40:.:				*NMAP:syn scan:3:NMAP syn scan (3)
4096:64:0:40:.:				*NMAP:syn scan:4:NMAP syn scan (4)

1024:64:0:60:W10,N,M265,T:		*NMAP:OS:1:NMAP OS detection probe (1)
2048:64:0:60:W10,N,M265,T:		*NMAP:OS:2:NMAP OS detection probe (2)
3072:64:0:60:W10,N,M265,T:		*NMAP:OS:3:NMAP OS detection probe (3)
4096:64:0:60:W10,N,M265,T:		*NMAP:OS:4:NMAP OS detection probe (4)

#####################################
# Generic signatures - just in case #
#####################################

#*:64:1:60:M*,N,W*,N,N,T:		@FreeBSD:4.0-4.9::FreeBSD 4.x/5.x
#*:64:1:60:M*,N,W*,N,N,T:		@FreeBSD:5.0-5.1::FreeBSD 4.x/5.x

