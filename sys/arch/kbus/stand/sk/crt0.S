#include <machine/led.h>
#include <machine/asi.h>
#include <machine/kbus.h>
#include <machine/psl.h>
#include <machine/pmap.h>

	.text
#define __WINSIZE 8

#define DEF_TRAP(func,val)					\
	mov	val, %l3;					\
	b	func;						\
	mov	%wim, %l0;					\
	nop
	
	.globl start
	.globl _exception_table
_exception_table:
start:
reset:
	b 	real_start
	nop
	nop
	nop
DEF_TRAP (bad_except, 1)	! 1: Instruction access
DEF_TRAP (bad_except, 2)	! 2: Illegal Instruction
DEF_TRAP (bad_except, 3)	! 3: Privilegied Instruction
DEF_TRAP (bad_except, 4)	! 4: Floating-Point disabled
DEF_TRAP (win_ovf, 5)		! 5: Window overflow
DEF_TRAP (win_unf, 6)		! 6: Window undeflow
DEF_TRAP (bad_except, 7)	! 7: Memory address not aligned
DEF_TRAP (bad_except, 8)	! 8: Floating point exception
#if 1
DEF_TRAP (bad_except, 9)	! 9: data access exception
#else
DEF_TRAP (chk_data_tlbmiss, 9)	! 9: data access exception
#endif
DEF_TRAP (bad_except, 10)	! 10: Tag overflow
DEF_TRAP (bad_except, 11)	! 11: -
DEF_TRAP (bad_except, 12)	! 12: -
DEF_TRAP (bad_except, 13)	! 13: -
DEF_TRAP (bad_except, 14)	! 14: -
DEF_TRAP (bad_except, 15)	! 15: -
DEF_TRAP (bad_except, 16)	! 16: -
DEF_TRAP (interrupt, 1)		! 17: Interrupt level 1
DEF_TRAP (interrupt, 2)		! 18: Interrupt level 2
DEF_TRAP (interrupt, 3)		! 19: Interrupt level 3
DEF_TRAP (interrupt, 4)		! 20: Interrupt level 4
DEF_TRAP (interrupt, 5)		! 21: Interrupt level 5
DEF_TRAP (interrupt, 6)		! 22: Interrupt level 6
DEF_TRAP (interrupt, 7)		! 23: Interrupt level 7
DEF_TRAP (interrupt, 8)		! 24: Interrupt level 8
DEF_TRAP (interrupt, 9)		! 25: Interrupt level 9
DEF_TRAP (interrupt, 10)	! 26: Interrupt level 10
DEF_TRAP (interrupt, 11)	! 27: Interrupt level 11
DEF_TRAP (interrupt, 12)	! 28: Interrupt level 12
DEF_TRAP (interrupt, 13)	! 29: Interrupt level 13
DEF_TRAP (interrupt, 14)	! 30: Interrupt level 14
DEF_TRAP (nmi, 15)		! 31: Interrupt level 15
DEF_TRAP (bad_except, 32)	! 32: -
DEF_TRAP (bad_except, 33)	! 33: -
DEF_TRAP (bad_except, 34)	! 34: -
DEF_TRAP (bad_except, 35)	! 35: -
DEF_TRAP (bad_except, 36)	! 36: Coprocessor disabled
DEF_TRAP (bad_except, 37)	! 37: -
DEF_TRAP (bad_except, 38)	! 38: -
DEF_TRAP (bad_except, 39)	! 39: -
DEF_TRAP (bad_except, 40)	! 40: -
DEF_TRAP (bad_except, 41)	! 41: -
DEF_TRAP (bad_except, 42)	! 42: -
DEF_TRAP (bad_except, 43)	! 43: -
DEF_TRAP (bad_except, 44)	! 44: -
DEF_TRAP (bad_except, 45)	! 45: -
DEF_TRAP (bad_except, 46)	! 46: -
DEF_TRAP (bad_except, 47)	! 47: -
DEF_TRAP (bad_except, 48)	! 48: -
DEF_TRAP (bad_except, 49)	! 49: -
DEF_TRAP (bad_except, 50)	! 50: -
DEF_TRAP (bad_except, 51)	! 51: -
DEF_TRAP (bad_except, 52)	! 52: -
DEF_TRAP (bad_except, 53)	! 53: -
DEF_TRAP (bad_except, 54)	! 54: -
DEF_TRAP (bad_except, 55)	! 55: -
DEF_TRAP (bad_except, 56)	! 56: -
DEF_TRAP (bad_except, 57)	! 57: -
DEF_TRAP (bad_except, 58)	! 58: -
DEF_TRAP (bad_except, 59)	! 59: -
DEF_TRAP (bad_except, 60)	! 60: -
DEF_TRAP (bad_except, 61)	! 61: -
DEF_TRAP (bad_except, 62)	! 62: -
DEF_TRAP (bad_except, 63)	! 63: -
DEF_TRAP (bad_except, 64)	! 64: -
DEF_TRAP (bad_except, 65)	! 65: -
DEF_TRAP (bad_except, 66)	! 66: -
DEF_TRAP (bad_except, 67)	! 67: -
DEF_TRAP (bad_except, 68)	! 68: -
DEF_TRAP (bad_except, 69)	! 69: -
DEF_TRAP (bad_except, 70)	! 70: -
DEF_TRAP (bad_except, 71)	! 71: -
DEF_TRAP (bad_except, 72)	! 72: -
DEF_TRAP (bad_except, 73)	! 73: -
DEF_TRAP (bad_except, 74)	! 74: -
DEF_TRAP (bad_except, 75)	! 75: -
DEF_TRAP (bad_except, 76)	! 76: -
DEF_TRAP (bad_except, 77)	! 77: -
DEF_TRAP (bad_except, 78)	! 78: -
DEF_TRAP (bad_except, 79)	! 79: -
DEF_TRAP (bad_except, 80)	! 80: -
DEF_TRAP (bad_except, 81)	! 81: -
DEF_TRAP (bad_except, 82)	! 82: -
DEF_TRAP (bad_except, 83)	! 83: -
DEF_TRAP (bad_except, 84)	! 84: -
DEF_TRAP (bad_except, 85)	! 85: -
DEF_TRAP (bad_except, 86)	! 86: -
DEF_TRAP (bad_except, 87)	! 87: -
DEF_TRAP (bad_except, 88)	! 88: -
DEF_TRAP (bad_except, 89)	! 89: -
DEF_TRAP (bad_except, 90)	! 90: -
DEF_TRAP (bad_except, 91)	! 91: -
DEF_TRAP (bad_except, 92)	! 92: -
DEF_TRAP (bad_except, 93)	! 93: -
DEF_TRAP (bad_except, 94)	! 94: -
DEF_TRAP (bad_except, 95)	! 95: -
DEF_TRAP (bad_except, 96)	! 96: -
DEF_TRAP (bad_except, 97)	! 97: -
DEF_TRAP (bad_except, 98)	! 98: -
DEF_TRAP (bad_except, 99)	! 99: -
DEF_TRAP (bad_except, 100)	! 100: -
DEF_TRAP (bad_except, 101)	! 101: -
DEF_TRAP (bad_except, 102)	! 102: -
DEF_TRAP (bad_except, 103)	! 103: -
DEF_TRAP (bad_except, 104)	! 104: -
DEF_TRAP (bad_except, 105)	! 105: -
DEF_TRAP (bad_except, 106)	! 106: -
DEF_TRAP (bad_except, 107)	! 107: -
DEF_TRAP (bad_except, 108)	! 108: -
DEF_TRAP (bad_except, 109)	! 109: -
DEF_TRAP (bad_except, 110)	! 110: -
DEF_TRAP (bad_except, 111)	! 111: -
DEF_TRAP (bad_except, 112)	! 112: -
DEF_TRAP (bad_except, 113)	! 113: -
DEF_TRAP (bad_except, 114)	! 114: -
DEF_TRAP (bad_except, 115)	! 115: -
DEF_TRAP (bad_except, 116)	! 116: -
DEF_TRAP (bad_except, 117)	! 117: -
DEF_TRAP (bad_except, 118)	! 118: -
DEF_TRAP (bad_except, 119)	! 119: -
DEF_TRAP (bad_except, 120)	! 120: -
DEF_TRAP (bad_except, 121)	! 121: -
DEF_TRAP (bad_except, 122)	! 122: -
DEF_TRAP (bad_except, 123)	! 123: -
DEF_TRAP (bad_except, 124)	! 124: -
DEF_TRAP (bad_except, 125)	! 125: -
DEF_TRAP (bad_except, 126)	! 126: -
DEF_TRAP (bad_except, 127)	! 127: -
DEF_TRAP (bad_except, 128)	! 128: Trap instruction
DEF_TRAP (bad_except, 129)	! 129: Trap instruction
DEF_TRAP (bad_except, 130)	! 130: Trap instruction
DEF_TRAP (bad_except, 131)	! 131: Trap instruction
DEF_TRAP (bad_except, 132)	! 132: Trap instruction
DEF_TRAP (bad_except, 133)	! 133: Trap instruction
DEF_TRAP (bad_except, 134)	! 134: Trap instruction
DEF_TRAP (bad_except, 135)	! 135: Trap instruction
DEF_TRAP (bad_except, 136)	! 136: Trap instruction
DEF_TRAP (bad_except, 137)	! 137: Trap instruction
DEF_TRAP (bad_except, 138)	! 138: Trap instruction
DEF_TRAP (bad_except, 139)	! 139: Trap instruction
DEF_TRAP (bad_except, 140)	! 140: Trap instruction
DEF_TRAP (bad_except, 141)	! 141: Trap instruction
DEF_TRAP (bad_except, 142)	! 142: Trap instruction
DEF_TRAP (bad_except, 143)	! 143: Trap instruction
DEF_TRAP (bad_except, 144)	! 144: Trap instruction
DEF_TRAP (bad_except, 145)	! 145: Trap instruction
DEF_TRAP (bad_except, 146)	! 146: Trap instruction
DEF_TRAP (bad_except, 147)	! 147: Trap instruction
DEF_TRAP (bad_except, 148)	! 148: Trap instruction
DEF_TRAP (bad_except, 149)	! 149: Trap instruction
DEF_TRAP (bad_except, 150)	! 150: Trap instruction
DEF_TRAP (bad_except, 151)	! 151: Trap instruction
DEF_TRAP (bad_except, 152)	! 152: Trap instruction
DEF_TRAP (bad_except, 153)	! 153: Trap instruction
DEF_TRAP (bad_except, 154)	! 154: Trap instruction
DEF_TRAP (bad_except, 155)	! 155: Trap instruction
DEF_TRAP (bad_except, 156)	! 156: Trap instruction
DEF_TRAP (bad_except, 157)	! 157: Trap instruction
DEF_TRAP (bad_except, 158)	! 158: Trap instruction
DEF_TRAP (bad_except, 159)	! 159: Trap instruction
DEF_TRAP (bad_except, 160)	! 160: Trap instruction
DEF_TRAP (bad_except, 161)	! 161: Trap instruction
DEF_TRAP (bad_except, 162)	! 162: Trap instruction
DEF_TRAP (bad_except, 163)	! 163: Trap instruction
DEF_TRAP (bad_except, 164)	! 164: Trap instruction
DEF_TRAP (bad_except, 165)	! 165: Trap instruction
DEF_TRAP (bad_except, 166)	! 166: Trap instruction
DEF_TRAP (bad_except, 167)	! 167: Trap instruction
DEF_TRAP (bad_except, 168)	! 168: Trap instruction
DEF_TRAP (bad_except, 169)	! 169: Trap instruction
DEF_TRAP (bad_except, 170)	! 170: Trap instruction
DEF_TRAP (bad_except, 171)	! 171: Trap instruction
DEF_TRAP (bad_except, 172)	! 172: Trap instruction
DEF_TRAP (bad_except, 173)	! 173: Trap instruction
DEF_TRAP (bad_except, 174)	! 174: Trap instruction
DEF_TRAP (bad_except, 175)	! 175: Trap instruction
DEF_TRAP (bad_except, 176)	! 176: Trap instruction
DEF_TRAP (bad_except, 177)	! 177: Trap instruction
DEF_TRAP (bad_except, 178)	! 178: Trap instruction
DEF_TRAP (bad_except, 179)	! 179: Trap instruction
DEF_TRAP (bad_except, 180)	! 180: Trap instruction
DEF_TRAP (bad_except, 181)	! 181: Trap instruction
DEF_TRAP (bad_except, 182)	! 182: Trap instruction
DEF_TRAP (bad_except, 183)	! 183: Trap instruction
DEF_TRAP (bad_except, 184)	! 184: Trap instruction
DEF_TRAP (bad_except, 185)	! 185: Trap instruction
DEF_TRAP (bad_except, 186)	! 186: Trap instruction
DEF_TRAP (bad_except, 187)	! 187: Trap instruction
DEF_TRAP (bad_except, 188)	! 188: Trap instruction
DEF_TRAP (bad_except, 189)	! 189: Trap instruction
DEF_TRAP (bad_except, 190)	! 190: Trap instruction
DEF_TRAP (bad_except, 191)	! 191: Trap instruction
DEF_TRAP (bad_except, 192)	! 192: Trap instruction
DEF_TRAP (bad_except, 193)	! 193: Trap instruction
DEF_TRAP (bad_except, 194)	! 194: Trap instruction
DEF_TRAP (bad_except, 195)	! 195: Trap instruction
DEF_TRAP (bad_except, 196)	! 196: Trap instruction
DEF_TRAP (bad_except, 197)	! 197: Trap instruction
DEF_TRAP (bad_except, 198)	! 198: Trap instruction
DEF_TRAP (bad_except, 199)	! 199: Trap instruction
DEF_TRAP (bad_except, 200)	! 200: Trap instruction
DEF_TRAP (bad_except, 201)	! 201: Trap instruction
DEF_TRAP (bad_except, 202)	! 202: Trap instruction
DEF_TRAP (bad_except, 203)	! 203: Trap instruction
DEF_TRAP (bad_except, 204)	! 204: Trap instruction
DEF_TRAP (bad_except, 205)	! 205: Trap instruction
DEF_TRAP (bad_except, 206)	! 206: Trap instruction
DEF_TRAP (bad_except, 207)	! 207: Trap instruction
DEF_TRAP (bad_except, 208)	! 208: Trap instruction
DEF_TRAP (bad_except, 209)	! 209: Trap instruction
DEF_TRAP (bad_except, 210)	! 210: Trap instruction
DEF_TRAP (bad_except, 211)	! 211: Trap instruction
DEF_TRAP (bad_except, 212)	! 212: Trap instruction
DEF_TRAP (bad_except, 213)	! 213: Trap instruction
DEF_TRAP (bad_except, 214)	! 214: Trap instruction
DEF_TRAP (bad_except, 215)	! 215: Trap instruction
DEF_TRAP (bad_except, 216)	! 216: Trap instruction
DEF_TRAP (bad_except, 217)	! 217: Trap instruction
DEF_TRAP (bad_except, 218)	! 218: Trap instruction
DEF_TRAP (bad_except, 219)	! 219: Trap instruction
DEF_TRAP (bad_except, 220)	! 220: Trap instruction
DEF_TRAP (bad_except, 221)	! 221: Trap instruction
DEF_TRAP (bad_except, 222)	! 222: Trap instruction
DEF_TRAP (bad_except, 223)	! 223: Trap instruction
DEF_TRAP (bad_except, 224)	! 224: Trap instruction
DEF_TRAP (bad_except, 225)	! 225: Trap instruction
DEF_TRAP (bad_except, 226)	! 226: Trap instruction
DEF_TRAP (bad_except, 227)	! 227: Trap instruction
DEF_TRAP (bad_except, 228)	! 228: Trap instruction
DEF_TRAP (bad_except, 229)	! 229: Trap instruction
DEF_TRAP (bad_except, 230)	! 230: Trap instruction
DEF_TRAP (bad_except, 231)	! 231: Trap instruction
DEF_TRAP (bad_except, 232)	! 232: Trap instruction
DEF_TRAP (bad_except, 233)	! 233: Trap instruction
DEF_TRAP (bad_except, 234)	! 234: Trap instruction
DEF_TRAP (bad_except, 235)	! 235: Trap instruction
DEF_TRAP (bad_except, 236)	! 236: Trap instruction
DEF_TRAP (bad_except, 237)	! 237: Trap instruction
DEF_TRAP (bad_except, 238)	! 238: Trap instruction
DEF_TRAP (bad_except, 239)	! 239: Trap instruction
DEF_TRAP (bad_except, 240)	! 240: Trap instruction
DEF_TRAP (bad_except, 241)	! 241: Trap instruction
DEF_TRAP (bad_except, 242)	! 242: Trap instruction
DEF_TRAP (bad_except, 243)	! 243: Trap instruction
DEF_TRAP (bad_except, 244)	! 244: Trap instruction
DEF_TRAP (bad_except, 245)	! 245: Trap instruction
DEF_TRAP (bad_except, 246)	! 246: Trap instruction
DEF_TRAP (bad_except, 247)	! 247: Trap instruction
DEF_TRAP (bad_except, 248)	! 248: Trap instruction
DEF_TRAP (bad_except, 249)	! 249: Trap instruction
DEF_TRAP (bad_except, 250)	! 250: Trap instruction
DEF_TRAP (bad_except, 251)	! 251: Trap instruction
DEF_TRAP (bad_except, 252)	! 252: Trap instruction
DEF_TRAP (bad_except, 253)	! 253: Trap instruction
DEF_TRAP (bad_except, 254)	! 254: Trap instruction
DEF_TRAP (bad_except, 255)	! 255: Trap instruction

	.reserve trapstack, 1000 * 4, "bss", 8

	.data
	.align	4

	.globl _in_trap_handler
