#!/bin/sh
#	$NetBSD: hack.sh,v 1.2 1995/03/23 08:31:30 cgd Exp $
HACKDIR=/usr/games/lib/hackdir
HACK=$HACKDIR/hack
MAXNROFPLAYERS=4

cd $HACKDIR
case $1 in
	-s*)
		exec $HACK $@
		;;
	*)
		exec $HACK $@ $MAXNROFPLAYERS
		;;
esac
