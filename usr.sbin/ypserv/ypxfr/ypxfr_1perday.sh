#!/bin/sh
#	$OpenBSD: ypxfr_1perday.sh,v 1.1 1997/04/20 10:08:36 maja Exp $
#
# ypxfr_1perday.sh - YP maps to be updated daily
#

/usr/sbin/ypxfr group.byname
/usr/sbin/ypxfr group.bygid 
/usr/sbin/ypxfr protocols.byname
/usr/sbin/ypxfr protocols.bynumber
/usr/sbin/ypxfr networks.byname
/usr/sbin/ypxfr networks.byaddr
/usr/sbin/ypxfr services.byname
/usr/sbin/ypxfr rpc.bynumber
/usr/sbin/ypxfr ypservers
/usr/sbin/ypxfr amd.home
