#!/usr/bin/perl -w
# 
# Regenerate (overwriting only if changed):
#
#    opcode.h
#    opnames.h
#    pp_proto.h
#
# from information stored in regen/opcodes, plus the
# values hardcoded into this script in @raw_alias.
#
# Accepts the standard regen_lib -q and -v args.
#
# This script is normally invoked from regen.pl.

use strict;

BEGIN {
    # Get function prototypes
    require 'regen/regen_lib.pl';
}

my $oc = open_new('opcode.h', '>',
		  {by => 'regen/opcode.pl', from => 'its data',
		   file => 'opcode.h', style => '*',
		   copyright => [1993 .. 2007]});

my $on = open_new('opnames.h', '>',
		  { by => 'regen/opcode.pl', from => 'its data', style => '*',
		    file => 'opnames.h', copyright => [1999 .. 2008] });

# Read data.

my %seen;
my (@ops, %desc, %check, %ckname, %flags, %args, %opnum);

open OPS, 'regen/opcodes' or die $!;

while (<OPS>) {
    chop;
    next unless $_;
    next if /^#/;
    my ($key, $desc, $check, $flags, $args) = split(/\t+/, $_, 5);
    $args = '' unless defined $args;

    warn qq[Description "$desc" duplicates $seen{$desc}\n]
     if $seen{$desc} and $key !~ "transr|(?:intro|clone)cv";
    die qq[Opcode "$key" duplicates $seen{$key}\n] if $seen{$key};
    die qq[Opcode "freed" is reserved for the slab allocator\n]
	if $key eq 'freed';
    $seen{$desc} = qq[description of opcode "$key"];
    $seen{$key} = qq[opcode "$key"];

    push(@ops, $key);
    $opnum{$key} = $#ops;
    $desc{$key} = $desc;
    $check{$key} = $check;
    $ckname{$check}++;
    $flags{$key} = $flags;
    $args{$key} = $args;
}

# Set up aliases

my %alias;

