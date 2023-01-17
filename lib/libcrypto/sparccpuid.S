#if defined(__SUNPRO_C) && defined(__sparcv9)
# define ABI64  /* They've said -xarch=v9 at command line */
#elif defined(__GNUC__) && defined(__arch64__)
# define ABI64  /* They've said -m64 at command line */
#endif

#ifdef ABI64
  .register	%g2,#scratch
  .register	%g3,#scratch
# define	FRAME	-192
# define	BIAS	2047
#else
# define	FRAME	-96
# define	BIAS	0
#endif

.global	_sparcv9_vis1_probe
.align	8
_sparcv9_vis1_probe:
	add	%sp,BIAS+2,%o1
	.word	0xc19a5a40	!ldda	[%o1]ASI_FP16_P,%f0
	retl
	.word	0x81b00d80	!fxor	%f0,%f0,%f0
.type	_sparcv9_vis1_probe,#function
.size	_sparcv9_vis1_probe,.-_sparcv9_vis1_probe

! Probe and instrument VIS1 instruction. Output is number of cycles it
! takes to execute rdtick and pair of VIS1 instructions. US-Tx VIS unit
! is slow (documented to be 6 cycles on T2) and the core is in-order
! single-issue, it should be possible to distinguish Tx reliably...
! Observed return values are:
!
!	UltraSPARC IIe		7
!	UltraSPARC III		7
!	UltraSPARC T1		24
!
! Numbers for T2 and SPARC64 V-VII are more than welcomed.
!
! It would be possible to detect specifically US-T1 by instrumenting
! fmul8ulx16, which is emulated on T1 and as such accounts for quite
! a lot of %tick-s, couple of thousand on Linux...
.global	_sparcv9_vis1_instrument
.align	8
_sparcv9_vis1_instrument:
	.word	0x91410000	!rd	%tick,%o0
	.word	0x81b00d80	!fxor	%f0,%f0,%f0
	.word	0x85b08d82	!fxor	%f2,%f2,%f2
	.word	0x93410000	!rd	%tick,%o1
	.word	0x81b00d80	!fxor	%f0,%f0,%f0
	.word	0x85b08d82	!fxor	%f2,%f2,%f2
	.word	0x95410000	!rd	%tick,%o2
	.word	0x81b00d80	!fxor	%f0,%f0,%f0
	.word	0x85b08d82	!fxor	%f2,%f2,%f2
	.word	0x97410000	!rd	%tick,%o3
	.word	0x81b00d80	!fxor	%f0,%f0,%f0
	.word	0x85b08d82	!fxor	%f2,%f2,%f2
	.word	0x99410000	!rd	%tick,%o4

	! calculate intervals
	sub	%o1,%o0,%o0
	sub	%o2,%o1,%o1
	sub	%o3,%o2,%o2
	sub	%o4,%o3,%o3

	! find minimum value
	cmp	%o0,%o1
	.word	0x38680002	!bgu,a	%xcc,.+8
	mov	%o1,%o0
	cmp	%o0,%o2
	.word	0x38680002	!bgu,a	%xcc,.+8
	mov	%o2,%o0
	cmp	%o0,%o3
	.word	0x38680002	!bgu,a	%xcc,.+8
	mov	%o3,%o0

	retl
	nop
.type	_sparcv9_vis1_instrument,#function
.size	_sparcv9_vis1_instrument,.-_sparcv9_vis1_instrument

.global	_sparcv9_vis2_probe
.align	8
_sparcv9_vis2_probe:
	retl
	.word	0x81b00980	!bshuffle	%f0,%f0,%f0
.type	_sparcv9_vis2_probe,#function
.size	_sparcv9_vis2_probe,.-_sparcv9_vis2_probe

.global	_sparcv9_fmadd_probe
.align	8
_sparcv9_fmadd_probe:
	.word	0x81b00d80	!fxor	%f0,%f0,%f0
	.word	0x85b08d82	!fxor	%f2,%f2,%f2
	retl
	.word	0x81b80440	!fmaddd	%f0,%f0,%f2,%f0
.type	_sparcv9_fmadd_probe,#function
.size	_sparcv9_fmadd_probe,.-_sparcv9_fmadd_probe

.section	".init",#alloc,#execinstr
	call	OPENSSL_cpuid_setup
	nop
