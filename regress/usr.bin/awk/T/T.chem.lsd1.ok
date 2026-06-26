.PS
copy "/dev/null"
	textht = 0.16; textwid = .1; cwid = 0.12
	lineht = 0.2; linewid = 0.2
Last: 0,0

# B:	benzene pointing right
B:
Last: [
	C: 0,0
	V0: (0.15,0.259808)
	V1: (0.3,4.00474e-09)
	V2: (0.15,-0.259808)
	V3: (-0.15,-0.259808)
	V4: (-0.3,-1.20142e-08)
	V5: (-0.15,0.259808)
	V6: (0.15,0.259808)
	V7: (0.3,2.00237e-08)
	line from V1 to V2
	line from V2 to V3
	line from V3 to V4
	line from V4 to V5
	line from V5 to V6
	line from V6 to V1
	circle rad 0.15 at 0,0
] with .V4.w at Last.e 

# F:	flatring pointing left put N at 5 double 3,4 with .V1 at B.V2
F:
Last: [
	C: 0,0
	V0: (-0.15,-0.259808)
	V1: (-0.3,-1.20142e-08)
	V2: (-0.15,0.259808)
	V3: (0.15,0.259808)
	V4: (0.3,2.00237e-08)
	V5: (0.15,-0.259808)
	V6: (-0.15,-0.259808)
	V7: (-0.3,-2.80332e-08)
	V4: V5; V5: V6
	line from V1 to V2 chop 0 chop 0
	line from V2 to V3 chop 0 chop 0
	line from V3 to V4 chop 0 chop 0
	line from 0.75<C,V3> to 0.75<C,V4> chop 0 chop 0
	line from V4 to V5 chop 0 chop 0.08
	V5: ellipse invis ht 0.16 wid 0.12 at V5
	N:atom("N", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) at V5
	line from V5 to V1 chop 0.08 chop 0
]   with .V1 at B.V2

# 	H below F.N
Last: H: atom("H", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .n at F.N.s

# R:	ring pointing right with .V4 at B.V6
R:
Last: [
	C: 0,0
	V0: (0.15,0.259808)
	V1: (0.3,4.00474e-09)
	V2: (0.15,-0.259808)
	V3: (-0.15,-0.259808)
	V4: (-0.3,-1.20142e-08)
	V5: (-0.15,0.259808)
	V6: (0.15,0.259808)
	V7: (0.3,2.00237e-08)
	line from V1 to V2
	line from V2 to V3
	line from V3 to V4
	line from V4 to V5
	line from V5 to V6
	line from V6 to V1
]   with .V4 at B.V6

# 	front bond right from R.V6 ; H
Last: frontbond(0.2, 90, from R.V6.e)
Last: H: atom("H", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .L.w at Last.end

# W:	ring pointing right with .V2 at R.V6 put N at 1 double 3,4
W:
Last: [
	C: 0,0
	V0: (0.15,0.259808)
	V1: (0.3,4.00474e-09)
	V2: (0.15,-0.259808)
	V3: (-0.15,-0.259808)
	V4: (-0.3,-1.20142e-08)
	V5: (-0.15,0.259808)
	V6: (0.15,0.259808)
	V7: (0.3,2.00237e-08)
	V1: ellipse invis ht 0.16 wid 0.12 at V1
	N:atom("N", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) at V1
	line from V1 to V2 chop 0.08 chop 0
	line from V2 to V3 chop 0 chop 0
	line from V3 to V4 chop 0 chop 0
	line from 0.85<C,V3> to 0.85<C,V4> chop 0 chop 0
	line from V4 to V5 chop 0 chop 0
	line from V5 to V6 chop 0 chop 0
	line from V6 to V1 chop 0 chop 0.08
]   with .V2 at R.V6

# 	bond right from W.N ; CH3
Last: bond(0.2, 90, from W.N.e)
Last: CH3: atom("CH\s-3\d3\u\s+3", 0.3, 0.16, 0.06, 0.16, 0.12, 0.015) with .L.w at Last.end

# 	back bond -60 from W.V5 ; H
Last: backbond(0.2, 300, from W.V5.nw)
Last: H: atom("H", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .R.se at Last.end

# 	bond up from W.V5 ; C
Last: bond(0.2, 0, from W.V5.n)
Last: C: atom("C", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .C.s at Last.end

# 	doublebond up from C ; O
Last: doublebond(0.2, 0, from C.C.n)
Last: O: atom("O", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .C.s at Last.end

# 	bond right from C ; N
Last: bond(0.2, 90, from C.R.e)
Last: N: atom("N", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .L.w at Last.end

# 	bond 45 from N ; C2H5
Last: bond(0.2, 45, from N.R.ne)
Last: C2H5: atom("C\s-3\d2\u\s+3H\s-3\d5\u\s+3", 0.36, 0.16, 0.06, 0.16, 0.12, 0.015) with .L.sw at Last.end

# 	bond 135 from N ; C2H5
Last: bond(0.2, 135, from N.R.se)
Last: C2H5: atom("C\s-3\d2\u\s+3H\s-3\d5\u\s+3", 0.36, 0.16, 0.06, 0.16, 0.12, 0.015) with .L.nw at Last.end
.PE