_in_trap_handler:
in_trap_handler:
	.word	0
	
	.globl	_debug_tbr
_debug_tbr:
	.word	0
	.globl	_debug_ret
_debug_ret:
	.word	0

	.globl	_nofault
_nofault:
	.word	0

led_xlat:
	.byte	LED_0
	.byte	LED_1
	.byte	LED_2
	.byte	LED_3
	.byte	LED_4
	.byte	LED_5
	.byte	LED_6
	.byte	LED_7
	.byte	LED_8
	.byte	LED_9
	.byte	LED_A
	.byte	LED_b
	.byte	LED_C
	.byte	LED_d
	.byte	LED_E
	.byte	LED_F

	.text
	.align 4


#if 0	
excpt_dataaccess:
	! %l3 contains the trap number. 
	! Disp it on the leds.

	! Load the led table addressin %l4
	sethi	%hi(led_xlat), %l4
	or	%l4, %lo(led_xlat), %l4

	! Convert %l3 into a led half word.
	and	%l3, 0x0f, %l5
	ldub	[%l4 + %l5], %l5
	srl	%l3, 4, %l3
	and	%l3, 0x0f, %l3
	ldub	[%l4 + %l3], %l3
	and	%l3, LED_DP, %l3
	sll	%l3, 8, %l3
	or	%l3, %l5, %l3

	! Disp it on the led
	stha	%l3, [%g0](0xc1)

	lda	[%g0](0x81), %l3
	lda	[%g0](0x82), %g0
	sethi	%hi(_nofault), %l3
	st	%g0, [%l3 + %lo(_nofault)]

	! Return to the next instruction
	jmp	%l2
	rett	%l2 + 4
