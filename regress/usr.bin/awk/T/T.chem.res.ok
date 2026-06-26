.PS
copy "/dev/null"
	textht = 0.16; textwid = .1; cwid = 0.12
	lineht = 0.2; linewid = 0.2
Last: 0,0

# 	CH3O
Last: CH3O: atom("CH\s-3\d3\u\s+3O", 0.42, 0.16, 0.06, 0.16, 0.12, 0.015) with .L.w at Last.e

# 	bond 60
Last: bond(0.2, 60, from Last.R.ne )

# R1:	benzene
R1:
Last: [
	C: 0,0
	V0: (-0.259808,0.15)
	V1: (0,0.3)
	V2: (0.259808,0.15)
	V3: (0.259808,-0.15)
	V4: (8.00947e-09,-0.3)
	V5: (-0.259808,-0.15)
	V6: (-0.259808,0.15)
	V7: (-1.60189e-08,0.3)
	line from V1 to V2
	line from V2 to V3
	line from V3 to V4
	line from V4 to V5
	line from V5 to V6
	line from V6 to V1
	circle rad 0.15 at 0,0
] with .V5.sw at Last.end 

# R2:	aromatic flatring5 pointing down put N at 1 with .V3 at R1.V2
R2:
Last: [
	C: 0,0
	V0: (0.259808,-0.15)
	V1: (8.00947e-09,-0.3)
	V2: (-0.259808,-0.15)
	V3: (-0.259808,0.15)
	V4: (-1.60189e-08,0.3)
	V5: (0.259808,0.15)
	V6: (0.259808,-0.15)
	V7: (2.40284e-08,-0.3)
	V4: V5; V5: V6
	V1: ellipse invis ht 0.16 wid 0.12 at V1
	N:atom("N", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) at V1
	line from V1 to V2 chop 0.08 chop 0
	line from V2 to V3 chop 0 chop 0
	line from V3 to V4 chop 0 chop 0
	line from V4 to V5 chop 0 chop 0
	line from V5 to V1 chop 0 chop 0.08
	circle rad 0.12 at 0,0
]   with .V3 at R1.V2

# 	H below R2.V1
Last: H: atom("H", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .n at R2.V1.s

# R3:	ring put N at 3 with .V5 at R2.V5
R3:
Last: [
	C: 0,0
	V0: (-0.259808,0.15)
	V1: (0,0.3)
	V2: (0.259808,0.15)
	V3: (0.259808,-0.15)
	V4: (8.00947e-09,-0.3)
	V5: (-0.259808,-0.15)
	V6: (-0.259808,0.15)
	V7: (-1.60189e-08,0.3)
	line from V1 to V2 chop 0 chop 0
	line from V2 to V3 chop 0 chop 0.08
	V3: ellipse invis ht 0.16 wid 0.12 at V3
	N:atom("N", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) at V3
	line from V3 to V4 chop 0.08 chop 0
	line from V4 to V5 chop 0 chop 0
	line from V5 to V6 chop 0 chop 0
	line from V6 to V1 chop 0 chop 0
]   with .V5 at R2.V5

# R4:	ring put N at 1 with .V1 at R3.V3
R4:
Last: [
	C: 0,0
	V0: (-0.259808,0.15)
	V1: (0,0.3)
	V2: (0.259808,0.15)
	V3: (0.259808,-0.15)
	V4: (8.00947e-09,-0.3)
	V5: (-0.259808,-0.15)
	V6: (-0.259808,0.15)
	V7: (-1.60189e-08,0.3)
	V1: ellipse invis ht 0.16 wid 0.12 at V1
	N:atom("N", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) at V1
	line from V1 to V2 chop 0.08 chop 0
	line from V2 to V3 chop 0 chop 0
	line from V3 to V4 chop 0 chop 0
	line from V4 to V5 chop 0 chop 0
	line from V5 to V6 chop 0 chop 0
	line from V6 to V1 chop 0 chop 0.08
]   with .V1 at R3.V3

# 	back bond -120 from R4.V4 ; H
Last: backbond(0.2, 240, from R4.V4.sw)
Last: H: atom("H", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .R.ne at Last.end

# 	back bond 60 from R4.V3 ; H
Last: backbond(0.2, 60, from R4.V3.ne)
Last: H: atom("H", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .L.sw at Last.end

# R5:	ring with .V1 at R4.V3
R5:
Last: [
	C: 0,0
	V0: (-0.259808,0.15)
	V1: (0,0.3)
	V2: (0.259808,0.15)
	V3: (0.259808,-0.15)
	V4: (8.00947e-09,-0.3)
	V5: (-0.259808,-0.15)
	V6: (-0.259808,0.15)
	V7: (-1.60189e-08,0.3)
	line from V1 to V2
	line from V2 to V3
	line from V3 to V4
	line from V4 to V5
	line from V5 to V6
	line from V6 to V1
]   with .V1 at R4.V3

# 	bond -120 ; C
Last: bond(0.2, 240, from Last.V5.sw )
Last: C: atom("C", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .R.ne at Last.end

# 	doublebond down from C ; O
Last: doublebond(0.2, 180, from C.C.s)
Last: O: atom("O", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .C.n at Last.end

# 	CH3O left of C
Last: CH3O: atom("CH\s-3\d3\u\s+3O", 0.42, 0.16, 0.06, 0.16, 0.12, 0.015) with .e at C.w+(0.02,0)

# 	back bond 60 from R5.V3 ; H
Last: backbond(0.2, 60, from R5.V3.ne)
Last: H: atom("H", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .L.sw at Last.end

# 	back bond down from R5.V4 ; O
Last: backbond(0.2, 180, from R5.V4.s)
Last: O: atom("O", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .C.n at Last.end

# 	CH3 right of O
Last: CH3: atom("CH\s-3\d3\u\s+3", 0.3, 0.16, 0.06, 0.16, 0.12, 0.015) with .w at O.e-(0.02,0)

# 	bond 120 from R5.V3 ; O
Last: bond(0.2, 120, from R5.V3.se)
Last: O: atom("O", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .L.nw at Last.end

# 	bond right lenght .1 from O ; C
Last: bond(0.1, 90, from O.R.e)
Last: C: atom("C", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .L.w at Last.end

# 	double bond down ; O
Last: doublebond(0.2, 180, from Last.C.s )
Last: O: atom("O", 0.12, 0.16, 0.06, 0.16, 0.12, 0.015) with .C.n at Last.end

# 	bond right length .1 from C 
Last: bond(0.1, 90, from C.R.e)

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
] with .V4.w at Last.end 

# 	bond 30 from B ; OCH3
Last: bond(0.2, 30, from B.V6.ne)
Last: OCH3: atom("OCH\s-3\d3\u\s+3", 0.42, 0.16, 0.18, 0.16, 0.12, 0.015) with .L.sw at Last.end

# 	bond right from B ; OCH3
Last: bond(0.2, 90, from B.V1.e)
Last: OCH3: atom("OCH\s-3\d3\u\s+3", 0.42, 0.16, 0.18, 0.16, 0.12, 0.015) with .L.w at Last.end

# 	bond 150 from B ; OCH3
Last: bond(0.2, 150, from B.V2.se)
Last: OCH3: atom("OCH\s-3\d3\u\s+3", 0.42, 0.16, 0.18, 0.16, 0.12, 0.015) with .L.nw at Last.end
.PE
