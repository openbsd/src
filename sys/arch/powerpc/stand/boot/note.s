.section ".note"

	## Note header - these are in target order

	# length of ns.name, including NULL = 8 = strlen("PowerPC") + 1
	# but not including padding
	.long 8
	
	# Note descriptor size
	.long 20
	
	# Note type
	.long 0x1275

	# The name of the owner
	.asciz "PowerPC"
	.balign 4


	## Note descriptor - these are in BE order

	# Real-mode # 0 or -1 (true)
	.long 0
	
	# real-base
	.byte 0xff ; .byte 0xff ; .byte 0xff ; .byte 0xff
	# real-size
	.byte 0x00 ; .byte 0x00 ; .byte 0x00 ; .byte 0x00

	# virt-base
	.byte 0xff ; .byte 0xff ; .byte 0xff ; .byte 0xff
	# virt-size
	.byte 0x00 ; .byte 0x00 ; .byte 0x00 ; .byte 0x00
	
.previous