#endif
! This function is called when any SPARC trap (except window overflow or
! underflow) occurs.  It makes sure that the invalid register window is still
! available before jumping into C code.  It will also restore the world if you
! return from handle_exception.

! %l1 contains %pc
! %l2 contains %npc
	.globl bad_except
bad_except:
interrupt:
	rd	%psr, %l0
#if 1
	! %l3 contains the trap number. 
	! Disp it on the leds.

	! Load the led table addressin %l4
	sethi	%hi(led_xlat), %l4
	or	%l4, %lo(led_xlat), %l4

	! Convert %l3 into a led half word.
	and	%l3, 0x0f, %l5
	ldub	[%l4 + %l5], %l5
	srl	%l3, 4, %l3
	and	%l3, 0x0f, %l3
	ldub	[%l4 + %l3], %l3
	and	%l3, LED_DP, %l3
	sll	%l3, 8, %l3
	or	%l3, %l5, %l3

	! Disp it on the led
	stha	%l3, [%g0](ASI_LED)
#endif
#if 0
	! Return
	jmp	%l2
	rett	%l2 + 4

	! Wait for ever.
1:
	b	1b
	nop
#endif

	mov	%wim, %l3

	! Read FCR and FVAR now, otherwise data access to memory is disabled.
	lda	[%g0](ASI_FCR), %l6
	lda	[%g0](ASI_FVAR), %l7

	srl	%l3, %l0, %l4		! wim >> cwp
	cmp	%l4, 1
	bne	window_fine		! Branch if not in the invalid window
	nop

