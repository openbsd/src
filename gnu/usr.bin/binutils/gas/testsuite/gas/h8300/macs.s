	.h8300s
	.text
h8300s_mac:
	clrmac
	ldmac er0,mach
	ldmac er1,macl
	mac @er0+,@er1+
	

