#!/bin/sh
#	$OpenBSD: ndc.sh,v 1.6 1998/05/22 19:34:46 millert Exp $

USAGE='echo \
	"usage: $0 \
 (status|dumpdb|reload|stats|trace|notrace|querylog|start|stop|restart) \
	 ... \
	"; exit 1'

PATH=%DESTSBIN%:/bin:/usr/bin:/usr/ucb:$PATH
PIDFILE=%PIDDIR%/named.pid
NAMED_CMD=named

if [ -f $PIDFILE ]
then
	PID=`sed 1q $PIDFILE`
	NAMED_CMD=`tail -1 $PIDFILE`
	if kill -0 $PID >/dev/null 2>&1; then
		RUNNING=1
	else
		RUNNING=0
	fi
	PS=`%PS% $PID | tail -1 | grep $PID`
	[ `echo $PS | wc -w` -ne 0 ] || {
		if [ $RUNNING -eq 1 ]; then
			PS="named (pid $PID) can't get name list"
		else
			PS="named (pid $PID?) not running"
		fi
	}
else
	PS="named (no pid file) not running"
	RUNNING=0
fi

for ARG
do
	case $ARG in
	start|stop|restart)
		;;
	*)
		[ $RUNNING -eq 0 ] && {
			echo $PS
			exit 1
		}
	esac

	case $ARG in
	status)	echo "$PS";;
	dumpdb)	kill -INT $PID && echo Dumping Database;;
	reload)	kill -HUP $PID && echo Reloading Database;;
	stats)	kill -%IOT% $PID && echo Dumping Statistics;;
	trace)	kill -USR1 $PID && echo Trace Level Incremented;;
	notrace) kill -USR2 $PID && echo Tracing Cleared;;
	querylog|qrylog) kill -WINCH $PID && echo Query Logging Toggled;;
	start)
		[ $RUNNING -eq 1 ] && {
			echo "$0: start: named (pid $PID) already running"
			continue
		}
		rm -f $PIDFILE
		$NAMED_CMD && {
			sleep 5
			echo Name Server Started
		}
		;;
	stop)
		[ $RUNNING -eq 0 ] && {
			echo "$0: stop: named not running"
			continue
		}
		kill $PID && {
			sleep 5
			rm -f $PIDFILE
			echo Name Server Stopped
		}
		;;
	restart)
		[ $RUNNING -eq 1 ] && {
			kill $PID && sleep 5
		}
		rm -f $PIDFILE
		$NAMED_CMD && {
			sleep 5
			echo Name Server Restarted
		}
		;;
	*)	eval "$USAGE";;
	esac
done
test -z "$ARG" && eval "$USAGE"

exit 0