! Handle window overflow
	mov	%g1, %l4
	sll	%l3, 1, %g1		! Rotate wim left
	srl	%l3, __WINSIZE-1, %l3
	or	%g1, %l3, %g1

	save	%g0, %g0, %g0		! Slip into next window
	mov	%g1, %wim		! Install the new wim

	std	%l0, [%sp + 0 * 4]	! save L & I registers
	std	%l2, [%sp + 2 * 4]
	std	%l4, [%sp + 4 * 4]
	std	%l6, [%sp + 6 * 4]

	std	%i0, [%sp + 8 * 4]
	std	%i2, [%sp + 10 * 4]
	std	%i4, [%sp + 12 * 4]
	std	%i6, [%sp + 14 * 4]

	restore				! Go back to trap window.
	mov	%l4, %g1		! Restore %g1

window_fine:
	sethi	%hi(in_trap_handler), %l4
	ld	[%lo(in_trap_handler) + %l4], %l5
	tst	%l5
	bg	recursive_trap
	inc	%l5

	set	trapstack+1000*4, %sp	! Switch to trap stack

recursive_trap:
	st	%l5, [%lo(in_trap_handler) + %l4]
	sub	%sp,(16+1+6+1+80)*4,%sp	! Make room for input & locals
 					! + hidden arg + arg spill
					! + doubleword alignment
					! + registers[72] local var

	std	%g0, [%sp + (24 + 0) * 4] ! registers[Gx]
	std	%g2, [%sp + (24 + 2) * 4]
	std	%g4, [%sp + (24 + 4) * 4]
	std	%g6, [%sp + (24 + 6) * 4]

	std	%i0, [%sp + (24 + 8) * 4] ! registers[Ox]
	std	%i2, [%sp + (24 + 10) * 4]
	std	%i4, [%sp + (24 + 12) * 4]
	std	%i6, [%sp + (24 + 14) * 4]

	mov	%y, %l4
	mov	%tbr, %l5
	st	%l4, [%sp + (24 + 64) * 4] ! Y
	st	%l0, [%sp + (24 + 65) * 4] ! PSR
	st	%l3, [%sp + (24 + 66) * 4] ! WIM
	st	%l5, [%sp + (24 + 67) * 4] ! TBR
	st	%l1, [%sp + (24 + 68) * 4] ! PC
	st	%l2, [%sp + (24 + 69) * 4] ! NPC

	or	%l0, 0xf20, %l4
	mov	%l4, %psr		! Turn on traps, disable interrupts
	nop
	nop
	nop

	set	0x1000, %l1
	btst	%l1, %l0		! FP enabled?
	be	no_fpstore
	nop

