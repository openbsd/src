#!./perl -T

BEGIN {
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bDevel\/Peek\b/) {
        print "1..0 # Skip: Devel::Peek was not built\n";
        exit 0;
    }
}

use Test::More;

use Devel::Peek;

our $DEBUG = 0;
open(SAVERR, ">&STDERR") or die "Can't dup STDERR: $!";

# If I reference any lexicals in this, I get the entire outer subroutine (or
# MAIN) dumped too, which isn't really what I want, as it's a lot of faff to
# maintain that.
format PIE =
Pie     @<<<<<
$::type
Good    @>>>>>
$::mmmm
.

use constant thr => $Config{useithreads};

sub do_test {
    my $todo = $_[3];
    my $repeat_todo = $_[4];
    my $pattern = $_[2];
    if (open(OUT,">peek$$")) {
	open(STDERR, ">&OUT") or die "Can't dup OUT: $!";
	Dump($_[1]);
        print STDERR "*****\n";
        Dump($_[1]); # second dump to compare with the first to make sure nothing changed.
	open(STDERR, ">&SAVERR") or die "Can't restore STDERR: $!";
	close(OUT);
	if (open(IN, "peek$$")) {
	    local $/;
	    $pattern =~ s/\$ADDR/0x[[:xdigit:]]+/g;
	    $pattern =~ s/\$FLOAT/(?:\\d*\\.\\d+(?:e[-+]\\d+)?|\\d+)/g;
	    # handle DEBUG_LEAKING_SCALARS prefix
	    $pattern =~ s/^(\s*)(SV =.* at )/(?:$1ALLOCATED at .*?\n)?$1$2/mg;

	    # Need some clear generic mechanism to eliminate (or add) lines
	    # of dump output dependant on perl version. The (previous) use of
	    # things like $IVNV gave the illusion that the string passed in was
	    # a regexp into which variables were interpolated, but this wasn't
	    # actually true as those 'variables' actually also ate the
	    # whitespace on the line. So it seems better to mark lines that
	    # need to be eliminated. I considered (?# ... ) and (?{ ... }),
	    # but whilst embedded code or comment syntax would keep it as a
	    # legitimate regexp, it still isn't true. Seems easier and clearer
	    # things that look like comments.

	    # Could do this is in a s///mge but seems clearer like this:
	    $pattern = join '', map {
		# If we identify the version condition, take *it* out whatever
		s/\s*# (\$].*)$//
		    ? (eval $1 ? $_ : '')
		    : $_ # Didn't match, so this line is in
	    } split /^/, $pattern;
	    
	    $pattern =~ s/\$PADMY/
		($] < 5.009) ? 'PADBUSY,PADMY' : 'PADMY';
	    /mge;
	    $pattern =~ s/\$PADTMP/
		($] < 5.009) ? 'PADBUSY,PADTMP' : 'PADTMP';
	    /mge;
	    $pattern =~ s/\$RV/
		($] < 5.011) ? 'RV' : 'IV';
	    /mge;

	    print $pattern, "\n" if $DEBUG;
	    my ($dump, $dump2) = split m/\*\*\*\*\*\n/, scalar <IN>;
	    print $dump, "\n"    if $DEBUG;
	    like( $dump, qr/\A$pattern\Z/ms, $_[0])
	      or note("line " . (caller)[2]);

            local $TODO = $repeat_todo;
            is($dump2, $dump, "$_[0] (unchanged by dump)")
	      or note("line " . (caller)[2]);

	    close(IN);

            return $1;
	} else {
	    die "$0: failed to open peek$$: !\n";
	}
    } else {
	die "$0: failed to create peek$$: $!\n";
    }
}

our   $a;
our   $b;
my    $c;
local $d = 0;

END {
    1 while unlink("peek$$");
}

do_test('assignment of immediate constant (string)',
	$a = "foo",
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(POK,pPOK\\)
  PV = $ADDR "foo"\\\0
  CUR = 3
  LEN = \\d+'
       );

