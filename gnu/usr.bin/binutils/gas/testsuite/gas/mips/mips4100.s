	.text

stuff:
	.ent stuff
	dmadd16 $4,$5
	madd16 $5,$6
	hibernate
	standby
	suspend
	nop