# Format is "this function" => "does these op names"
my @raw_alias = (
		 Perl_do_kv => [qw( keys values )],
		 Perl_unimplemented_op => [qw(padany mapstart custom)],
		 # All the ops with a body of { return NORMAL; }
		 Perl_pp_null => [qw(scalar regcmaybe lineseq scope)],

		 Perl_pp_goto => ['dump'],
		 Perl_pp_require => ['dofile'],
		 Perl_pp_untie => ['dbmclose'],
		 Perl_pp_sysread => {read => '', recv => '#ifdef HAS_SOCKET'},
		 Perl_pp_sysseek => ['seek'],
		 Perl_pp_ioctl => ['fcntl'],
		 Perl_pp_ssockopt => {gsockopt => '#ifdef HAS_SOCKET'},
		 Perl_pp_getpeername => {getsockname => '#ifdef HAS_SOCKET'},
		 Perl_pp_stat => ['lstat'],
		 Perl_pp_ftrowned => [qw(fteowned ftzero ftsock ftchr ftblk
					 ftfile ftdir ftpipe ftsuid ftsgid
 					 ftsvtx)],
		 Perl_pp_fttext => ['ftbinary'],
		 Perl_pp_gmtime => ['localtime'],
		 Perl_pp_semget => [qw(shmget msgget)],
		 Perl_pp_semctl => [qw(shmctl msgctl)],
		 Perl_pp_ghostent => [qw(ghbyname ghbyaddr)],
		 Perl_pp_gnetent => [qw(gnbyname gnbyaddr)],
		 Perl_pp_gprotoent => [qw(gpbyname gpbynumber)],
		 Perl_pp_gservent => [qw(gsbyname gsbyport)],
		 Perl_pp_gpwent => [qw(gpwnam gpwuid)],
		 Perl_pp_ggrent => [qw(ggrnam ggrgid)],
		 Perl_pp_ftis => [qw(ftsize ftmtime ftatime ftctime)],
		 Perl_pp_chown => [qw(unlink chmod utime kill)],
		 Perl_pp_link => ['symlink'],
		 Perl_pp_ftrread => [qw(ftrwrite ftrexec fteread ftewrite
 					fteexec)],
		 Perl_pp_shmwrite => [qw(shmread msgsnd msgrcv semop)],
		 Perl_pp_syswrite => {send => '#ifdef HAS_SOCKET'},
		 Perl_pp_defined => [qw(dor dorassign)],
                 Perl_pp_and => ['andassign'],
		 Perl_pp_or => ['orassign'],
		 Perl_pp_ucfirst => ['lcfirst'],
		 Perl_pp_sle => [qw(slt sgt sge)],
		 Perl_pp_print => ['say'],
		 Perl_pp_index => ['rindex'],
		 Perl_pp_oct => ['hex'],
		 Perl_pp_shift => ['pop'],
		 Perl_pp_sin => [qw(cos exp log sqrt)],
		 Perl_pp_bit_or => ['bit_xor'],
		 Perl_pp_rv2av => ['rv2hv'],
		 Perl_pp_akeys => ['avalues'],
		 Perl_pp_rkeys => [qw(rvalues reach)],
		 Perl_pp_trans => [qw(trans transr)],
		 Perl_pp_chop => [qw(chop chomp)],
		 Perl_pp_schop => [qw(schop schomp)],
		 Perl_pp_bind => {connect => '#ifdef HAS_SOCKET'},
		 Perl_pp_preinc => ['i_preinc', 'predec', 'i_predec'],
		 Perl_pp_postinc => ['i_postinc', 'postdec', 'i_postdec'],
		 Perl_pp_ehostent => [qw(enetent eprotoent eservent
					 spwent epwent sgrent egrent)],
		 Perl_pp_shostent => [qw(snetent sprotoent sservent)],
		 Perl_pp_aelemfast => ['aelemfast_lex'],
		);

while (my ($func, $names) = splice @raw_alias, 0, 2) {
    if (ref $names eq 'ARRAY') {
	foreach (@$names) {
	    $alias{$_} = [$func, ''];
	}
    } else {
	while (my ($opname, $cond) = each %$names) {
	    $alias{$opname} = [$func, $cond];
	}
    }
}

foreach my $sock_func (qw(socket bind listen accept shutdown
			  ssockopt getpeername)) {
    $alias{$sock_func} = ["Perl_pp_$sock_func", '#ifdef HAS_SOCKET'],
}

# Emit defines.

print $oc    "#ifndef PERL_GLOBAL_STRUCT_INIT\n\n";

{
    my $last_cond = '';
    my @unimplemented;

    sub unimplemented {
	if (@unimplemented) {
	    print $oc "#else\n";
	    foreach (@unimplemented) {
		print $oc "#define $_ Perl_unimplemented_op\n";
	    }
	    print $oc "#endif\n";
	    @unimplemented = ();
	}

    }

    for (@ops) {
	my ($impl, $cond) = @{$alias{$_} || ["Perl_pp_$_", '']};
	my $op_func = "Perl_pp_$_";

	if ($cond ne $last_cond) {
	    # A change in condition. (including to or from no condition)
	    unimplemented();
	    $last_cond = $cond;
	    if ($last_cond) {
		print $oc "$last_cond\n";
	    }
	}
	push @unimplemented, $op_func if $last_cond;
	print $oc "#define $op_func $impl\n" if $impl ne $op_func;
    }
    # If the last op was conditional, we need to close it out:
    unimplemented();
}

print $on "typedef enum opcode {\n";

my $i = 0;
for (@ops) {
      print $on "\t", tab(3,"OP_\U$_"), " = ", $i++, ",\n";
}
print $on "\t", tab(3,"OP_max"), "\n";
print $on "} opcode;\n";
print $on "\n#define MAXO ", scalar @ops, "\n";
print $on "#define OP_FREED MAXO\n";

