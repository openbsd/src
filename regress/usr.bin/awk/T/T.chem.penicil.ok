.so /usr/bwk/talks/vg.mac
.vg
.ft R
.PS
copy "/dev/null"
	textht = 0.16; textwid = .1; cwid = 0.12
	lineht = 0.2; linewid = 0.2
Last: 0,0

# R1:	ring4 pointing 45 put N at 2 
R1:
Last: [
	C: 0,0
	V0: (-0.21,0.21)
	V1: (0.21,0.21)
	V2: (0.21,-0.21)
	V3: (-0.21,-0.21)
	V4: (-0.21,0.21)
	V5: (0.21,0.21)
	line from V1 to V2 chop 0 chop 0.112
	V2: ellipse invis ht 0.224 wid 0.168 at V2
	N:atom("N", 0.168, 0.224, 0.084, 0.224, 0.168, 0.021) at V2
	line from V2 to V3 chop 0.112 chop 0
	line from V3 to V4 chop 0 chop 0
	line from V4 to V1 chop 0 chop 0
] with .V3.w at Last.e 

# 	doublebond -135 from R1.V3 ; O
Last: doublebond(0.28, 225, from R1.V3.sw)
Last: O: atom("O", 0.168, 0.224, 0.084, 0.224, 0.168, 0.021) with .R.ne at Last.end

# 	backbond up from R1.V1 ; H
Last: backbond(0.28, 0, from R1.V1.n)
Last: H: atom("H", 0.168, 0.224, 0.084, 0.224, 0.168, 0.021) with .C.s at Last.end

# 	frontbond -45 from R1.V4 ; N
Last: frontbond(0.28, 315, from R1.V4.nw)
Last: N: atom("N", 0.168, 0.224, 0.084, 0.224, 0.168, 0.021) with .R.se at Last.end

# 	H above N
Last: H: atom("H", 0.168, 0.224, 0.084, 0.224, 0.168, 0.021) with .s at N.n

# 	bond left from N ; C
Last: bond(0.28, 270, from N.L.w)
Last: C: atom("C", 0.168, 0.224, 0.084, 0.224, 0.168, 0.021) with .R.e at Last.end

# 	doublebond up ; O
Last: doublebond(0.28, 0, from Last.C.n )
Last: O: atom("O", 0.168, 0.224, 0.084, 0.224, 0.168, 0.021) with .C.s at Last.end

# 	bond length .1 left from C ; CH2
Last: bond(0.1, 270, from C.L.w)
Last: CH2: atom("CH\s-3\d2\u\s+3", 0.42, 0.224, 0.084, 0.224, 0.168, 0.021) with .R.e at Last.end

# 	bond length .1 left
Last: bond(0.1, 270, from Last.L.w )

# 	benzene pointing left
Last: [
	C: 0,0
	V0: (-0.21,-0.363731)
	V1: (-0.42,-1.68199e-08)
	V2: (-0.21,0.363731)
	V3: (0.21,0.363731)
	V4: (0.42,2.80332e-08)
	V5: (0.21,-0.363731)
	V6: (-0.21,-0.363731)
	V7: (-0.42,-3.92464e-08)
	line from V1 to V2
	line from V2 to V3
	line from V3 to V4
	line from V4 to V5
	line from V5 to V6
	line from V6 to V1
	circle rad 0.21 at 0,0
] with .V4.e at Last.end 

# R2:	flatring5 put S at 1 put N at 4 with .V5 at R1.V1
R2:
Last: [
	C: 0,0
	V0: (-0.363731,0.21)
	V1: (0,0.42)
	V2: (0.363731,0.21)
	V3: (0.363731,-0.21)
	V4: (1.12133e-08,-0.42)
	V5: (-0.363731,-0.21)
	V6: (-0.363731,0.21)
	V7: (-2.24265e-08,0.42)
	V4: V5; V5: V6
	V1: ellipse invis ht 0.224 wid 0.168 at V1
	S:atom("S", 0.168, 0.224, 0.084, 0.224, 0.168, 0.021) at V1
	line from V1 to V2 chop 0.112 chop 0
	line from V2 to V3 chop 0 chop 0
	line from V3 to V4 chop 0 chop 0.112
	V4: ellipse invis ht 0.224 wid 0.168 at V4
	N:atom("N", 0.168, 0.224, 0.084, 0.224, 0.168, 0.021) at V4
	line from V4 to V5 chop 0.112 chop 0
	line from V5 to V1 chop 0 chop 0.112
]   with .V5 at R1.V1

# 	bond 20 from R2.V2 ; CH3
Last: bond(0.28, 20, from R2.V2.n)
Last: CH3: atom("CH\s-3\d3\u\s+3", 0.42, 0.224, 0.084, 0.224, 0.168, 0.021) with .L.s at Last.end

# 	bond 90 from R2.V2 ; CH3
Last: bond(0.28, 90, from R2.V2.e)
Last: CH3: atom("CH\s-3\d3\u\s+3", 0.42, 0.224, 0.084, 0.224, 0.168, 0.021) with .L.w at Last.end

# 	bond 90 from R2.V3 ; H
Last: bond(0.28, 90, from R2.V3.e)
Last: H: atom("H", 0.168, 0.224, 0.084, 0.224, 0.168, 0.021) with .L.w at Last.end

# 	backbond 170 from R2.V3 ; COOH
Last: backbond(0.28, 170, from R2.V3.s)
Last: COOH: atom("COOH", 0.672, 0.224, 0.084, 0.224, 0.168, 0.021) with .L.n at Last.end
.PE
.CW
	# this is the structure of penicillin G, an antibiotic
R1:	ring4 pointing 45 put N at 2 
	doublebond -135 from R1.V3 ; O
	backbond up from R1.V1 ; H
	frontbond -45 from R1.V4 ; N
	H above N
	bond left from N ; C
	doublebond up ; O
	bond length .1 left from C ; CH2
	bond length .1 left
	benzene pointing left
R2:	flatring5 put S at 1 put N at 4 with .V5 at R1.V1
	bond 20 from R2.V2 ; CH3
	bond 90 from R2.V2 ; CH3
	bond 90 from R2.V3 ; H
	backbond 170 from R2.V3 ; COOH
