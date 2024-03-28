#!/usr/bin/env perl

$flavour = shift;
$output  = shift;
if ($flavour =~ /\./) { $output = $flavour; undef $flavour; }

$0 =~ m/(.*[\/\\])[^\/\\]+$/; $dir=$1;
( $xlate="${dir}x86_64-xlate.pl" and -f $xlate ) or
( $xlate="${dir}perlasm/x86_64-xlate.pl" and -f $xlate) or
die "can't locate x86_64-xlate.pl";

open OUT,"| \"$^X\" $xlate $flavour $output";
*STDOUT=*OUT;

($arg1,$arg2,$arg3,$arg4)=("%rdi","%rsi","%rdx","%rcx");	# Unix order

print<<___;
.text
.globl	OPENSSL_ia32_cpuid
.type	OPENSSL_ia32_cpuid,\@abi-omnipotent
.align	16
OPENSSL_ia32_cpuid:
	_CET_ENDBR
	mov	%rbx,%r8		# save %rbx

	xor	%eax,%eax
	cpuid
	mov	%eax,%r11d		# max value for standard query level

	xor	%eax,%eax
	cmp	\$0x756e6547,%ebx	# "Genu"
	setne	%al
	mov	%eax,%r9d
	cmp	\$0x49656e69,%edx	# "ineI"
	setne	%al
	or	%eax,%r9d
	cmp	\$0x6c65746e,%ecx	# "ntel"
	setne	%al
	or	%eax,%r9d		# 0 indicates Intel CPU
	jz	.Lintel

	cmp	\$0x68747541,%ebx	# "Auth"
	setne	%al
	mov	%eax,%r10d
	cmp	\$0x69746E65,%edx	# "enti"
	setne	%al
	or	%eax,%r10d
	cmp	\$0x444D4163,%ecx	# "cAMD"
	setne	%al
	or	%eax,%r10d		# 0 indicates AMD CPU
	jnz	.Lintel

	# AMD specific
	mov	\$0x80000000,%eax
	cpuid
	cmp	\$0x80000001,%eax
	jb	.Lintel
	mov	%eax,%r10d
	mov	\$0x80000001,%eax
	cpuid
	or	%ecx,%r9d
	and	\$IA32CAP_MASK1_AMD_XOP,%r9d	# isolate AMD XOP bit
	or	\$1,%r9d			# make sure %r9d is not zero

	cmp	\$0x80000008,%r10d
	jb	.Lintel

	mov	\$0x80000008,%eax
	cpuid
	movzb	%cl,%r10		# number of cores - 1
	inc	%r10			# number of cores

	mov	\$1,%eax
	cpuid
	bt	\$IA32CAP_BIT0_HT,%edx	# test hyper-threading bit
	jnc	.Lgeneric
	shr	\$16,%ebx		# number of logical processors
	cmp	%r10b,%bl
	ja	.Lgeneric
	xor	\$IA32CAP_MASK0_HT,%edx
	jmp	.Lgeneric

.Lintel:
	cmp	\$4,%r11d
	mov	\$-1,%r10d
	jb	.Lnocacheinfo

	mov	\$4,%eax
	mov	\$0,%ecx		# query L1D
	cpuid
	mov	%eax,%r10d
	shr	\$14,%r10d
	and	\$0xfff,%r10d		# number of cores -1 per L1D

.Lnocacheinfo:
	mov	\$1,%eax
	cpuid
	# force reserved bits to 0
	and	\$(~(IA32CAP_MASK0_INTELP4 | IA32CAP_MASK0_INTEL)),%edx
	cmp	\$0,%r9d
	jne	.Lnotintel
	# set reserved bit#30 on Intel CPUs
	or	\$IA32CAP_MASK0_INTEL,%edx
	and	\$15,%ah
	cmp	\$15,%ah		# examine Family ID
	jne	.Lnotintel
	# set reserved bit#20 to engage RC4_CHAR
	or	\$IA32CAP_MASK0_INTELP4,%edx
.Lnotintel:
	bt	\$IA32CAP_BIT0_HT,%edx	# test hyper-threading bit
	jnc	.Lgeneric
	xor	\$IA32CAP_MASK0_HT,%edx
	cmp	\$0,%r10d
	je	.Lgeneric

	or	\$IA32CAP_MASK0_HT,%edx
	shr	\$16,%ebx
	cmp	\$1,%bl			# see if cache is shared
	ja	.Lgeneric
	xor	\$IA32CAP_MASK0_HT,%edx	# clear hyper-threading bit if not

.Lgeneric:
	and	\$IA32CAP_MASK1_AMD_XOP,%r9d	# isolate AMD XOP flag
	and	\$(~IA32CAP_MASK1_AMD_XOP),%ecx
	or	%ecx,%r9d		# merge AMD XOP flag

	mov	%edx,%r10d		# %r9d:%r10d is copy of %ecx:%edx
	bt	\$IA32CAP_BIT1_OSXSAVE,%r9d	# check OSXSAVE bit
	jnc	.Lclear_avx
	xor	%ecx,%ecx		# XCR0
	.byte	0x0f,0x01,0xd0		# xgetbv
	and	\$6,%eax		# isolate XMM and YMM state support
	cmp	\$6,%eax
	je	.Ldone
.Lclear_avx:
	mov	\$(~(IA32CAP_MASK1_AVX | IA32CAP_MASK1_FMA3 | IA32CAP_MASK1_AMD_XOP)),%eax
	and	%eax,%r9d		# clear AVX, FMA and AMD XOP bits
.Ldone:
	shl	\$32,%r9
	mov	%r10d,%eax
	mov	%r8,%rbx		# restore %rbx
	or	%r9,%rax
	ret
.size	OPENSSL_ia32_cpuid,.-OPENSSL_ia32_cpuid
___

close STDOUT;	# flush
