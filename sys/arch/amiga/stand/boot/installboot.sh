#!/bin/sh
#	$OpenBSD: installboot.sh,v 1.1 1997/01/16 09:26:34 niklas Exp $
#	$NetBSD: installboot.sh,v 1.1.1.1 1996/11/29 23:36:29 is Exp $

# compatibility with old installboot program
#
#	@(#)installboot.sh	8.1 (Berkeley) 6/10/93
#
if [ $# != 2 ]
then
	echo "Usage: installboot bootprog device"
	exit 1
fi
if [ ! -f $1 ]
then
	echo "Usage: installboot bootprog device"
	echo "${1}: bootprog must be a regular file"
	exit 1
fi
if [ ! -c $2 ]
then
	echo "Usage: installboot bootprog device"
	echo "${2}: device must be a char special file"
	exit 1
fi
#/sbin/disklabel -B -b $1 $2
dd if=$1 of=$2 bs=512 count=16
exit $?
