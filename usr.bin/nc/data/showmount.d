# UDP mountd call.  Use as input to find mount daemons and avoid portmap.
# Useful proc numbers are 2, 5, and 6.
# UDP-scan around between 600-800 to find most mount daemons.
# Using this with "2", plugged into "nc -u -v -w 2 victim X-Y" will
# directly scan *and* dump the current exports when mountd is hit.
# combine stdout *and* stderr thru "strings" or something to clean it up

000	# XID: 4 trash bytes
001
002
003

000	# CALL: 0
000
000
000

000	# RPC version: 2
000
000
002

000	# mount: 100005
001
0x86
0xa5

000	# mount version: 1
000
000
001

000	# procedure number -- put what you need here:
000	#	2 = dump  [showmount -e]
000	#	5 = exportlist [showmount -a]
xxx	# "sed s/xxx/$1/ | data -g | nc ..."  or some such...

000	# port: junk
000
000
000

000	# auth trash
000
000
000

000	# auth trash
000
000
000

000	# auth trash
000
000
000

000	# extra auth trash?  probably not needed
000
000
000

# that's it!
