#!/bin/sh
#
# Copyright (C) 2000-2002  Internet Software Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

# $ISC: ifconfig.sh,v 1.35.2.5 2002/08/02 03:05:39 marka Exp $

#
# Set up interface aliases for bind9 system tests.
#

# If running on hp-ux, don't even try to run config.guess.
# It will try to create a temporary file in the current directory,
# which fails when running as root with the current directory
# on a NFS mounted disk.

case `uname -a` in
  *HP-UX*) sys=hpux ;;
  *) sys=`../../../config.guess` ;;
esac

case "$1" in

    start|up)
	for ns in 1 2 3 4 5
	do
		case "$sys" in
		    *-pc-solaris2.5.1)
			ifconfig lo0:$ns 10.53.0.$ns netmask 0xffffffff up
			;;
		    *-sun-solaris2.[6-7])
			ifconfig lo0:$ns 10.53.0.$ns netmask 0xffffffff up
			;;
		    *-*-solaris2.8)
    			ifconfig lo0:$ns plumb
			ifconfig lo0:$ns 10.53.0.$ns up
			;;
		    *-*-linux*)
			ifconfig lo:$ns 10.53.0.$ns up netmask 255.255.255.0
		        ;;
		    *-unknown-freebsd*)
			ifconfig lo0 10.53.0.$ns alias netmask 0xffffffff
			;;
		    *-unknown-netbsd*)
			ifconfig lo0 10.53.0.$ns alias netmask 255.255.255.0
			;;
		    *-pc-bsdi[3-4].*)
			ifconfig lo0 add 10.53.0.$ns netmask 255.255.255.0
			;;
		    *-dec-osf[4-5].*)
			ifconfig lo0 alias 10.53.0.$ns
			;;
		    *-sgi-irix6.*)
			ifconfig lo0 alias 10.53.0.$ns
			;;
		    *-*-sysv5uw[7-8]*)
			ifconfig lo0 10.53.0.$ns alias netmask 0xffffffff
			;;
		    *-ibm-aix4.*)
			ifconfig lo0 alias 10.53.0.$ns
			;;
		    hpux)
			ifconfig lo0:$ns 10.53.0.$ns up
		        ;;
		    *-sco3.2v*)
			ifconfig lo0 alias 10.53.0.$ns
			;;
		    *-darwin5*)
			ifconfig lo0 alias 10.53.0.$ns
			;;
	            *)
			echo "Don't know how to set up interface.  Giving up."
			exit 1
		esac
	done
	;;

    stop|down)
	for ns in 5 4 3 2 1
	do
		case "$sys" in
		    *-pc-solaris2.5.1)
			ifconfig lo0:$ns 0.0.0.0 down
			;;
		    *-sun-solaris2.[6-7])
			ifconfig lo0:$ns 10.53.0.$ns down
			;;
		    *-*-solaris2.8)
			ifconfig lo0:$ns 10.53.0.$ns down
			ifconfig lo0:$ns 10.53.0.$ns unplumb
			;;
		    *-*-linux*)
			ifconfig lo:$ns 10.53.0.$ns down
		        ;;
		    *-unknown-freebsd*)
			ifconfig lo0 10.53.0.$ns delete
			;;
		    *-unknown-netbsd*)
			ifconfig lo0 10.53.0.$ns delete
			;;
		    *-pc-bsdi[3-4].*)
			ifconfig lo0 remove 10.53.0.$ns
			;;
		    *-dec-osf[4-5].*)
			ifconfig lo0 -alias 10.53.0.$ns
			;;
		    *-sgi-irix6.*)
			ifconfig lo0 -alias 10.53.0.$ns
			;;
		    *-*-sysv5uw[7-8]*)
			ifconfig lo0 -alias 10.53.0.$ns
			;;
		    *-ibm-aix4.*)
			ifconfig lo0 delete 10.53.0.$ns
			;;
		    hpux)
			ifconfig lo0:$ns 10.53.0.$ns down
		        ;;
		    *-sco3.2v*)
			ifconfig lo0 -alias 10.53.0.$ns
			;;
		    *darwin5*)
			ifconfig lo0 -alias 10.53.0.$ns
			;;
	            *)
			echo "Don't know how to destroy interface.  Giving up."
			exit 1
		esac
	done

	;;

	*)
		echo "Usage: $0 { up | down }"
		exit 1
esac
