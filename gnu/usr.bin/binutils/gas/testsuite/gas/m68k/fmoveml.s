# Test handling of the fmoveml instruction.
	.text
	.globl	foo
foo:
	fmoveml %fpcr,%a0@
	fmoveml %fpsr,%a0@
	fmoveml %fpiar,%a0@
	fmoveml %fpcr/%fpsr,%a0@
	fmoveml	%fpcr/%fpiar,%a0@
	fmoveml	%fpsr/%fpiar,%a0@
	fmoveml	%fpcr/%fpsr/%fpiar,%a0@
	fmoveml	%fpcr,%d0
	fmoveml	%fpsr,%d0
	fmoveml	%fpiar,%d0
	fmoveml	%fpiar,%a0
	fmoveml %a0@,%fpcr
	fmoveml %a0@,%fpsr
	fmoveml %a0@,%fpiar
	fmoveml %a0@,%fpsr/%fpcr
	fmoveml	%a0@,%fpiar/%fpcr
	fmoveml	%a0@,%fpiar/%fpsr
	fmoveml	%a0@,%fpsr/%fpiar/%fpcr
	fmoveml	%d0,%fpcr
	fmoveml	%d0,%fpsr
	fmoveml	%d0,%fpiar
	fmoveml	%a0,%fpiar
	fmoveml	&1,%fpcr
	fmoveml	&1,%fpsr
	fmoveml	&1,%fpiar
	fmoveml	&1,%fpcr/%fpsr
	fmoveml	&1,%fpcr/%fpiar
	fmoveml	&1,%fpsr/%fpiar
	fmoveml	&1,%fpiar/%fpsr/%fpcr
