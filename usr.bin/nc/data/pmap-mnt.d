# portmap request for mountd [or whatever; see where prog=MOUNT]
# send to UDP 111 and hope it's not a logging portmapper!
# split into longwords, since rpc apparently only deals with them

001 # 0x01 # .	# XID: 4 trash bytes
002 # 0x02 # .
003 # 0x03 # .
004 # 0x04 # .

000 # 0x00 # .	# MSG: int 0=call, 1=reply
000 # 0x00 # .
000 # 0x00 # .
000 # 0x00 # .

000 # 0x00 # .	# pmap call body: rpc version=2
000 # 0x00 # .
000 # 0x00 # .
002 # 0x02 # .

000 # 0x00 # .	# pmap call body: prog=PMAP, 100000
001 # 0x01 # .
134 # 0x86 # .
160 # 0xa0 # .

000 # 0x00 # .	# pmap call body: progversion=2
000 # 0x00 # .
000 # 0x00 # .
002 # 0x02 # .

000 # 0x00 # .	# pmap call body: proc=GETPORT, 3
000 # 0x00 # .
000 # 0x00 # .
003 # 0x03 # .

# with AUTH_NONE, there are 4 zero integers [16 bytes] here

000 # 0x00 # .	# auth junk: cb_cred: auth_unix = 1; NONE = 0
000 # 0x00 # .
000 # 0x00 # .
000 # 0x00 # .

000 # 0x00 # .	# auth junk
000 # 0x00 # .
000 # 0x00 # .
000 # 0x00 # .

000 # 0x00 # .	# auth junk
000 # 0x00 # .
000 # 0x00 # .
000 # 0x00 # .

000 # 0x00 # .	# auth junk
000 # 0x00 # .
000 # 0x00 # .
000 # 0x00 # .

000 # 0x00 # .	# prog=MOUNT, 100005
001 # 0x01 # .
134 # 0x86 # .
165 # 0xa5 # .

000 # 0x00 # .	# progversion=1
000 # 0x00 # .
000 # 0x00 # .
001 # 0x01 # .

000 # 0x00 # .	# protocol=udp, 17
000 # 0x00 # .
000 # 0x00 # .
017 # 0x11 # .

000 # 0x00 # .	# proc num = junk
000 # 0x00 # .
000 # 0x00 # .
000 # 0x00 # .

# The reply you get back contains your XID, int 1 if "accepted", and
# mountd's port number at the end or 0 if not registered.
