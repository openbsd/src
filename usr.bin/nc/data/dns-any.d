# dns "any for ." query, to udp 53
# if tcp: precede with 2 bytes of len:
# 0
# 17
# you should get at least *one* record back out

# HEADER:
0	# query id = 2
2

1	# flags/opcodes = query, dorecurse
0

0	# qdcount, i.e. nqueries: 1
1

0	# ancount: answers, 0
0

0	# nscount: 0
0

0	# addl records: 0
0

# end of fixed header

0	# name-len: 0 for ".", lenbyte plus name-bytes otherwise

0	# type: any, 255
0xff

0	# class: IN
1

# i think that's it..
