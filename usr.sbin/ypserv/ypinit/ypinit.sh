#!/bin/sh
#	$Id: ypinit.sh,v 1.2 1995/11/08 00:01:05 deraadt Exp $
#
# ypinit.sh - setup an master or slave server.
#
DOMAINNAME=/bin/domainname
HOSTNAME=/bin/hostname
YPWHICH=/usr/bin/ypwhich
YPXFR=/usr/sbin/ypxfr
YP_DIR=/var/yp

#set -xv

ERROR=USAGE				# assume usage error

if [ $# -eq 1 ]
then
	if [ $1 = "-m" ]		# ypinit -m
	then
		DOMAIN=`${DOMAINNAME}`
		SERVERTYPE=MASTER
		ERROR=
	fi
fi

if [ $# -eq 2 ]
then
	if [ $1 = "-m" ]		# ypinit -m domainname
	then
		DOMAIN=${2}
		SERVERTYPE=MASTER
		ERROR=
	fi
	if [ $1 = "-s" ]		# ypinit -s master_server
	then
		DOMAIN=`${DOMAINNAME}`
		SERVERTYPE=SLAVE
		MASTER=${2}
		ERROR=
	fi
fi

if [ $# -eq 3 ]
then
	if [ $1 = "-s" ]		# ypinit -s master_server domainname
	then
		DOMAIN=`${3}`
		SERVERTYPE=MASTER
		MASTER=${2}
		ERROR=
	fi
fi

if [ "${ERROR}" = "USAGE" ]
then
	echo "usage: ypinit -m [domainname]" 1>&2
	echo "       ypinit -s master_server [domainname]" 1>&2
	echo "" 1>&2
	echo "\
where -m is used to build a master YP server data base, and -s is used for" 1>&2
	echo "\
a slave data base.  master_server must be an existing reachable YP server." 1>&2
	exit 1
fi

# Just allow master server for now!

#if [ "${SERVERTYPE}" != "MASTER" ];
#then
#	echo "Sorry, only master server is implemented. Support for slave server" 1>&2
#	echo "needs support for map transfer which isn't implemented yet." 1>&2
#	exit 1
#fi

# Check if domainname is set, don't accept an empty domainname

if [ -z "${DOMAIN}" ]
then
	echo "The local host's domain name hasn't been set. Please set it." 1>&2
	exit 1
fi

# Check if hostname is set, don't accept an empty hostname

HOST=`${HOSTNAME}`

if [ -z "${HOST}" ]
then
	echo "The local host's name hasn't been set. Please set it." 1>&2
	exit 1
fi

# Check if the YP directory exists.

if [ ! -d ${YP_DIR} -o -f ${YP_DIR} ]
then
	echo "The directory ${YP_DIR} doesn't exist. Restore it from the distribution." 1>&2
	echo "(Or move ${YP_DIR}.no to ${YP_DIR} if YP has not been activated before." 1>&2
	exit 1

fi

#echo "Server Type: ${SERVERTYPE} Domain: ${DOMAIN} Master: ${MASTER}"

echo "Installing the YP data base will require that you answer a few questions."
echo "Questions will all be asked at the beginning of the procedure."
echo ""

if [ -d ${YP_DIR}/${DOMAIN} ]; then
	
	echo -n "Can we destroy the existing ${YP_DIR}/${DOMAIN} and its contents? [y/n: n]  "
	read KILL

	ERROR=
	case ${KILL} in
	y*)	ERROR=DELETE;;
	Y*)	ERROR=DELETE;;
	*)	ERROR=;;
	esac

	if [ -z "${ERROR}" ]
	then
		echo "OK, please clean it up by hand and start again.  Bye"
		exit 0
	fi

	if [ "${ERROR}" = "DELETE" ]
	then
		rm -r -f ${YP_DIR}/${DOMAIN}
		
		if [ $?  -ne 0 ]
		then
			echo "Can't clean up old directory ${YP_DIR}/${DOMAIN}.  Fatal error." 1>&2
			exit 1
		fi
	fi

fi

mkdir ${YP_DIR}/${DOMAIN}

if [ $?  -ne 0 ]
then
	echo "Can't make new directory ${YP_DIR}/${DOMAIN}.  Fatal error." 1>&2
	exit 1
fi

if [ "${SERVERTYPE}" = "MASTER" ];
then

	if [ ! -f ${YP_DIR}/Makefile ]
	then
		if [ ! -f ${YP_DIR}/Makefile.main ]
		then
			echo "Can't find ${YP_DIR}/Makefile.main. " 1>&2
			exit 1
		fi
		cp ${YP_DIR}/Makefile.main ${YP_DIR}/Makefile
	fi

	SUBDIR=`grep "^SUBDIR=" ${YP_DIR}/Makefile`
	
	if [ -z "${SUBDIR}" ]
	then
		echo "Can't find line starting with 'SUBDIR=' in ${YP_DIR}/Makefile. " 1>&2
		exit 1
	fi

	NEWSUBDIR="SUBDIR="
	for DIR in `echo ${SUBDIR} | cut -c8-255`
	do
		if [ ${DIR} != ${DOMAIN} ]
		then
			NEWSUBDIR="${NEWSUBDIR} ${DIR}"
		fi
	done
	NEWSUBDIR="${NEWSUBDIR} ${DOMAIN}"

	if [ -f ${YP_DIR}/Makefile.tmp ]
	then 
		rm ${YP_DIR}/Makefile.tmp
	fi

	mv ${YP_DIR}/Makefile ${YP_DIR}/Makefile.tmp
	sed -e "s/^${SUBDIR}/${NEWSUBDIR}/" ${YP_DIR}/Makefile.tmp > ${YP_DIR}/Makefile
	rm ${YP_DIR}/Makefile.tmp

	if [ ! -f ${YP_DIR}/Makefile.yp ]
	then
		echo "Can't find ${YP_DIR}/Makefile.yp. " 1>&2
		exit 1
	fi

	cp ${YP_DIR}/Makefile.yp ${YP_DIR}/${DOMAIN}/Makefile

fi

if [ "${SERVERTYPE}" = "SLAVE" ];
then
	
	for MAP in `${YPWHICH} -d ${DOMAIN} -m | cut -d\  -f1`
	do
		${YPXFR} -h ${MASTER} -c -d ${DOMAIN} ${MAP}

		if [ $?  -ne 0 ]
		then
			echo "Can't transfer map ${MAP}." 1>&2
			exit 1
		fi
	done
fi