do_test('immediate constant (string)',
        "bar",
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(.*POK,READONLY,pPOK\\)
  PV = $ADDR "bar"\\\0
  CUR = 3
  LEN = \\d+');

do_test('assignment of immediate constant (integer)',
        $b = 123,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(IOK,pIOK\\)
  IV = 123');

do_test('immediate constant (integer)',
        456,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(.*IOK,READONLY,pIOK\\)
  IV = 456');

do_test('assignment of immediate constant (integer)',
        $c = 456,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\($PADMY,IOK,pIOK\\)
  IV = 456');

# If perl is built with PERL_PRESERVE_IVUV then maths is done as integers
# where possible and this scalar will be an IV. If NO_PERL_PRESERVE_IVUV then
# maths is done in floating point always, and this scalar will be an NV.
# ([NI]) captures the type, referred to by \1 in this regexp and $type for
# building subsequent regexps.
my $type = do_test('result of addition',
        $c + $d,
'SV = ([NI])V\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(PADTMP,\1OK,p\1OK\\)
  \1V = 456');

($d = "789") += 0.1;

do_test('floating point value',
       $d,
'SV = PVNV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(NOK,pNOK\\)
  IV = \d+
  NV = 789\\.(?:1(?:000+\d+)?|0999+\d+)
  PV = $ADDR "789"\\\0
  CUR = 3
  LEN = \\d+');

do_test('integer constant',
        0xabcd,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(.*IOK,READONLY,pIOK\\)
  IV = 43981');

do_test('undef',
        undef,
'SV = NULL\\(0x0\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(\\)');

do_test('reference to scalar',
        \$a,
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PV\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(POK,pPOK\\)
    PV = $ADDR "foo"\\\0
    CUR = 3
    LEN = \\d+');

my $c_pattern;
if ($type eq 'N') {
  $c_pattern = '
    SV = PVNV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(IOK,NOK,pIOK,pNOK\\)
      IV = 456
      NV = 456
      PV = 0';
} else {
  $c_pattern = '
    SV = IV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(IOK,pIOK\\)
      IV = 456';
}
do_test('reference to array',
       [$b,$c],
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVAV\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(\\)
    IV = 0					# $] < 5.009
    NV = 0					# $] < 5.009
    ARRAY = $ADDR
    FILL = 1
    MAX = 1
    ARYLEN = 0x0
    FLAGS = \\(REAL\\)
    Elt No. 0
    SV = IV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(IOK,pIOK\\)
      IV = 123
    Elt No. 1' . $c_pattern);

do_test('reference to hash',
       {$b=>$c},
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = [12]
    FLAGS = \\(SHAREKEYS\\)
    IV = 1					# $] < 5.009
    NV = $FLOAT					# $] < 5.009
    ARRAY = $ADDR  \\(0:7, 1:1\\)
    hash quality = 100.0%
    KEYS = 1
    FILL = 1
    MAX = 7
    Elt "123" HASH = $ADDR' . $c_pattern,
	'',
	$] > 5.009 && $] < 5.015
	 && 'The hash iterator used in dump.c sets the OOK flag');

do_test('reference to anon sub with empty prototype',
        sub(){@_},
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVCV\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\($PADMY,POK,pPOK,ANON,WEAKOUTSIDE,CVGV_RC\\) # $] < 5.015 || !thr
    FLAGS = \\($PADMY,POK,pPOK,ANON,WEAKOUTSIDE,CVGV_RC,DYNFILE\\) # $] >= 5.015 && thr
    IV = 0					# $] < 5.009
    NV = 0					# $] < 5.009
    PROTOTYPE = ""
    COMP_STASH = $ADDR\\t"main"
    START = $ADDR ===> \\d+
    ROOT = $ADDR
    XSUB = 0x0					# $] < 5.009
    XSUBANY = 0					# $] < 5.009
    GVGV::GV = $ADDR\\t"main" :: "__ANON__[^"]*"
    FILE = ".*\\b(?i:peek\\.t)"
    DEPTH = 0(?:
    MUTEXP = $ADDR
    OWNER = $ADDR)?
    FLAGS = 0x404				# $] < 5.009
    FLAGS = 0x490		# $] >= 5.009 && ($] < 5.015 || !thr)
    FLAGS = 0x1490				# $] >= 5.015 && thr
    OUTSIDE_SEQ = \\d+
    PADLIST = $ADDR
    PADNAME = $ADDR\\($ADDR\\) PAD = $ADDR\\($ADDR\\)
    OUTSIDE = $ADDR \\(MAIN\\)');

do_test('reference to named subroutine without prototype',
        \&do_test,
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVCV\\($ADDR\\) at $ADDR
    REFCNT = (3|4)
    FLAGS = \\((?:HASEVAL)?\\)			# $] < 5.015 || !thr
    FLAGS = \\(DYNFILE(?:,HASEVAL)?\\)		# $] >= 5.015 && thr
    IV = 0					# $] < 5.009
    NV = 0					# $] < 5.009
    COMP_STASH = $ADDR\\t"main"
    START = $ADDR ===> \\d+
    ROOT = $ADDR
    XSUB = 0x0					# $] < 5.009
    XSUBANY = 0					# $] < 5.009
    GVGV::GV = $ADDR\\t"main" :: "do_test"
    FILE = ".*\\b(?i:peek\\.t)"
    DEPTH = 1(?:
    MUTEXP = $ADDR
    OWNER = $ADDR)?
    FLAGS = 0x(?:400)?0				# $] < 5.015 || !thr
    FLAGS = 0x[145]000				# $] >= 5.015 && thr
    OUTSIDE_SEQ = \\d+
    PADLIST = $ADDR
    PADNAME = $ADDR\\($ADDR\\) PAD = $ADDR\\($ADDR\\)
       \\d+\\. $ADDR<\\d+> \\(\\d+,\\d+\\) "\\$todo"
       \\d+\\. $ADDR<\\d+> \\(\\d+,\\d+\\) "\\$repeat_todo"
       \\d+\\. $ADDR<\\d+> \\(\\d+,\\d+\\) "\\$pattern"
      \\d+\\. $ADDR<\\d+> FAKE "\\$DEBUG"			# $] < 5.009
      \\d+\\. $ADDR<\\d+> FAKE "\\$DEBUG" flags=0x0 index=0	# $] >= 5.009
      \\d+\\. $ADDR<\\d+> \\(\\d+,\\d+\\) "\\$dump"
      \\d+\\. $ADDR<\\d+> \\(\\d+,\\d+\\) "\\$dump2"
    OUTSIDE = $ADDR \\(MAIN\\)');

if ($] >= 5.011) {
do_test('reference to regexp',
        qr(tic),
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = REGEXP\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(OBJECT,POK,FAKE,pPOK\\)		# $] < 5.017006
    FLAGS = \\(OBJECT,FAKE\\)			# $] >= 5.017006
    PV = $ADDR "\\(\\?\\^:tic\\)"
    CUR = 8
    LEN = 0					# $] < 5.017006
    STASH = $ADDR\\t"Regexp"'
. ($] < 5.013 ? '' :
'
    COMPFLAGS = 0x0 \(\)
    EXTFLAGS = 0x680000 \(CHECK_ALL,USE_INTUIT_NOML,USE_INTUIT_ML\)
    INTFLAGS = 0x0
    NPARENS = 0
    LASTPAREN = 0
    LASTCLOSEPAREN = 0
    MINLEN = 3
    MINLENRET = 3
    GOFS = 0
    PRE_PREFIX = 4
    SUBLEN = 0
    SUBOFFSET = 0
    SUBCOFFSET = 0
    SUBBEG = 0x0
    ENGINE = $ADDR
    MOTHER_RE = $ADDR
    PAREN_NAMES = 0x0
    SUBSTRS = $ADDR
    PPRIVATE = $ADDR
    OFFS = $ADDR
    QR_ANONCV = 0x0(?:
    SAVED_COPY = 0x0)?'
));
} else {
do_test('reference to regexp',
        qr(tic),
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVMG\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(OBJECT,SMG\\)
    IV = 0
    NV = 0
    PV = 0
    MAGIC = $ADDR
      MG_VIRTUAL = $ADDR
      MG_TYPE = PERL_MAGIC_qr\(r\)
      MG_OBJ = $ADDR
        PAT = "\(\?^:tic\)"			# $] >= 5.009
        REFCNT = 2				# $] >= 5.009
    STASH = $ADDR\\t"Regexp"');
}

do_test('reference to blessed hash',
        (bless {}, "Tac"),
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = [12]
    FLAGS = \\(OBJECT,SHAREKEYS\\)
    IV = 0					# $] < 5.009
    NV = 0					# $] < 5.009
    STASH = $ADDR\\t"Tac"
    ARRAY = 0x0
    KEYS = 0
    FILL = 0
    MAX = 7', '',
	$] > 5.009
	? $] >= 5.015
	     ? 0
	     : 'The hash iterator used in dump.c sets the OOK flag'
	: "Something causes the HV's array to become allocated");

do_test('typeglob',
	*a,
'SV = PVGV\\($ADDR\\) at $ADDR
  REFCNT = 5
  FLAGS = \\(MULTI(?:,IN_PAD)?\\)		# $] >= 5.009
  FLAGS = \\(GMG,SMG,MULTI(?:,IN_PAD)?\\)	# $] < 5.009
  IV = 0					# $] < 5.009
  NV = 0					# $] < 5.009
  PV = 0					# $] < 5.009
  MAGIC = $ADDR					# $] < 5.009
    MG_VIRTUAL = &PL_vtbl_glob			# $] < 5.009
    MG_TYPE = PERL_MAGIC_glob\(\*\)		# $] < 5.009
    MG_OBJ = $ADDR				# $] < 5.009
  NAME = "a"
  NAMELEN = 1
  GvSTASH = $ADDR\\t"main"
  GP = $ADDR
    SV = $ADDR
    REFCNT = 1
    IO = 0x0
    FORM = 0x0  
    AV = 0x0
    HV = 0x0
    CV = 0x0
    CVGEN = 0x0
    GPFLAGS = 0x0				# $] < 5.009
    LINE = \\d+
    FILE = ".*\\b(?i:peek\\.t)"
    FLAGS = $ADDR
    EGV = $ADDR\\t"a"');

if (ord('A') == 193) {
do_test('string with Unicode',
	chr(256).chr(0).chr(512),
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\((?:$PADTMP,)?POK,READONLY,pPOK,UTF8\\)
  PV = $ADDR "\\\214\\\101\\\0\\\235\\\101"\\\0 \[UTF8 "\\\x\{100\}\\\x\{0\}\\\x\{200\}"\]
  CUR = 5
  LEN = \\d+');
} else {
do_test('string with Unicode',
	chr(256).chr(0).chr(512),
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\((?:$PADTMP,)?POK,READONLY,pPOK,UTF8\\)
  PV = $ADDR "\\\304\\\200\\\0\\\310\\\200"\\\0 \[UTF8 "\\\x\{100\}\\\x\{0\}\\\x\{200\}"\]
  CUR = 5
  LEN = \\d+');
}

if (ord('A') == 193) {
do_test('reference to hash containing Unicode',
	{chr(256)=>chr(512)},
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = [12]
    FLAGS = \\(SHAREKEYS,HASKFLAGS\\)
    UV = 1					# $] < 5.009
    NV = $FLOAT					# $] < 5.009
    ARRAY = $ADDR  \\(0:7, 1:1\\)
    hash quality = 100.0%
    KEYS = 1
    FILL = 1
    MAX = 7
    Elt "\\\214\\\101" \[UTF8 "\\\x\{100\}"\] HASH = $ADDR
    SV = PV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(POK,pPOK,UTF8\\)
      PV = $ADDR "\\\235\\\101"\\\0 \[UTF8 "\\\x\{200\}"\]
      CUR = 2
      LEN = \\d+',
	$] > 5.009
	? $] >= 5.015
	    ?  0
	    : 'The hash iterator used in dump.c sets the OOK flag'
	: 'sv_length has been called on the element, and cached the result in MAGIC');
} else {
do_test('reference to hash containing Unicode',
	{chr(256)=>chr(512)},
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = [12]
    FLAGS = \\(SHAREKEYS,HASKFLAGS\\)
    UV = 1					# $] < 5.009
    NV = 0					# $] < 5.009
    ARRAY = $ADDR  \\(0:7, 1:1\\)
    hash quality = 100.0%
    KEYS = 1
    FILL = 1
    MAX = 7
    Elt "\\\304\\\200" \[UTF8 "\\\x\{100\}"\] HASH = $ADDR
    SV = PV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(POK,pPOK,UTF8\\)
      PV = $ADDR "\\\310\\\200"\\\0 \[UTF8 "\\\x\{200\}"\]
      CUR = 2
      LEN = \\d+', '',
	$] > 5.009
	? $] >= 5.015
	    ?  0
	    : 'The hash iterator used in dump.c sets the OOK flag'
	: 'sv_length has been called on the element, and cached the result in MAGIC');
}

my $x="";
$x=~/.??/g;
do_test('scalar with pos magic',
        $x,
'SV = PVMG\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\($PADMY,SMG,POK,(?:IsCOW,)?pPOK\\)
  IV = \d+
  NV = 0
  PV = $ADDR ""\\\0
  CUR = 0
  LEN = \d+(?:
  COW_REFCNT = 1)?
  MAGIC = $ADDR
    MG_VIRTUAL = &PL_vtbl_mglob
    MG_TYPE = PERL_MAGIC_regex_global\\(g\\)
    MG_FLAGS = 0x01
      MINMATCH');

#
# TAINTEDDIR is not set on: OS2, AMIGAOS, WIN32, MSDOS
# environment variables may be invisibly case-forced, hence the (?i:PATH)
# C<scalar(@ARGV)> is turned into an IV on VMS hence the (?:IV)?
# Perl 5.18 ensures all env vars end up as strings only, hence the (?:,pIOK)?
# Perl 5.18 ensures even magic vars have public OK, hence the (?:,POK)?
# VMS is setting FAKE and READONLY flags.  What VMS uses for storing
# ENV hashes is also not always null terminated.
#
if (${^TAINT}) {
  do_test('tainted value in %ENV',
          $ENV{PATH}=@ARGV,  # scalar(@ARGV) is a handy known tainted value
'SV = PVMG\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(GMG,SMG,RMG(?:,POK)?(?:,pIOK)?,pPOK\\)
  IV = 0
  NV = 0
  PV = $ADDR "0"\\\0
  CUR = 1
  LEN = \d+
  MAGIC = $ADDR
    MG_VIRTUAL = &PL_vtbl_envelem
    MG_TYPE = PERL_MAGIC_envelem\\(e\\)
(?:    MG_FLAGS = 0x01
      TAINTEDDIR
)?    MG_LEN = -?\d+
    MG_PTR = $ADDR (?:"(?i:PATH)"|=> HEf_SVKEY
    SV = PV(?:IV)?\\($ADDR\\) at $ADDR
      REFCNT = \d+
      FLAGS = \\(TEMP,POK,(?:FAKE,READONLY,)?pPOK\\)
(?:      IV = 0
)?      PV = $ADDR "(?i:PATH)"(?:\\\0)?
      CUR = \d+
      LEN = \d+)
  MAGIC = $ADDR
    MG_VIRTUAL = &PL_vtbl_taint
    MG_TYPE = PERL_MAGIC_taint\\(t\\)');
}

do_test('blessed reference',
	bless(\\undef, 'Foobar'),
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVMG\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(OBJECT,ROK\\)
    IV = -?\d+
    NV = $FLOAT
    RV = $ADDR
    SV = NULL\\(0x0\\) at $ADDR
      REFCNT = \d+
      FLAGS = \\(READONLY\\)
    PV = $ADDR ""
    CUR = 0
    LEN = 0
    STASH = $ADDR\s+"Foobar"');

sub const () {
    "Perl rules";
}

do_test('constant subroutine',
	\&const,
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVCV\\($ADDR\\) at $ADDR
    REFCNT = (2)
    FLAGS = \\(POK,pPOK,CONST,ISXSUB\\)		# $] < 5.015
    FLAGS = \\(POK,pPOK,CONST,DYNFILE,ISXSUB\\)	# $] >= 5.015
    IV = 0					# $] < 5.009
    NV = 0					# $] < 5.009
    PROTOTYPE = ""
    COMP_STASH = 0x0
    ROOT = 0x0					# $] < 5.009
    XSUB = $ADDR
    XSUBANY = $ADDR \\(CONST SV\\)
    SV = PV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(.*POK,READONLY,pPOK\\)
      PV = $ADDR "Perl rules"\\\0
      CUR = 10
      LEN = \\d+
    GVGV::GV = $ADDR\\t"main" :: "const"
    FILE = ".*\\b(?i:peek\\.t)"
    DEPTH = 0(?:
    MUTEXP = $ADDR
    OWNER = $ADDR)?
    FLAGS = 0x200				# $] < 5.009
    FLAGS = 0xc00				# $] >= 5.009 && $] < 5.013
    FLAGS = 0xc					# $] >= 5.013 && $] < 5.015
    FLAGS = 0x100c				# $] >= 5.015
    OUTSIDE_SEQ = 0
    PADLIST = 0x0
    OUTSIDE = 0x0 \\(null\\)');	

do_test('isUV should show on PVMG',
	do { my $v = $1; $v = ~0; $v },
'SV = PVMG\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(IOK,pIOK,IsUV\\)
  UV = \d+
  NV = 0
  PV = 0');

do_test('IO',
	*STDOUT{IO},
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVIO\\($ADDR\\) at $ADDR
    REFCNT = 3
    FLAGS = \\(OBJECT\\)
    IV = 0					# $] < 5.011
    NV = 0					# $] < 5.011
    STASH = $ADDR\s+"IO::File"
    IFP = $ADDR
    OFP = $ADDR
    DIRP = 0x0
    LINES = 0
    PAGE = 0
    PAGE_LEN = 60
    LINES_LEFT = 0
    TOP_GV = 0x0
    FMT_GV = 0x0
    BOTTOM_GV = 0x0
    SUBPROCESS = 0				# $] < 5.009
    TYPE = \'>\'
    FLAGS = 0x4');

do_test('FORMAT',
	*PIE{FORMAT},
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVFM\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(\\)				# $] < 5.015 || !thr
    FLAGS = \\(DYNFILE\\)			# $] >= 5.015 && thr
    IV = 0					# $] < 5.009
    NV = 0					# $] < 5.009
(?:    PV = 0
)?    COMP_STASH = 0x0
    START = $ADDR ===> \\d+
    ROOT = $ADDR
    XSUB = 0x0					# $] < 5.009
    XSUBANY = 0					# $] < 5.009
    GVGV::GV = $ADDR\\t"main" :: "PIE"
    FILE = ".*\\b(?i:peek\\.t)"(?:
    DEPTH = 0)?(?:
    MUTEXP = $ADDR
    OWNER = $ADDR)?
    FLAGS = 0x0					# $] < 5.015 || !thr
    FLAGS = 0x1000				# $] >= 5.015 && thr
    OUTSIDE_SEQ = \\d+
    LINES = 0					# $] < 5.017_003
    PADLIST = $ADDR
    PADNAME = $ADDR\\($ADDR\\) PAD = $ADDR\\($ADDR\\)
    OUTSIDE = $ADDR \\(MAIN\\)');

do_test('blessing to a class with embedded NUL characters',
        (bless {}, "\0::foo::\n::baz::\t::\0"),
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = [12]
    FLAGS = \\(OBJECT,SHAREKEYS\\)
    IV = 0					# $] < 5.009
    NV = 0					# $] < 5.009
    STASH = $ADDR\\t"\\\\0::foo::\\\\n::baz::\\\\t::\\\\0"
    ARRAY = $ADDR
    KEYS = 0
    FILL = 0
    MAX = 7', '',
	$] > 5.009
	? $] >= 5.015
	    ?  0
	    : 'The hash iterator used in dump.c sets the OOK flag'
	: "Something causes the HV's array to become allocated");

do_test('ENAME on a stash',
        \%RWOM::,
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(OOK,SHAREKEYS\\)
    IV = 1					# $] < 5.009
    NV = $FLOAT					# $] < 5.009
    ARRAY = $ADDR
    KEYS = 0
    FILL = 0
    MAX = 7
    RITER = -1
    EITER = 0x0
    RAND = $ADDR
    NAME = "RWOM"
    ENAME = "RWOM"				# $] > 5.012
');

*KLANK:: = \%RWOM::;

do_test('ENAMEs on a stash',
        \%RWOM::,
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 3
    FLAGS = \\(OOK,SHAREKEYS\\)
    IV = 1					# $] < 5.009
    NV = $FLOAT					# $] < 5.009
    ARRAY = $ADDR
    KEYS = 0
    FILL = 0
    MAX = 7
    RITER = -1
    EITER = 0x0
    RAND = $ADDR
    NAME = "RWOM"
    NAMECOUNT = 2				# $] > 5.012
    ENAME = "RWOM", "KLANK"			# $] > 5.012
');

undef %RWOM::;

do_test('ENAMEs on a stash with no NAME',
        \%RWOM::,
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 3
    FLAGS = \\(OOK,SHAREKEYS\\)			# $] < 5.017
    FLAGS = \\(OOK,OVERLOAD,SHAREKEYS\\)	# $] >=5.017
    IV = 1					# $] < 5.009
    NV = $FLOAT					# $] < 5.009
    ARRAY = $ADDR
    KEYS = 0
    FILL = 0
    MAX = 7
    RITER = -1
    EITER = 0x0
    RAND = $ADDR
    NAMECOUNT = -3				# $] > 5.012
    ENAME = "RWOM", "KLANK"			# $] > 5.012
');

SKIP: {
    skip "Not built with usemymalloc", 1
      unless $Config{usemymalloc} eq 'y';
    my $x = __PACKAGE__;
    ok eval { fill_mstats($x); 1 }, 'fill_mstats on COW scalar'
     or diag $@;
}

# This is more a test of fbm_compile/pp_study (non) interaction than dumping
# prowess, but short of duplicating all the gubbins of this file, I can't see
# a way to make a better place for it:

use constant {
    perl => 'rules',
    beer => 'foamy',
};

unless ($Config{useithreads}) {
    # These end up as copies in pads under ithreads, which rather defeats the
    # the point of what we're trying to test here.

    do_test('regular string constant', perl,
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 5
  FLAGS = \\(PADMY,POK,READONLY,pPOK\\)
  PV = $ADDR "rules"\\\0
  CUR = 5
  LEN = \d+
');

    eval 'index "", perl';

    # FIXME - really this shouldn't say EVALED. It's a false posistive on
    # 0x40000000 being used for several things, not a flag for "I'm in a string
    # eval"

    do_test('string constant now an FBM', perl,
'SV = PVMG\\($ADDR\\) at $ADDR
  REFCNT = 5
  FLAGS = \\(PADMY,SMG,POK,READONLY,pPOK,VALID,EVALED\\)
  PV = $ADDR "rules"\\\0
  CUR = 5
  LEN = \d+
  MAGIC = $ADDR
    MG_VIRTUAL = &PL_vtbl_regexp
    MG_TYPE = PERL_MAGIC_bm\\(B\\)
    MG_LEN = 256
    MG_PTR = $ADDR "(?:\\\\\d){256}"
  RARE = \d+
  PREVIOUS = 1
  USEFUL = 100
');

    is(study perl, '', "Not allowed to study an FBM");

    do_test('string constant still an FBM', perl,
'SV = PVMG\\($ADDR\\) at $ADDR
  REFCNT = 5
  FLAGS = \\(PADMY,SMG,POK,READONLY,pPOK,VALID,EVALED\\)
  PV = $ADDR "rules"\\\0
  CUR = 5
  LEN = \d+
  MAGIC = $ADDR
    MG_VIRTUAL = &PL_vtbl_regexp
    MG_TYPE = PERL_MAGIC_bm\\(B\\)
    MG_LEN = 256
    MG_PTR = $ADDR "(?:\\\\\d){256}"
  RARE = \d+
  PREVIOUS = 1
  USEFUL = 100
');

    do_test('regular string constant', beer,
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 6
  FLAGS = \\(PADMY,POK,READONLY,pPOK\\)
  PV = $ADDR "foamy"\\\0
  CUR = 5
  LEN = \d+
');

    is(study beer, 1, "Our studies were successful");

    do_test('string constant quite unaffected', beer, 'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 6
  FLAGS = \\(PADMY,POK,READONLY,pPOK\\)
  PV = $ADDR "foamy"\\\0
  CUR = 5
  LEN = \d+
');

    my $want = 'SV = PVMG\\($ADDR\\) at $ADDR
  REFCNT = 6
  FLAGS = \\(PADMY,SMG,POK,READONLY,pPOK,VALID,EVALED\\)
  PV = $ADDR "foamy"\\\0
  CUR = 5
  LEN = \d+
  MAGIC = $ADDR
    MG_VIRTUAL = &PL_vtbl_regexp
    MG_TYPE = PERL_MAGIC_bm\\(B\\)
    MG_LEN = 256
    MG_PTR = $ADDR "(?:\\\\\d){256}"
  RARE = \d+
  PREVIOUS = \d+
  USEFUL = 100
';

    is (eval 'index "not too foamy", beer', 8, 'correct index');

    do_test('string constant now FBMed', beer, $want);

    my $pie = 'good';

    is(study $pie, 1, "Our studies were successful");

    do_test('string constant still FBMed', beer, $want);

    do_test('second string also unaffected', $pie, 'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(PADMY,POK,pPOK\\)
  PV = $ADDR "good"\\\0
  CUR = 4
  LEN = \d+
');
}

# (One block of study tests removed when study was made a no-op.)

{
    open(OUT,">peek$$") or die "Failed to open peek $$: $!";
    open(STDERR, ">&OUT") or die "Can't dup OUT: $!";
    DeadCode();
    open(STDERR, ">&SAVERR") or die "Can't restore STDERR: $!";
    pass "no crash with DeadCode";
    close OUT;
}

do_test('UTF-8 in a regular expression',
        qr/\x{100}/,
'SV = IV\($ADDR\) at $ADDR
  REFCNT = 1
  FLAGS = \(ROK\)
  RV = $ADDR
  SV = REGEXP\($ADDR\) at $ADDR
    REFCNT = 1
    FLAGS = \(OBJECT,FAKE,UTF8\)
    PV = $ADDR "\(\?\^u:\\\\\\\\x\{100\}\)" \[UTF8 "\(\?\^u:\\\\\\\\x\{100\}\)"\]
    CUR = 13
    STASH = $ADDR	"Regexp"
    COMPFLAGS = 0x0 \(\)
    EXTFLAGS = 0x680040 \(CHECK_ALL,USE_INTUIT_NOML,USE_INTUIT_ML\)
    INTFLAGS = 0x0
    NPARENS = 0
    LASTPAREN = 0
    LASTCLOSEPAREN = 0
    MINLEN = 1
    MINLENRET = 1
    GOFS = 0
    PRE_PREFIX = 5
    SUBLEN = 0
    SUBOFFSET = 0
    SUBCOFFSET = 0
    SUBBEG = 0x0
    ENGINE = $ADDR
    MOTHER_RE = $ADDR
    PAREN_NAMES = 0x0
    SUBSTRS = $ADDR
    PPRIVATE = $ADDR
    OFFS = $ADDR
    QR_ANONCV = 0x0(?:
    SAVED_COPY = 0x0)?
');

done_testing();
