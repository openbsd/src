.TL
OpenSSH Channel Close Protocol 1.5 Implementation
.SH
Channel Input State Diagram
.PS
reset
l=1
s=1.2
ellipsewid=s*ellipsewid
boxwid=s*boxwid
ellipseht=s*ellipseht
S1: ellipse "INPUT" "OPEN"
move right 2*l from last ellipse.e
S4: ellipse "INPUT" "CLOSED"
move down l from last ellipse.s
S3: ellipse "INPUT" "WAIT" "OCLOSED"
move down l from 1st ellipse.s
S2: ellipse "INPUT" "WAIT" "DRAIN"
arrow "" "rcvd OCLOSE/" "shutdown_read" "send IEOF" from S1.e to S4.w
arrow "ibuf_empty/" "send IEOF" from S2.e to S3.w
arrow from S1.s to S2.n
box invis "read_failed/" "shutdown_read" with .e at last arrow.c
arrow  from S3.n to S4.s
box invis "rcvd OCLOSE/" "-" with .w at last arrow.c
ellipse wid .9*ellipsewid ht .9*ellipseht at S4
arrow "start" "" from S1.w+(-0.5,0) to S1.w
.PE
.SH
Channel Output State Diagram
.PS
S1: ellipse "OUTPUT" "OPEN"
move right 2*l from last ellipse.e
S3: ellipse "OUTPUT" "WAIT" "IEOF"
move down l from last ellipse.s
S4: ellipse "OUTPUT" "CLOSED"
move down l from 1st ellipse.s
S2: ellipse "OUTPUT" "WAIT" "DRAIN"
arrow "" "write_failed/" "shutdown_write" "send OCLOSE" from S1.e to S3.w
arrow "obuf_empty ||" "write_failed/" "shutdown_write" "send OCLOSE" from S2.e to S4.w
arrow from S1.s to S2.n
box invis "rcvd IEOF/" "-" with .e at last arrow.c
arrow from S3.s to S4.n
box invis "rcvd IEOF/" "-" with .w at last arrow.c
ellipse wid .9*ellipsewid ht .9*ellipseht at S4
arrow "start" "" from S1.w+(-0.5,0) to S1.w
.PE