! Must save fsr first, to flush the FQ.  This may cause a deferred fp trap, so
! traps must be enabled to allow the trap handler to clean things up.

	st	%fsr, [%sp + (24 + 70) * 4]

	std	%f0, [%sp + (24 + 32) * 4]
	std	%f2, [%sp + (24 + 34) * 4]
	std	%f4, [%sp + (24 + 36) * 4]
	std	%f6, [%sp + (24 + 38) * 4]
	std	%f8, [%sp + (24 + 40) * 4]
	std	%f10, [%sp + (24 + 42) * 4]
	std	%f12, [%sp + (24 + 44) * 4]
	std	%f14, [%sp + (24 + 46) * 4]
	std	%f16, [%sp + (24 + 48) * 4]
	std	%f18, [%sp + (24 + 50) * 4]
	std	%f20, [%sp + (24 + 52) * 4]
	std	%f22, [%sp + (24 + 54) * 4]
	std	%f24, [%sp + (24 + 56) * 4]
	std	%f26, [%sp + (24 + 58) * 4]
	std	%f28, [%sp + (24 + 60) * 4]
	std	%f30, [%sp + (24 + 62) * 4]
no_fpstore:

! 	call 	_get_in_break_mode
! 	nop

	lda	[%g0](ASI_MMCR), %l4
	st	%l4, [%sp + (24 + 72) * 4]