# Emit op names and descriptions.

print $oc <<'END';
START_EXTERN_C

#ifndef DOINIT
EXTCONST char* const PL_op_name[];
#else
EXTCONST char* const PL_op_name[] = {
END

for (@ops) {
    print $oc qq(\t"$_",\n);
}

print $oc <<'END';
	"freed",
};
#endif

#ifndef DOINIT
EXTCONST char* const PL_op_desc[];
#else
EXTCONST char* const PL_op_desc[] = {
END

for (@ops) {
    my($safe_desc) = $desc{$_};

    # Have to escape double quotes and escape characters.
    $safe_desc =~ s/([\\"])/\\$1/g;

    print $oc qq(\t"$safe_desc",\n);
}

print $oc <<'END';
	"freed op",
};
#endif

END_EXTERN_C

#endif /* !PERL_GLOBAL_STRUCT_INIT */
END

# Emit ppcode switch array.

print $oc <<'END';

START_EXTERN_C

#ifdef PERL_GLOBAL_STRUCT_INIT
#  define PERL_PPADDR_INITED
static const Perl_ppaddr_t Gppaddr[]
#else
#  ifndef PERL_GLOBAL_STRUCT
#    define PERL_PPADDR_INITED
EXT Perl_ppaddr_t PL_ppaddr[] /* or perlvars.h */
#  endif
#endif /* PERL_GLOBAL_STRUCT */
#if (defined(DOINIT) && !defined(PERL_GLOBAL_STRUCT)) || defined(PERL_GLOBAL_STRUCT_INIT)
#  define PERL_PPADDR_INITED
= {
END

for (@ops) {
    my $op_func = "Perl_pp_$_";
    my $name = $alias{$_};
    if ($name && $name->[0] ne $op_func) {
	print $oc "\t$op_func,\t/* implemented by $name->[0] */\n";
    }
    else {
	print $oc "\t$op_func,\n";
    }
}

print $oc <<'END';
}
#endif
#ifdef PERL_PPADDR_INITED
;
#endif

#ifdef PERL_GLOBAL_STRUCT_INIT
#  define PERL_CHECK_INITED
static const Perl_check_t Gcheck[]
#else
#  ifndef PERL_GLOBAL_STRUCT
#    define PERL_CHECK_INITED
EXT Perl_check_t PL_check[] /* or perlvars.h */
#  endif
#endif
#if (defined(DOINIT) && !defined(PERL_GLOBAL_STRUCT)) || defined(PERL_GLOBAL_STRUCT_INIT)
#  define PERL_CHECK_INITED
= {
END

for (@ops) {
    print $oc "\t", tab(3, "Perl_$check{$_},"), "\t/* $_ */\n";
}

print $oc <<'END';
}
#endif
#ifdef PERL_CHECK_INITED
;
#endif /* #ifdef PERL_CHECK_INITED */

#ifndef PERL_GLOBAL_STRUCT_INIT

#ifndef DOINIT
EXTCONST U32 PL_opargs[];
#else
EXTCONST U32 PL_opargs[] = {
END

# Emit allowed argument types.

my $ARGBITS = 32;

my %argnum = (
    'S',  1,		# scalar
    'L',  2,		# list
    'A',  3,		# array value
    'H',  4,		# hash value
    'C',  5,		# code value
    'F',  6,		# file value
    'R',  7,		# scalar reference
);

my %opclass = (
    '0',  0,		# baseop
    '1',  1,		# unop
    '2',  2,		# binop
    '|',  3,		# logop
    '@',  4,		# listop
    '/',  5,		# pmop
    '$',  6,		# svop_or_padop
    '#',  7,		# padop
    '"',  8,		# pvop_or_svop
    '{',  9,		# loop
    ';',  10,		# cop
    '%',  11,		# baseop_or_unop
    '-',  12,		# filestatop
    '}',  13,		# loopexop
);

my %opflags = (
    'm' =>   1,		# needs stack mark
    'f' =>   2,		# fold constants
    's' =>   4,		# always produces scalar
    't' =>   8,		# needs target scalar
    'T' =>   8 | 16,	# ... which may be lexical
    'i' =>   0,		# always produces integer (unused since e7311069)
    'I' =>  32,		# has corresponding int op
    'd' =>  64,		# danger, unknown side effects
    'u' => 128,		# defaults to $_
);

my %OP_IS_SOCKET;	# /Fs/
my %OP_IS_FILETEST;	# /F-/
my %OP_IS_FT_ACCESS;	# /F-+/
my %OP_IS_NUMCOMPARE;	# /S</
my %OP_IS_DIRHOP;	# /Fd/

my $OCSHIFT = 8;
my $OASHIFT = 12;

for my $op (@ops) {
    my $argsum = 0;
    my $flags = $flags{$op};
    for my $flag (keys %opflags) {
	if ($flags =~ s/$flag//) {
	    die "Flag collision for '$op' ($flags{$op}, $flag)\n"
		if $argsum & $opflags{$flag};
	    $argsum |= $opflags{$flag};
	}
    }
    die qq[Opcode '$op' has no class indicator ($flags{$op} => $flags)\n]
	unless exists $opclass{$flags};
    $argsum |= $opclass{$flags} << $OCSHIFT;
    my $argshift = $OASHIFT;
    for my $arg (split(' ',$args{$op})) {
	if ($arg =~ s/^D//) {
	    # handle 1st, just to put D 1st.
	    $OP_IS_DIRHOP{$op}   = $opnum{$op};
	}
	if ($arg =~ /^F/) {
	    # record opnums of these opnames
	    $OP_IS_SOCKET{$op}   = $opnum{$op} if $arg =~ s/s//;
	    $OP_IS_FILETEST{$op} = $opnum{$op} if $arg =~ s/-//;
	    $OP_IS_FT_ACCESS{$op} = $opnum{$op} if $arg =~ s/\+//;
        }
	elsif ($arg =~ /^S</) {
	    $OP_IS_NUMCOMPARE{$op} = $opnum{$op} if $arg =~ s/<//;
	}
	my $argnum = ($arg =~ s/\?//) ? 8 : 0;
        die "op = $op, arg = $arg\n"
	    unless exists $argnum{$arg};
	$argnum += $argnum{$arg};
	die "Argument overflow for '$op'\n"
	    if $argshift >= $ARGBITS ||
	       $argnum > ((1 << ($ARGBITS - $argshift)) - 1);
	$argsum += $argnum << $argshift;
	$argshift += 4;
    }
    $argsum = sprintf("0x%08x", $argsum);
    print $oc "\t", tab(3, "$argsum,"), "/* $op */\n";
}

print $oc <<'END';
};
#endif

#endif /* !PERL_GLOBAL_STRUCT_INIT */

END_EXTERN_C
END

# Emit OP_IS_* macros

print $on <<'EO_OP_IS_COMMENT';

/* the OP_IS_* macros are optimized to a simple range check because
    all the member OPs are contiguous in regen/opcodes table.
    opcode.pl verifies the range contiguity, or generates an OR-equals
    expression */
EO_OP_IS_COMMENT

gen_op_is_macro( \%OP_IS_SOCKET, 'OP_IS_SOCKET');
gen_op_is_macro( \%OP_IS_FILETEST, 'OP_IS_FILETEST');
gen_op_is_macro( \%OP_IS_FT_ACCESS, 'OP_IS_FILETEST_ACCESS');
gen_op_is_macro( \%OP_IS_NUMCOMPARE, 'OP_IS_NUMCOMPARE');
gen_op_is_macro( \%OP_IS_DIRHOP, 'OP_IS_DIRHOP');

sub gen_op_is_macro {
    my ($op_is, $macname) = @_;
    if (keys %$op_is) {
	
	# get opnames whose numbers are lowest and highest
	my ($first, @rest) = sort {
	    $op_is->{$a} <=> $op_is->{$b}
	} keys %$op_is;
	
	my $last = pop @rest;	# @rest slurped, get its last
	die "Invalid range of ops: $first .. $last\n" unless $last;

	print $on "\n#define $macname(op)	\\\n\t(";

	# verify that op-ct matches 1st..last range (and fencepost)
	# (we know there are no dups)
	if ( $op_is->{$last} - $op_is->{$first} == scalar @rest + 1) {
	    
	    # contiguous ops -> optimized version
	    print $on "(op) >= OP_" . uc($first)
		. " && (op) <= OP_" . uc($last);
	}
	else {
	    print $on join(" || \\\n\t ",
			   map { "(op) == OP_" . uc() } sort keys %$op_is);
	}
	print $on ")\n";
    }
}

my $pp = open_new('pp_proto.h', '>',
		  { by => 'opcode.pl', from => 'its data' });

{
    my %funcs;
    for (@ops) {
	my $name = $alias{$_} ? $alias{$_}[0] : "Perl_pp_$_";
	++$funcs{$name};
    }
    print $pp "PERL_CALLCONV OP *$_(pTHX);\n" foreach sort keys %funcs;
}
foreach ($oc, $on, $pp) {
    read_only_bottom_close_and_rename($_);
}

# Some comments about 'T' opcode classifier:

# Safe to set if the ppcode uses:
#	tryAMAGICbin, tryAMAGICun, SETn, SETi, SETu, PUSHn, PUSHTARG, SETTARG,
#	SETs(TARG), XPUSHn, XPUSHu,

# Unsafe to set if the ppcode uses dTARG or [X]RETPUSH[YES|NO|UNDEF]

# lt and friends do SETs (including ncmp, but not scmp)

# Additional mode of failure: the opcode can modify TARG before it "used"
# all the arguments (or may call an external function which does the same).
# If the target coincides with one of the arguments ==> kaboom.

# pp.c	pos substr each not OK (RETPUSHUNDEF)
#	substr vec also not OK due to LV to target (are they???)
#	ref not OK (RETPUSHNO)
#	trans not OK (dTARG; TARG = sv_newmortal();)
#	ucfirst etc not OK: TMP arg processed inplace
#	quotemeta not OK (unsafe when TARG == arg)
#	each repeat not OK too due to list context
#	pack split - unknown whether they are safe
#	sprintf: is calling do_sprintf(TARG,...) which can act on TARG
#	  before other args are processed.

#	Suspicious wrt "additional mode of failure" (and only it):
#	schop, chop, postinc/dec, bit_and etc, negate, complement.

#	Also suspicious: 4-arg substr, sprintf, uc/lc (POK_only), reverse, pack.

#	substr/vec: doing TAINT_off()???

# pp_hot.c
#	readline - unknown whether it is safe
#	match subst not OK (dTARG)
#	grepwhile not OK (not always setting)
#	join not OK (unsafe when TARG == arg)

#	Suspicious wrt "additional mode of failure": concat (dealt with
#	in ck_sassign()), join (same).

# pp_ctl.c
#	mapwhile flip caller not OK (not always setting)

# pp_sys.c
#	backtick glob warn die not OK (not always setting)
#	warn not OK (RETPUSHYES)
#	open fileno getc sysread syswrite ioctl accept shutdown
#	 ftsize(etc) readlink telldir fork alarm getlogin not OK (RETPUSHUNDEF)
#	umask select not OK (XPUSHs(&PL_sv_undef);)
#	fileno getc sysread syswrite tell not OK (meth("FILENO" "GETC"))
#	sselect shm* sem* msg* syscall - unknown whether they are safe
#	gmtime not OK (list context)

#	Suspicious wrt "additional mode of failure": warn, die, select.
