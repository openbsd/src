#!./perl -T

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bDevel\/Peek\b/) {
        print "1..0 # Skip: Devel::Peek was not built\n";
        exit 0;
    }
}

BEGIN { require "./test.pl"; }

use Devel::Peek;

plan(52);

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
	    # whitspace on the line. So it seems better to mark lines that
	    # need to be eliminated. I considered (?# ... ) and (?{ ... }),
	    # but whilst embedded code or comment syntax would keep it as a
	    # legitimate regexp, it still isn't true. Seems easier and clearer
	    # things that look like comments.

	    # Could do this is in a s///mge but seems clearer like this:
	    $pattern = join '', map {
		# If we identify the version condition, take *it* out whatever
		s/\s*# (\$] [<>]=? 5\.\d\d\d)$//
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
	    like( $dump, qr/\A$pattern\Z/ms );

            local $TODO = $repeat_todo;
            is($dump2, $dump);

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

do_test( 1,
	$a = "foo",
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(POK,pPOK\\)
  PV = $ADDR "foo"\\\0
  CUR = 3
  LEN = \\d+'
       );

do_test( 2,
        "bar",
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(.*POK,READONLY,pPOK\\)
  PV = $ADDR "bar"\\\0
  CUR = 3
  LEN = \\d+');

do_test( 3,
        $b = 123,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(IOK,pIOK\\)
  IV = 123');

do_test( 4,
        456,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(.*IOK,READONLY,pIOK\\)
  IV = 456');

do_test( 5,
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
my $type = do_test( 6,
        $c + $d,
'SV = ([NI])V\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(PADTMP,\1OK,p\1OK\\)
  \1V = 456');

($d = "789") += 0.1;

do_test( 7,
       $d,
'SV = PVNV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(NOK,pNOK\\)
  IV = \d+
  NV = 789\\.(?:1(?:000+\d+)?|0999+\d+)
  PV = $ADDR "789"\\\0
  CUR = 3
  LEN = \\d+');

do_test( 8,
        0xabcd,
'SV = IV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(.*IOK,READONLY,pIOK\\)
  IV = 43981');

do_test( 9,
        undef,
'SV = NULL\\(0x0\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(\\)');

do_test(10,
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
do_test(11,
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

do_test(12,
       {$b=>$c},
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(SHAREKEYS\\)
    IV = 1					# $] < 5.009
    NV = $FLOAT					# $] < 5.009
    ARRAY = $ADDR  \\(0:7, 1:1\\)
    hash quality = 100.0%
    KEYS = 1
    FILL = 1
    MAX = 7
    RITER = -1
    EITER = 0x0
    Elt "123" HASH = $ADDR' . $c_pattern,
	'',
	$] > 5.009 && 'The hash iterator used in dump.c sets the OOK flag');

do_test(13,
        sub(){@_},
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVCV\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\($PADMY,POK,pPOK,ANON,WEAKOUTSIDE\\)
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
    FLAGS = 0x90				# $] >= 5.009
    OUTSIDE_SEQ = \\d+
    PADLIST = $ADDR
    PADNAME = $ADDR\\($ADDR\\) PAD = $ADDR\\($ADDR\\)
    OUTSIDE = $ADDR \\(MAIN\\)');

do_test(14,
        \&do_test,
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVCV\\($ADDR\\) at $ADDR
    REFCNT = (3|4)
    FLAGS = \\(\\)
    IV = 0					# $] < 5.009
    NV = 0					# $] < 5.009
    COMP_STASH = $ADDR\\t"main"
    START = $ADDR ===> \\d+
    ROOT = $ADDR
    XSUB = 0x0					# $] < 5.009
    XSUBANY = 0					# $] < 5.009
    GVGV::GV = $ADDR\\t"main" :: "do_test"
    FILE = ".*\\b(?i:peek\\.t)"
    DEPTH = 1
(?:    MUTEXP = $ADDR
    OWNER = $ADDR
)?    FLAGS = 0x0
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
do_test(15,
        qr(tic),
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = REGEXP\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(OBJECT,POK,pPOK\\)
    IV = 0
    PV = $ADDR "\\(\\?-xism:tic\\)"\\\0
    CUR = 12
    LEN = \\d+
    STASH = $ADDR\\t"Regexp"');
} else {
do_test(15,
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
        PAT = "\(\?-xism:tic\)"			# $] >= 5.009
        REFCNT = 2				# $] >= 5.009
    STASH = $ADDR\\t"Regexp"');
}

do_test(16,
        (bless {}, "Tac"),
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(OBJECT,SHAREKEYS\\)
    IV = 0					# $] < 5.009
    NV = 0					# $] < 5.009
    STASH = $ADDR\\t"Tac"
    ARRAY = 0x0
    KEYS = 0
    FILL = 0
    MAX = 7
    RITER = -1
    EITER = 0x0', '',
	$] > 5.009 ? 'The hash iterator used in dump.c sets the OOK flag'
	: "Something causes the HV's array to become allocated");

do_test(17,
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
do_test(18,
	chr(256).chr(0).chr(512),
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\((?:$PADTMP,)?POK,READONLY,pPOK,UTF8\\)
  PV = $ADDR "\\\214\\\101\\\0\\\235\\\101"\\\0 \[UTF8 "\\\x\{100\}\\\x\{0\}\\\x\{200\}"\]
  CUR = 5
  LEN = \\d+');
} else {
do_test(18,
	chr(256).chr(0).chr(512),
'SV = PV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\((?:$PADTMP,)?POK,READONLY,pPOK,UTF8\\)
  PV = $ADDR "\\\304\\\200\\\0\\\310\\\200"\\\0 \[UTF8 "\\\x\{100\}\\\x\{0\}\\\x\{200\}"\]
  CUR = 5
  LEN = \\d+');
}

if (ord('A') == 193) {
do_test(19,
	{chr(256)=>chr(512)},
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(SHAREKEYS,HASKFLAGS\\)
    UV = 1					# $] < 5.009
    NV = $FLOAT					# $] < 5.009
    ARRAY = $ADDR  \\(0:7, 1:1\\)
    hash quality = 100.0%
    KEYS = 1
    FILL = 1
    MAX = 7
    RITER = -1
    EITER = $ADDR
    Elt "\\\214\\\101" \[UTF8 "\\\x\{100\}"\] HASH = $ADDR
    SV = PV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(POK,pPOK,UTF8\\)
      PV = $ADDR "\\\235\\\101"\\\0 \[UTF8 "\\\x\{200\}"\]
      CUR = 2
      LEN = \\d+',
	$] > 5.009 ? 'The hash iterator used in dump.c sets the OOK flag'
	: 'sv_length has been called on the element, and cached the result in MAGIC');
} else {
do_test(19,
	{chr(256)=>chr(512)},
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVHV\\($ADDR\\) at $ADDR
    REFCNT = 1
    FLAGS = \\(SHAREKEYS,HASKFLAGS\\)
    UV = 1					# $] < 5.009
    NV = 0					# $] < 5.009
    ARRAY = $ADDR  \\(0:7, 1:1\\)
    hash quality = 100.0%
    KEYS = 1
    FILL = 1
    MAX = 7
    RITER = -1
    EITER = $ADDR
    Elt "\\\304\\\200" \[UTF8 "\\\x\{100\}"\] HASH = $ADDR
    SV = PV\\($ADDR\\) at $ADDR
      REFCNT = 1
      FLAGS = \\(POK,pPOK,UTF8\\)
      PV = $ADDR "\\\310\\\200"\\\0 \[UTF8 "\\\x\{200\}"\]
      CUR = 2
      LEN = \\d+', '',
	$] > 5.009 ? 'The hash iterator used in dump.c sets the OOK flag'
	: 'sv_length has been called on the element, and cached the result in MAGIC');
}

my $x="";
$x=~/.??/g;
do_test(20,
        $x,
'SV = PVMG\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\($PADMY,SMG,POK,pPOK\\)
  IV = 0
  NV = 0
  PV = $ADDR ""\\\0
  CUR = 0
  LEN = \d+
  MAGIC = $ADDR
    MG_VIRTUAL = &PL_vtbl_mglob
    MG_TYPE = PERL_MAGIC_regex_global\\(g\\)
    MG_FLAGS = 0x01
      MINMATCH');

#
# TAINTEDDIR is not set on: OS2, AMIGAOS, WIN32, MSDOS
# environment variables may be invisibly case-forced, hence the (?i:PATH)
# C<scalar(@ARGV)> is turned into an IV on VMS hence the (?:IV)?
# VMS is setting FAKE and READONLY flags.  What VMS uses for storing
# ENV hashes is also not always null terminated.
#
do_test(21,
        $ENV{PATH}=@ARGV,  # scalar(@ARGV) is a handy known tainted value
'SV = PVMG\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(GMG,SMG,RMG,pIOK,pPOK\\)
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

# blessed refs
do_test(22,
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

# Constant subroutines

sub const () {
    "Perl rules";
}

do_test(23,
	\&const,
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVCV\\($ADDR\\) at $ADDR
    REFCNT = (2)
    FLAGS = \\(POK,pPOK,CONST\\)
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
    FLAGS = 0xc00				# $] >= 5.009
    OUTSIDE_SEQ = 0
    PADLIST = 0x0
    OUTSIDE = 0x0 \\(null\\)');	

# isUV should show on PVMG
do_test(24,
	do { my $v = $1; $v = ~0; $v },
'SV = PVMG\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(IOK,pIOK,IsUV\\)
  UV = \d+
  NV = 0
  PV = 0');

do_test(25,
	*STDOUT{IO},
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVIO\\($ADDR\\) at $ADDR
    REFCNT = 3
    FLAGS = \\(OBJECT\\)
    IV = 0
    NV = 0					# $] < 5.011
    STASH = $ADDR\s+"IO::Handle"
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
    FLAGS = 0x0');

do_test(26,
	*PIE{FORMAT},
'SV = $RV\\($ADDR\\) at $ADDR
  REFCNT = 1
  FLAGS = \\(ROK\\)
  RV = $ADDR
  SV = PVFM\\($ADDR\\) at $ADDR
    REFCNT = 2
    FLAGS = \\(\\)
    IV = 0
    NV = 0					# $] < 5.009
(?:    PV = 0
)?    COMP_STASH = 0x0
    START = $ADDR ===> \\d+
    ROOT = $ADDR
    XSUB = 0x0					# $] < 5.009
    XSUBANY = 0					# $] < 5.009
    GVGV::GV = $ADDR\\t"main" :: "PIE"
    FILE = ".*\\b(?i:peek\\.t)"
(?:    DEPTH = 0
    MUTEXP = $ADDR
    OWNER = $ADDR
)?    FLAGS = 0x0
    OUTSIDE_SEQ = \\d+
    LINES = 0
    PADLIST = $ADDR
    PADNAME = $ADDR\\($ADDR\\) PAD = $ADDR\\($ADDR\\)
    OUTSIDE = $ADDR \\(MAIN\\)');