!	Disable mmu
!	andn	%l4, 1, %l4
!	sta	%l4, [%g0](ASI_MMCR)
	st	%l6, [%sp + (24 + 73) * 4]
	st	%l7, [%sp + (24 + 74) * 4]
	lda	[%g0](ASI_PDBA), %l4
	st	%l4, [%sp + (24 + 75) * 4]
	lda	[%g0](ASI_FPAR), %l4
	st	%l4, [%sp + (24 + 76) * 4]
	lda	[%g0](ASI_FTOR), %l4
	st	%l4, [%sp + (24 + 77) * 4]
	lduha	[%g0](ASI_FES), %l4
	st	%l4, [%sp + (24 + 78) * 4]
	lda	[%g0](ASI_FTSR), %l4
	st	%l4, [%sp + (24 + 79) * 4]

	call	_handle_exception
	add	%sp, 24 * 4, %o0	! Pass address of registers

! Reload all of the registers that are not on the stack

	ld	[%sp + (24 + 1) * 4], %g1 ! registers[Gx]
	ldd	[%sp + (24 + 2) * 4], %g2
	ldd	[%sp + (24 + 4) * 4], %g4
	ldd	[%sp + (24 + 6) * 4], %g6

	ldd	[%sp + (24 + 8) * 4], %i0 ! registers[Ox]
	ldd	[%sp + (24 + 10) * 4], %i2
	ldd	[%sp + (24 + 12) * 4], %i4
	ldd	[%sp + (24 + 14) * 4], %i6

!        set	0xff00, %l2
!	ldd 	[%sp + (24 + 72) * 4], %l4
!	stda	%l4, [%l2]0x1		   ! DIA1, debug instr addr 1
					   ! DIA2, debug instr addr 2
!	inc	8, %l2
!	ldd	[%sp + (24 + 74) * 4], %l4
!        stda	%l4, [%l2]0x1		   ! DDA1, debug data addr 1
					   ! DDA2, debug data addr 2
!	inc	8, %l2
!	ldd	[%sp + (24 + 76) * 4], %l4
!	stda	%l4, [%l2]0x1		   ! DDV1, debug data value 1
					   ! DDV2, debug data val 2
