#	$OpenBSD: files.adb,v 1.1 2006/01/18 23:21:17 miod Exp $

file	dev/adb/adb_subr.c		adb

device	akbd: wskbddev
attach	akbd at adb
file	dev/adb/akbd.c			akbd needs-flag

device	ams: wsmousedev
attach	ams at adb
file	dev/adb/ams.c			ams needs-flag
