PATH=/sbin:/usr/sbin:/bin:/usr/bin
export PATH
HOME=/root
export HOME

if [ -x /usr/bin/tset ]; then
	eval `/usr/bin/tset -sQ \?$TERM`
fi