!	inc	8, %l2
!	ldd	[%sp + (24 + 78) * 4], %l4
!	bset	0x200, %l4
!	stda	%l4, [%l2]0x1		   ! DCR, debug control reg 
					   ! DSR, debug control reg 


	ldd	[%sp + (24 + 64) * 4], %l0 ! Y & PSR
	ldd	[%sp + (24 + 68) * 4], %l2 ! PC & NPC

	set	0x1000, %l5
	btst	%l5, %l1		! FP enabled?
	be	no_fpreload
	nop

	ldd	[%sp + (24 + 32) * 4], %f0
	ldd	[%sp + (24 + 34) * 4], %f2
	ldd	[%sp + (24 + 36) * 4], %f4
	ldd	[%sp + (24 + 38) * 4], %f6
	ldd	[%sp + (24 + 40) * 4], %f8
	ldd	[%sp + (24 + 42) * 4], %f10
	ldd	[%sp + (24 + 44) * 4], %f12
	ldd	[%sp + (24 + 46) * 4], %f14
	ldd	[%sp + (24 + 48) * 4], %f16
	ldd	[%sp + (24 + 50) * 4], %f18
	ldd	[%sp + (24 + 52) * 4], %f20
	ldd	[%sp + (24 + 54) * 4], %f22
	ldd	[%sp + (24 + 56) * 4], %f24
	ldd	[%sp + (24 + 58) * 4], %f26
	ldd	[%sp + (24 + 60) * 4], %f28
	ldd	[%sp + (24 + 62) * 4], %f30

	ld	[%sp + (24 + 70) * 4], %fsr
no_fpreload:

	restore				! Ensure that previous window is valid
	save	%g0, %g0, %g0		!  by causing a window_underflow trap


	mov	%l1, %psr		! Make sure that traps are disabled
					! for rett
	mov	%l0, %y
	nop

	sethi	%hi(in_trap_handler), %l4
	ld	[%lo(in_trap_handler) + %l4], %l5
	dec	%l5
	st	%l5, [%lo(in_trap_handler) + %l4]

	jmpl	%l2, %g0		! Restore old PC
	rett	%l3			! Restore old nPC

	.text
	.align 4

! Register window overflow handler.  Come here when save would move us
! into the invalid window.  This routine runs with traps disabled, and
! must be careful not to touch the condition codes, as PSR is never
! restored.
!
! We are called with %l0 = wim, %l1 = pc, %l2 = npc
win_ovf:
	save	%g0, %g0, %g0
	std	%l0, [%sp + (0*8)]
	rd	%psr, %l0
	mov	1, %l1
	sll	%l1, %l0, %l0
	wr	%l0, 0, %wim
	std	%l2, [%sp + (1*8)]
	std	%l4, [%sp + (2*8)]
	std	%l6, [%sp + (3*8)]
	std	%i0, [%sp + (4*8)]
	std	%i2, [%sp + (5*8)]
	std	%i4, [%sp + (6*8)]
	std	%i6, [%sp + (7*8)]
	restore
	jmpl	%l1, %g0
	rett	%l2

! Register window underflow handler.  Come here when restore would move us
! into the invalid window.  This routine runs with traps disabled, and
! must be careful not to touch the condition codes, as PSR is never
! restored.
!
! We are called with %l0 = wim, %l1 = pc, %l2 = npc
win_unf:
	sll	%l0, 1, %l3		! Rotate wim left
	srl	%l0, __WINSIZE-1, %l0
	or	%l0, %l3, %l0

	mov	%l0, %wim		! Install the new wim

	restore				! User window
	restore				! His caller window

	ldd	[%sp + 0 * 4], %l0	! restore L & I registers
	ldd	[%sp + 2 * 4], %l2
	ldd	[%sp + 4 * 4], %l4
	ldd	[%sp + 6 * 4], %l6

	ldd	[%sp + 8 * 4], %i0
	ldd	[%sp + 10 * 4], %i2
	ldd	[%sp + 12 * 4], %i4
	ldd	[%sp + 14 * 4], %i6

	save	%g0, %g0, %g0		! Back to trap window
	save	%g0, %g0, %g0

	jmpl	%l1,  %g0
	rett	%l2

	.globl chk_data_tlbmiss
chk_data_tlbmiss:
	! Save PSR.
	rd	%psr, %l0

	! Read fault cause register.
	lda	[ %g0 ] (0x81), %l7

	! If nofault is null, go to bad_except.
	sethi	%hi(_nofault), %l6
	ld	[%l6 + %lo(_nofault)], %l5
	cmp	%l5, %g0
	be	1f
	st	%g0, [%l6 + %lo(_nofault)]

	! Test if this is a tlb miss exception.
	btst	128, %l7
	bne,a   have_data_tlbmiss
	lda 	[ %g0 ] (0x83), %l3
1:	rd	%wim, %l0
	b	interrupt
	mov	9, %l3

have_data_tlbmiss:
	! This is a tlb miss exception.
	! %l3 now contains the page directory base register.
	! Test if bit 2 is set.
	btst  2, %l3
	bne  clean_data_tlbmiss

	! Read the fault virtual address.
	lda  [ %g0 ] (0x82), %l4

	! Read the corresponding pde.
	srl  %l4, 23, %l5
	sll  %l5, 3, %l5
	ldd  [ %l3 + %l5 ], %l6

	! Store the pte into the cache.
	sta  %l7, [ %l6 ] (0xaa)

	! Read the pte corresponding to the virtual address.
	! And store it into the cache.
	srl  %l4, 11, %l5
	and  %l5, 0xffc, %l5
	ld  [ %l6 + %l5 ], %l7
	andn  %l4, 3, %l4
	sta  %l7, [ %l4 ] (0xaa)

	! Return.
	wr  %l0, %g0, %psr
	jmp  %l1
	rett  %l2
	nop 
