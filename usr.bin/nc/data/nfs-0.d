# UDP NFS null-proc call; finds active NFS listeners on port 2049.
# If you get *something* back, there's an NFS server there.

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

000	# nfs: 100003
001
0x86
0xa3

000	# version: 1
000
000
001

000	# procedure number: 0
000
000
000

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
