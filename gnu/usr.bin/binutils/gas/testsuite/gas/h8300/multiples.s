	.h8300s
	.text
h8300s_multiple:
	ldm.l @sp+,er0-er1
	ldm.l @sp+,er0-er2
	ldm.l @sp+,er0-er3
	stm.l er0-er1,@-sp
	stm.l er0-er2,@-sp
	stm.l er0-er3,@-sp