clean_data_tlbmiss:
	andn  %l3, 2, %l3
	srl  %l4, 23, %l5
	sll  %l5, 3, %l5
	ldd  [ %l3 + %l5 ], %l6
	sta  %l7, [ %l6 ] (0xaa)
	srl  %l4, 11, %l5
	and  %l5, 0xffc, %l5
	ld  [ %l6 + %l5 ], %l7
	andn  %l4, 3, %l4
	sta  %l7, [ %l4 ] (0xaa)
	mov  1, %l5
	and  %l0, 31, %l6
	sll  %l5, %l6, %l5
	rd  %wim, %l3
	btst  %l5, %l3
	be  data_must_clean
	wr  %l0, %g0, %psr
	jmp  %l1
	rett  %l2
	nop 
data_must_clean:
	mov  %l1, %sp
	mov  %l2, %o7
	mov  %g0, %l0
	mov  %g0, %l1
	mov  %g0, %l2
	mov  %g0, %l3
	mov  %g0, %l4
	mov  %g0, %l5
	mov  %g0, %l6
	mov  %g0, %l7
	jmp  %sp
	rett  %o7
	nop 

nmi:
	set	400000, %l6
nmi2:
	lda	[%g0](ASI_BID), %l7
	deccc	%l6
	be	nmi3
	btst	BID_NMI, %l7
	bne	nmi2
	nop
	b	nmi
	nop
nmi3:
	set	(LED_t << 8) | LED_1, %l0
	stha	%l0, [%g0](ASI_LED)

	.globl _rom_reset
_rom_reset:
	/*
	 * like Startup. */

	! Disable the mmu.
	lda	[%g0] (ASI_MMCR), %o1
	andn	%o1, MMCR_ME, %o1
	sta	%o1, [%g0] (ASI_MMCR)

	! Invalidate FTLB & GTLB.
	sta	%g0, [%g0] (ASI_FGTLB_INV)	! 0x87

	! Read the fault virtual address reg.
	lda	[%g0] (ASI_FVAR), %g0

	! Read the fault physical address reg.
	lda	[%g0] (ASI_FPAR), %g0

	! Read the fault time out reg.
	lda	[%g0] (ASI_FTOR), %g0

	rd	%psr, %g3		! paranoia: make sure ...
	andn	%g3, PSR_ET, %g3	! we have traps off
	wr	%g3, 0, %psr		! so that we can fiddle safely
	nop; nop; nop

	wr	%g0, 0, %wim		! make sure we can set psr
	nop; nop; nop
	wr	%g0, PSR_S|PSR_PS|PSR_PIL, %psr	! set initial psr
	 nop; nop; nop

	wr	%g0, 2, %wim		! set initial %wim (w1 invalid)

!	set	USRSTACK - CCFSZ, %fp	! as if called from user code
!	set	estack0 - CCFSZ - 80, %sp ! via syscall(boot_me_up) or somesuch
	rd	%psr, %l0
	wr	%l0, PSR_ET, %psr
	nop; nop; nop
	
1:	call	__exit
	nop
	b	1b
	nop

! Reset entry point
real_start:
	! Disp on the led (=0)
	sethi	%hi(0xb6c0), %l1
	or	%l1, %lo(0xB6c0), %l1
	stha	%l1, [%g0](0xc1)

	! Save the return point
	sethi	%hi(_debug_ret), %l1
	st	%o7, [%l1 + %lo(_debug_ret)]

	! Just change the new TBR.
	mov	%tbr, %l0
	sethi	%hi(_debug_tbr), %l1
	st	%l0, [%l1 + %lo(_debug_tbr)]
	sethi	%hi(start), %l1
	wr	%l1, %lo(start), %tbr
	nop
	nop
	nop

	! Misc info
!	mov	%sp, %o0
	ld	[%sp + 0x5c], %o0
	ld	[%sp + 0x60], %o1
	ld	[%sp + 0x64], %o2

	! Call main
	call	_main
	nop

	! Exit
	call	__exit
	nop
	ret

	.globl	_set_ipl
_set_ipl:
	rd	%psr, %o1
	andn	%o1, 0xf00, %o1
	and	%o0, 0x0f, %o0
	sll	%o0, 8, %o0
	wr	%o0, %o1, %psr
	nop
	nop
	nop
	retl
	nop
