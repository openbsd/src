#! /bin/sh
#	$OpenBSD: testsuite.sh,v 1.1 2003/09/01 19:45:28 miod Exp $

# Simple test program to check what happens when userland tries to trap.
# Written by Miodrag Vallat 2003 AD -- public domain

PROG=./trap

ulimit -c 0

echo -n "Privilege violation: "
${PROG} 0
echo -n "Privilege violation: "
${PROG} 1
echo -n "Privilege violation: "
${PROG} 127
echo -n "Alternate system call: "
${PROG} 128
echo -n "Alternate system call: "
${PROG} 129
echo -n "DDB breakpoint: "
${PROG} 130
echo -n "DDB trace: "
${PROG} 131
echo -n "DDB entry: "
${PROG} 132
#echo -n "SIGSYS: "
#${PROG} 255
#echo -n "SIGSYS: "
#${PROG} 256
#echo -n "SIGSYS: "
#${PROG} 400
echo -n "System call: "
${PROG} 450
#echo -n "SIGSYS: "
#${PROG} 495
echo -n "BUG: "
${PROG} 496
# since there won't be any output...
echo
#echo -n "SIGTRAP: "
#${PROG} 502
echo -n "Software divide by zero: "
${PROG} 503
echo -n "Single step: "
${PROG} 504
echo -n "Breakpoint: "
${PROG} 511

exit 0
