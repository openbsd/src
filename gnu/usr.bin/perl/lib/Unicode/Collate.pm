package Unicode::Collate;

BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	die "Unicode::Collate cannot stringify a Unicode code point\n";
    }
}

use 5.006;
use strict;
use warnings;
use Carp;
use File::Spec;

no warnings 'utf8';

require Exporter;

our $VERSION = '0.33';
our $PACKAGE = __PACKAGE__;

our @ISA = qw(Exporter);

our %EXPORT_TAGS = ();
our @EXPORT_OK = ();
our @EXPORT = ();

(our $Path = $INC{'Unicode/Collate.pm'}) =~ s/\.pm$//;
our $KeyFile = "allkeys.txt";

# Perl's boolean
use constant TRUE  => 1;
use constant FALSE => "";
use constant NOMATCHPOS => -1;

# A coderef to get combining class imported from Unicode::Normalize
# (i.e. \&Unicode::Normalize::getCombinClass).
# This is also used as a HAS_UNICODE_NORMALIZE flag.
our $CVgetCombinClass;

# Supported Levels
use constant MinLevel => 1;
use constant MaxLevel => 4;

# Minimum weights at level 2 and 3, respectively
use constant Min2Wt => 0x20;
use constant Min3Wt => 0x02;

# Shifted weight at 4th level
use constant Shift4Wt => 0xFFFF;

# A boolean for Variable and 16-bit weights at 4 levels of Collation Element
# PROBLEM: The Default Unicode Collation Element Table
# has weights over 0xFFFF at the 4th level.
# The tie-breaking in the variable weights
# other than "shift" (as well as "shift-trimmed") is unreliable.
use constant VCE_TEMPLATE => 'Cn4';

# A sort key: 16-bit weights
# See also the PROBLEM on VCE_TEMPLATE above.
use constant KEY_TEMPLATE => 'n*';

# Level separator in a sort key:
# i.e. pack(KEY_TEMPLATE, 0)
use constant LEVEL_SEP => "\0\0";

# As Unicode code point separator for hash keys.
# A joined code point string (denoted by JCPS below)
# like "65;768" is used for internal processing
# instead of Perl's Unicode string like "\x41\x{300}",
# as the native code point is different from the Unicode code point
# on EBCDIC platform.
# This character must not be included in any stringified
# representation of an integer.
use constant CODE_SEP => ';';

# boolean values of variable weights
use constant NON_VAR => 0; # Non-Variable character
use constant VAR     => 1; # Variable character

# specific code points
use constant Hangul_LBase  => 0x1100;
use constant Hangul_LIni   => 0x1100;
use constant Hangul_LFin   => 0x1159;
use constant Hangul_LFill  => 0x115F;
use constant Hangul_VBase  => 0x1161;
use constant Hangul_VIni   => 0x1160;
use constant Hangul_VFin   => 0x11A2;
use constant Hangul_TBase  => 0x11A7;
use constant Hangul_TIni   => 0x11A8;
use constant Hangul_TFin   => 0x11F9;
use constant Hangul_TCount => 28;
use constant Hangul_NCount => 588;
use constant Hangul_SBase  => 0xAC00;
use constant Hangul_SIni   => 0xAC00;
use constant Hangul_SFin   => 0xD7A3;
use constant CJK_UidIni    => 0x4E00;
use constant CJK_UidFin    => 0x9FA5;
use constant CJK_ExtAIni   => 0x3400;
use constant CJK_ExtAFin   => 0x4DB5;
use constant CJK_ExtBIni   => 0x20000;
use constant CJK_ExtBFin   => 0x2A6D6;
use constant BMP_Max       => 0xFFFF;

# Logical_Order_Exception in PropList.txt
# TODO: synchronization with change of PropList.txt.
our $DefaultRearrange = [ 0x0E40..0x0E44, 0x0EC0..0x0EC4 ];

sub UCA_Version { "11" }

sub Base_Unicode_Version { "4.0" }

######

sub pack_U {
    return pack('U*', @_);
}

sub unpack_U {
    return unpack('U*', pack('U*').shift);
}

######

my (%VariableOK);
@VariableOK{ qw/
    blanked  non-ignorable  shifted  shift-trimmed
  / } = (); # keys lowercased

our @ChangeOK = qw/
    alternate backwards level normalization rearrange
    katakana_before_hiragana upper_before_lower
    overrideHangul overrideCJK preprocess UCA_Version
    hangul_terminator variable
  /;

our @ChangeNG = qw/
    entry mapping table maxlength
    ignoreChar ignoreName undefChar undefName variableTable
    versionTable alternateTable backwardsTable forwardsTable rearrangeTable
    derivCode normCode rearrangeHash L3_ignorable
    backwardsFlag
  /;
# The hash key 'ignored' is deleted at v 0.21.
# The hash key 'isShift' is deleted at v 0.23.
# The hash key 'combining' is deleted at v 0.24.
# The hash key 'entries' is deleted at v 0.30.

sub version {
    my $self = shift;
    return $self->{versionTable} || 'unknown';
}

my (%ChangeOK, %ChangeNG);
@ChangeOK{ @ChangeOK } = ();
@ChangeNG{ @ChangeNG } = ();

sub change {
    my $self = shift;
    my %hash = @_;
    my %old;
    if (exists $hash{variable} && exists $hash{alternate}) {
	delete $hash{alternate};
    }
    elsif (!exists $hash{variable} && exists $hash{alternate}) {
	$hash{variable} = $hash{alternate};
    }
    foreach my $k (keys %hash) {
	if (exists $ChangeOK{$k}) {
	    $old{$k} = $self->{$k};
	    $self->{$k} = $hash{$k};
	}
	elsif (exists $ChangeNG{$k}) {
	    croak "change of $k via change() is not allowed!";
	}
	# else => ignored
    }
    $self->checkCollator;
    return wantarray ? %old : $self;
}

sub _checkLevel {
    my $level = shift;
    my $key   = shift; # 'level' or 'backwards'
    MinLevel <= $level or croak sprintf
	"Illegal level %d (in value for key '%s') lower than %d.",
	    $level, $key, MinLevel;
    $level <= MaxLevel or croak sprintf
	"Unsupported level %d (in value for key '%s') higher than %d.",
	    $level, $key, MaxLevel;
}

my %DerivCode = (
    8 => \&_derivCE_8,
    9 => \&_derivCE_9,
   11 => \&_derivCE_9, # 11 == 9
);

sub checkCollator {
    my $self = shift;
    _checkLevel($self->{level}, "level");

    $self->{derivCode} = $DerivCode{ $self->{UCA_Version} }
	or croak "Illegal UCA version (passed $self->{UCA_Version}).";

    $self->{variable} ||= $self->{alternate} || $self->{variableTable} ||
				$self->{alternateTable} || 'shifted';
    $self->{variable} = $self->{alternate} = lc($self->{variable});
    exists $VariableOK{ $self->{variable} }
	or croak "$PACKAGE unknown variable tag name: $self->{variable}";

    if (! defined $self->{backwards}) {
	$self->{backwardsFlag} = 0;
    }
    elsif (! ref $self->{backwards}) {
	_checkLevel($self->{backwards}, "backwards");
	$self->{backwardsFlag} = 1 << $self->{backwards};
    }
    else {
	my %level;
	$self->{backwardsFlag} = 0;
	for my $b (@{ $self->{backwards} }) {
	    _checkLevel($b, "backwards");
	    $level{$b} = 1;
	}
	for my $v (sort keys %level) {
	    $self->{backwardsFlag} += 1 << $v;
	}
    }

    defined $self->{rearrange} or $self->{rearrange} = [];
    ref $self->{rearrange}
	or croak "$PACKAGE: list for rearrangement must be store in ARRAYREF";

    # keys of $self->{rearrangeHash} are $self->{rearrange}.
    $self->{rearrangeHash} = undef;

    if (@{ $self->{rearrange} }) {
	@{ $self->{rearrangeHash} }{ @{ $self->{rearrange} } } = ();
    }

    $self->{normCode} = undef;

    if (defined $self->{normalization}) {
	eval { require Unicode::Normalize };
	$@ and croak "Unicode::Normalize is required to normalize strings";

	$CVgetCombinClass ||= \&Unicode::Normalize::getCombinClass;

	if ($self->{normalization} =~ /^(?:NF)D\z/) { # tweak for default
	    $self->{normCode} = \&Unicode::Normalize::NFD;
	}
	elsif ($self->{normalization} ne 'prenormalized') {
	    my $norm = $self->{normalization};
	    $self->{normCode} = sub {
		Unicode::Normalize::normalize($norm, shift);
	    };
	    eval { $self->{normCode}->("") }; # try
	    $@ and croak "$PACKAGE unknown normalization form name: $norm";
	}
    }
    return;
}

sub new
{
    my $class = shift;
    my $self = bless { @_ }, $class;

    # If undef is passed explicitly, no file is read.
    $self->{table} = $KeyFile if ! exists $self->{table};
    $self->read_table if defined $self->{table};

    if ($self->{entry}) {
	$self->parseEntry($_) foreach split /\n/, $self->{entry};
    }

    $self->{level} ||= MaxLevel;
    $self->{UCA_Version} ||= UCA_Version();

    $self->{overrideHangul} = FALSE
	if ! exists $self->{overrideHangul};
    $self->{overrideCJK} = FALSE
	if ! exists $self->{overrideCJK};
    $self->{normalization} = 'NFD'
	if ! exists $self->{normalization};
    $self->{rearrange} = $self->{rearrangeTable} || $DefaultRearrange
	if ! exists $self->{rearrange};
    $self->{backwards} = $self->{backwardsTable}
	if ! exists $self->{backwards};

    $self->checkCollator;

    return $self;
}

sub read_table {
    my $self = shift;

    my $filepath = File::Spec->catfile($Path, $self->{table});
    open my $fk, "<$filepath"
	or croak "File does not exist at $filepath";

    while (<$fk>) {
	next if /^\s*#/;
	unless (s/^\s*\@//) {
	    $self->parseEntry($_);
	    next;
	}

	if (/^version\s*(\S*)/) {
	    $self->{versionTable} ||= $1;
	}
	elsif (/^variable\s+(\S*)/) { # since UTS #10-9
	    $self->{variableTable} ||= $1;
	}
	elsif (/^alternate\s+(\S*)/) { # till UTS #10-8
	    $self->{alternateTable} ||= $1;
	}
	elsif (/^backwards\s+(\S*)/) {
	    push @{ $self->{backwardsTable} }, $1;
	}
	elsif (/^forwards\s+(\S*)/) { # parhaps no use
	    push @{ $self->{forwardsTable} }, $1;
	}
	elsif (/^rearrange\s+(.*)/) { # (\S*) is NG
	    push @{ $self->{rearrangeTable} }, _getHexArray($1);
	}
    }
    close $fk;
}


##
## get $line, parse it, and write an entry in $self
##
sub parseEntry
{
    my $self = shift;
    my $line = shift;
    my($name, $entry, @uv, @key);

    return if $line !~ /^\s*[0-9A-Fa-f]/;

    # removes comment and gets name
    $name = $1
	if $line =~ s/[#%]\s*(.*)//;
    return if defined $self->{undefName} && $name =~ /$self->{undefName}/;

    # gets element
    my($e, $k) = split /;/, $line;
    croak "Wrong Entry: <charList> must be separated by ';' from <collElement>"
	if ! $k;

    @uv = _getHexArray($e);
    return if !@uv;

    $entry = join(CODE_SEP, @uv); # in JCPS

    if (defined $self->{undefChar} || defined $self->{ignoreChar}) {
	my $ele = pack_U(@uv);

	# regarded as if it were not entried in the table
	return
	    if defined $self->{undefChar} && $ele =~ /$self->{undefChar}/;

	# replaced as completely ignorable
	$k = '[.0000.0000.0000.0000]'
	    if defined $self->{ignoreChar} && $ele =~ /$self->{ignoreChar}/;
    }

    # replaced as completely ignorable
    $k = '[.0000.0000.0000.0000]'
	if defined $self->{ignoreName} && $name =~ /$self->{ignoreName}/;

    my $is_L3_ignorable = TRUE;

    foreach my $arr ($k =~ /\[([^\[\]]+)\]/g) { # SPACEs allowed
	my $var = $arr =~ /\*/; # exactly /^\*/ but be lenient.
	my @wt = _getHexArray($arr);
	push @key, pack(VCE_TEMPLATE, $var, @wt);
	$is_L3_ignorable = FALSE
	    if $wt[0] + $wt[1] + $wt[2] != 0;
	  # if $arr !~ /[1-9A-Fa-f]/; NG
	  # Conformance Test shows L3-ignorable is completely ignorable.
	# For expansion, an entry $is_L3_ignorable
	# if and only if "all" CEs are [.0000.0000.0000].
    }

    $self->{mapping}{$entry} = \@key;

    if (@uv > 1) {
	(!$self->{maxlength}{$uv[0]} || $self->{maxlength}{$uv[0]} < @uv)
	    and $self->{maxlength}{$uv[0]} = @uv;
    }
    else {
	$is_L3_ignorable
	    ? ($self->{L3_ignorable}{$uv[0]} = TRUE)
	    : ($self->{L3_ignorable}{$uv[0]} and
	       $self->{L3_ignorable}{$uv[0]} = FALSE); # &&= stores key.
    }
}


##
## VCE = _varCE(variable term, VCE)
##
sub _varCE
{
    my $vbl = shift;
    my $vce = shift;
    if ($vbl eq 'non-ignorable') {
	return $vce;
    }
    my ($var, @wt) = unpack VCE_TEMPLATE, $vce;

    if ($var) {
	return pack(VCE_TEMPLATE, $var, 0, 0, 0,
		$vbl eq 'blanked' ? $wt[3] : $wt[0]);
    }
    elsif ($vbl eq 'blanked') {
	return $vce;
    }
    else {
	return pack(VCE_TEMPLATE, $var, @wt[0..2],
	    $vbl eq 'shifted' && $wt[0]+$wt[1]+$wt[2] ? Shift4Wt : 0);
    }
}

sub viewSortKey
{
    my $self = shift;
    $self->visualizeSortKey($self->getSortKey(@_));
}

sub visualizeSortKey
{
    my $self = shift;
    my $view = join " ", map sprintf("%04X", $_), unpack(KEY_TEMPLATE, shift);

    if ($self->{UCA_Version} <= 8) {
	$view =~ s/ ?0000 ?/|/g;
    } else {
	$view =~ s/\b0000\b/|/g;
    }
    return "[$view]";
}


##
## arrayref of JCPS   = splitEnt(string to be collated)
## arrayref of arrayref[JCPS, ini_pos, fin_pos] = splitEnt(string, true)
##
sub splitEnt
{
    my $self = shift;
    my $wLen = $_[1];

    my $code = $self->{preprocess};
    my $norm = $self->{normCode};
    my $map  = $self->{mapping};
    my $max  = $self->{maxlength};
    my $reH  = $self->{rearrangeHash};
    my $ign  = $self->{L3_ignorable};
    my $ver9 = $self->{UCA_Version} >= 9;

    my ($str, @buf);

    if ($wLen) {
	$code and croak "Preprocess breaks character positions. "
			. "Don't use with index(), match(), etc.";
	$norm and croak "Normalization breaks character positions. "
			. "Don't use with index(), match(), etc.";
	$str = $_[0];
    }
    else {
	$str = $_[0];
	$str = &$code($str) if ref $code;
	$str = &$norm($str) if ref $norm;
    }

    # get array of Unicode code point of string.
    my @src = unpack_U($str);

    # rearrangement:
    # Character positions are not kept if rearranged,
    # then neglected if $wLen is true.
    if ($reH && ! $wLen) {
	for (my $i = 0; $i < @src; $i++) {
	    if (exists $reH->{ $src[$i] } && $i + 1 < @src) {
		($src[$i], $src[$i+1]) = ($src[$i+1], $src[$i]);
		$i++;
	    }
	}
    }

    # To remove a character marked as a completely ignorable.
    for (my $i = 0; $i < @src; $i++) {
	$src[$i] = undef
	    if _isIllegal($src[$i]) || ($ver9 && $ign->{ $src[$i] });
    }

    for (my $i = 0; $i < @src; $i++) {
	my $jcps = $src[$i];
	next if ! defined $jcps;
	my $i_orig = $i;

	if ($max->{$jcps}) { # contract
	    my $temp_jcps = $jcps;
	    my $jcpsLen = 1;
	    my $maxLen = $max->{$jcps};

	    for (my $p = $i + 1; $jcpsLen < $maxLen && $p < @src; $p++) {
		next if ! defined $src[$p];
		$temp_jcps .= CODE_SEP . $src[$p];
		$jcpsLen++;
		if ($map->{$temp_jcps}) {
		    $jcps = $temp_jcps;
		    $i = $p;
		}
	    }

	# not-contiguous contraction with Combining Char (cf. UTS#10, S2.1).
	# This process requires Unicode::Normalize.
	# If "normalization" is undef, here should be skipped *always*
	# (in spite of bool value of $CVgetCombinClass),
	# since canonical ordering cannot be expected.
	# Blocked combining character should not be contracted.

	    if ($self->{normalization})
	    # $self->{normCode} is false in the case of "prenormalized".
	    {
		my $preCC = 0;
		my $curCC = 0;

		for (my $p = $i + 1; $p < @src; $p++) {
		    next if ! defined $src[$p];
		    $curCC = $CVgetCombinClass->($src[$p]);
		    last unless $curCC;
		    my $tail = CODE_SEP . $src[$p];
		    if ($preCC != $curCC && $map->{$jcps.$tail}) {
			$jcps .= $tail;
			$src[$p] = undef;
		    } else {
			$preCC = $curCC;
		    }
		}
	    }
	}

	if ($wLen) {
	    for (; $i + 1 < @src; $i++) {
		last if defined $src[$i + 1];
	    }
	}

	push @buf, $wLen ? [$jcps, $i_orig, $i + 1] : $jcps;
    }
    return \@buf;
}


##
## list of VCE = getWt(JCPS)
##
sub getWt
{
    my $self = shift;
    my $u    = shift;
    my $vbl  = $self->{variable};
    my $map  = $self->{mapping};
    my $der  = $self->{derivCode};

    return if !defined $u;
    return map(_varCE($vbl, $_), @{ $map->{$u} })
	if $map->{$u};

    # JCPS must not be a contraction, then it's a code point.
    if (Hangul_SIni <= $u && $u <= Hangul_SFin) {
	my $hang = $self->{overrideHangul};
	my @hangulCE;
	if ($hang) {
	    @hangulCE = map(pack(VCE_TEMPLATE, NON_VAR, @$_), &$hang($u));
	}
	elsif (!defined $hang) {
	    @hangulCE = $der->($u);
	}
	else {
	    my $max  = $self->{maxlength};
	    my @decH = _decompHangul($u);

	    if (@decH == 2) {
		my $contract = join(CODE_SEP, @decH);
		@decH = ($contract) if $map->{$contract};
	    } else { # must be <@decH == 3>
		if ($max->{$decH[0]}) {
		    my $contract = join(CODE_SEP, @decH);
		    if ($map->{$contract}) {
			@decH = ($contract);
		    } else {
			$contract = join(CODE_SEP, @decH[0,1]);
			$map->{$contract} and @decH = ($contract, $decH[2]);
		    }
		    # even if V's ignorable, LT contraction is not supported.
		    # If such a situatution were required, NFD should be used.
		}
		if (@decH == 3 && $max->{$decH[1]}) {
		    my $contract = join(CODE_SEP, @decH[1,2]);
		    $map->{$contract} and @decH = ($decH[0], $contract);
		}
	    }

	    @hangulCE = map({
		    $map->{$_} ? @{ $map->{$_} } : $der->($_);
		} @decH);
	}
	return map _varCE($vbl, $_), @hangulCE;
    }
    elsif (CJK_UidIni  <= $u && $u <= CJK_UidFin  ||
	   CJK_ExtAIni <= $u && $u <= CJK_ExtAFin ||
	   CJK_ExtBIni <= $u && $u <= CJK_ExtBFin) {
	my $cjk  = $self->{overrideCJK};
	return map _varCE($vbl, $_),
	    $cjk
		? map(pack(VCE_TEMPLATE, NON_VAR, @$_), &$cjk($u))
		: defined $cjk && $self->{UCA_Version} <= 8 && $u <= BMP_Max
		    ? pack(VCE_TEMPLATE, NON_VAR, $u, Min2Wt, Min3Wt, $u)
		    : $der->($u);
    }
    else {
	return map _varCE($vbl, $_), $der->($u);
    }
}


##
## string sortkey = getSortKey(string arg)
##
sub getSortKey
{
    my $self = shift;
    my $lev  = $self->{level};
    my $rEnt = $self->splitEnt(shift); # get an arrayref of JCPS
    my $ver9 = $self->{UCA_Version} >= 9;
    my $v2i  = $ver9 && $self->{variable} ne 'non-ignorable';

    my @buf; # weight arrays
    if ($self->{hangul_terminator}) {
	my $preHST = '';
	foreach my $jcps (@$rEnt) {
	    # weird things like VL, TL-contraction are not considered!
	    my $curHST = '';
	    foreach my $u (split /;/, $jcps) {
		$curHST .= getHST($u);
	    }
	    if ($preHST && !$curHST || # hangul before non-hangul
		$preHST =~ /L\z/ && $curHST =~ /^T/ ||
		$preHST =~ /V\z/ && $curHST =~ /^L/ ||
		$preHST =~ /T\z/ && $curHST =~ /^[LV]/) {

		push @buf, $self->getWtHangulTerm();
	    }
	    $preHST = $curHST;

	    push @buf, $self->getWt($jcps);
	}
	$preHST # end at hangul
	    and push @buf, $self->getWtHangulTerm();
    }
    else {
	foreach my $jcps (@$rEnt) {
	    push @buf, $self->getWt($jcps);
	}
    }

    # make sort key
    my @ret = ([],[],[],[]);
    my $last_is_variable;

    foreach my $vwt (@buf) {
	my($var, @wt) = unpack(VCE_TEMPLATE, $vwt);
	if ($v2i) {
	    if ($var) {
		$last_is_variable = TRUE;
	    }
	    elsif (!$wt[0]) { # ignorable
		next if $last_is_variable;
	    }
	    else {
		$last_is_variable = FALSE;
	    }
	}
	foreach my $v (0..$lev-1) {
	    0 < $wt[$v] and push @{ $ret[$v] }, $wt[$v];
	}
    }

    # modification of tertiary weights
    if ($self->{upper_before_lower}) {
	foreach (@{ $ret[2] }) {
	    if    (0x8 <= $_ && $_ <= 0xC) { $_ -= 6 } # lower
	    elsif (0x2 <= $_ && $_ <= 0x6) { $_ += 6 } # upper
	    elsif ($_ == 0x1C)             { $_ += 1 } # square upper
	    elsif ($_ == 0x1D)             { $_ -= 1 } # square lower
	}
    }
    if ($self->{katakana_before_hiragana}) {
	foreach (@{ $ret[2] }) {
	    if    (0x0F <= $_ && $_ <= 0x13) { $_ -= 2 } # katakana
	    elsif (0x0D <= $_ && $_ <= 0x0E) { $_ += 5 } # hiragana
	}
    }

    if ($self->{backwardsFlag}) {
	for (my $v = MinLevel; $v <= MaxLevel; $v++) {
	    if ($self->{backwardsFlag} & (1 << $v)) {
		@{ $ret[$v-1] } = reverse @{ $ret[$v-1] };
	    }
	}
    }

    join LEVEL_SEP, map pack(KEY_TEMPLATE, @$_), @ret;
}


##
## int compare = cmp(string a, string b)
##
sub cmp { $_[0]->getSortKey($_[1]) cmp $_[0]->getSortKey($_[2]) }
sub eq  { $_[0]->getSortKey($_[1]) eq  $_[0]->getSortKey($_[2]) }
sub ne  { $_[0]->getSortKey($_[1]) ne  $_[0]->getSortKey($_[2]) }
sub lt  { $_[0]->getSortKey($_[1]) lt  $_[0]->getSortKey($_[2]) }
sub le  { $_[0]->getSortKey($_[1]) le  $_[0]->getSortKey($_[2]) }
sub gt  { $_[0]->getSortKey($_[1]) gt  $_[0]->getSortKey($_[2]) }
sub ge  { $_[0]->getSortKey($_[1]) ge  $_[0]->getSortKey($_[2]) }

##
## list[strings] sorted = sort(list[strings] arg)
##
sub sort {
    my $obj = shift;
    return
	map { $_->[1] }
	    sort{ $a->[0] cmp $b->[0] }
		map [ $obj->getSortKey($_), $_ ], @_;
}


sub _derivCE_9 {
    my $u = shift;
    my $base =
	(CJK_UidIni  <= $u && $u <= CJK_UidFin)
	    ? 0xFB40 : # CJK
	(CJK_ExtAIni <= $u && $u <= CJK_ExtAFin ||
	 CJK_ExtBIni <= $u && $u <= CJK_ExtBFin)
	    ? 0xFB80   # CJK ext.
	    : 0xFBC0;  # others

    my $aaaa = $base + ($u >> 15);
    my $bbbb = ($u & 0x7FFF) | 0x8000;
    return
	pack(VCE_TEMPLATE, NON_VAR, $aaaa, Min2Wt, Min3Wt, $u),
	pack(VCE_TEMPLATE, NON_VAR, $bbbb,      0,      0, $u);
}

sub _derivCE_8 {
    my $code = shift;
    my $aaaa =  0xFF80 + ($code >> 15);
    my $bbbb = ($code & 0x7FFF) | 0x8000;
    return
	pack(VCE_TEMPLATE, NON_VAR, $aaaa, 2, 1, $code),
	pack(VCE_TEMPLATE, NON_VAR, $bbbb, 0, 0, $code);
}


sub getWtHangulTerm {
    my $self = shift;
    return _varCE($self->{variable},
	pack(VCE_TEMPLATE, NON_VAR, $self->{hangul_terminator}, 0,0,0));
}


##
## "hhhh hhhh hhhh" to (dddd, dddd, dddd)
##
sub _getHexArray { map hex, $_[0] =~ /([0-9a-fA-F]+)/g }

#
# $code *must* be in Hangul syllable.
# Check it before you enter here.
#
sub _decompHangul {
    my $code = shift;
    my $SIndex = $code - Hangul_SBase;
    my $LIndex = int( $SIndex / Hangul_NCount);
    my $VIndex = int(($SIndex % Hangul_NCount) / Hangul_TCount);
    my $TIndex =      $SIndex % Hangul_TCount;
    return (
	Hangul_LBase + $LIndex,
	Hangul_VBase + $VIndex,
	$TIndex ? (Hangul_TBase + $TIndex) : (),
    );
}

sub _isIllegal {
    my $code = shift;
    return ! defined $code                      # removed
	|| ($code < 0 || 0x10FFFF < $code)      # out of range
	|| (($code & 0xFFFE) == 0xFFFE)         # ??FFF[EF] (cf. utf8.c)
	|| (0xD800 <= $code && $code <= 0xDFFF) # unpaired surrogates
	|| (0xFDD0 <= $code && $code <= 0xFDEF) # other non-characters
    ;
}

# Hangul Syllable Type
sub getHST {
    my $u = shift;
    return
	Hangul_LIni <= $u && $u <= Hangul_LFin || $u == Hangul_LFill ? "L" :
	Hangul_VIni <= $u && $u <= Hangul_VFin	     ? "V" :
	Hangul_TIni <= $u && $u <= Hangul_TFin	     ? "T" :
	Hangul_SIni <= $u && $u <= Hangul_SFin ?
	    ($u - Hangul_SBase) % Hangul_TCount ? "LVT" : "LV" : "";
}


##
## bool _nonIgnorAtLevel(arrayref weights, int level)
##
sub _nonIgnorAtLevel($$)
{
    my $wt = shift;
    return if ! defined $wt;
    my $lv = shift;
    return grep($wt->[$_-1] != 0, MinLevel..$lv) ? TRUE : FALSE;
}

##
## bool _eqArray(
##    arrayref of arrayref[weights] source,
##    arrayref of arrayref[weights] substr,
##    int level)
## * comparison of graphemes vs graphemes.
##   @$source >= @$substr must be true (check it before call this);
##
sub _eqArray($$$)
{
    my $source = shift;
    my $substr = shift;
    my $lev = shift;

    for my $g (0..@$substr-1){
	# Do the $g'th graphemes have the same number of AV weigths?
	return if @{ $source->[$g] } != @{ $substr->[$g] };

	for my $w (0..@{ $substr->[$g] }-1) {
	    for my $v (0..$lev-1) {
		return if $source->[$g][$w][$v] != $substr->[$g][$w][$v];
	    }
	}
    }
    return 1;
}

##
## (int position, int length)
## int position = index(string, substring, position, [undoc'ed grobal])
##
## With "grobal" (only for the list context),
##  returns list of arrayref[position, length].
##
sub index
{
    my $self = shift;
    my $str  = shift;
    my $len  = length($str);
    my $subE = $self->splitEnt(shift);
    my $pos  = @_ ? shift : 0;
       $pos  = 0 if $pos < 0;
    my $grob = shift;

    my $lev  = $self->{level};
    my $ver9 = $self->{UCA_Version} >= 9;
    my $v2i  = $self->{variable} ne 'non-ignorable';

    if (! @$subE) {
	my $temp = $pos <= 0 ? 0 : $len <= $pos ? $len : $pos;
	return $grob
	    ? map([$_, 0], $temp..$len)
	    : wantarray ? ($temp,0) : $temp;
    }
    $len < $pos
	and return wantarray ? () : NOMATCHPOS;
    my $strE = $self->splitEnt($pos ? substr($str, $pos) : $str, TRUE);
    @$strE
	or return wantarray ? () : NOMATCHPOS;

    my(@strWt, @iniPos, @finPos, @subWt, @g_ret);

    my $last_is_variable;
    for my $vwt (map $self->getWt($_), @$subE) {
	my($var, @wt) = unpack(VCE_TEMPLATE, $vwt);
	my $to_be_pushed = _nonIgnorAtLevel(\@wt,$lev);

	if ($v2i && $ver9) {
	    if ($var) {
		$last_is_variable = TRUE;
	    }
	    elsif (!$wt[0]) { # ignorable
		$to_be_pushed = FALSE if $last_is_variable;
	    }
	    else {
		$last_is_variable = FALSE;
	    }
	}

	if (@subWt && !$var && !$wt[0]) {
	    push @{ $subWt[-1] }, \@wt if $to_be_pushed;
	} else {
	    push @subWt, [ \@wt ];
	}
    }

    my $count = 0;
    my $end = @$strE - 1;

    $last_is_variable = FALSE; # reuse
    for (my $i = 0; $i <= $end; ) { # no $i++
	my $found_base = 0;

	# fetch a grapheme
	while ($i <= $end && $found_base == 0) {
	    for my $vwt ($self->getWt($strE->[$i][0])) {
		my($var, @wt) = unpack(VCE_TEMPLATE, $vwt);
		my $to_be_pushed = _nonIgnorAtLevel(\@wt,$lev);

		if ($v2i && $ver9) {
		    if ($var) {
			$last_is_variable = TRUE;
		    }
		    elsif (!$wt[0]) { # ignorable
			$to_be_pushed = FALSE if $last_is_variable;
		    }
		    else {
			$last_is_variable = FALSE;
		    }
		}

		if (@strWt && !$var && !$wt[0]) {
		    push @{ $strWt[-1] }, \@wt if $to_be_pushed;
		    $finPos[-1] = $strE->[$i][2];
		} elsif ($to_be_pushed) {
		    push @strWt, [ \@wt ];
		    push @iniPos, $found_base ? NOMATCHPOS : $strE->[$i][1];
		    $finPos[-1] = NOMATCHPOS if $found_base;
		    push @finPos, $strE->[$i][2];
		    $found_base++;
		}
		# else ===> no-op
	    }
	    $i++;
	}

	# try to match
	while ( @strWt > @subWt || (@strWt == @subWt && $i > $end) ) {
	    if ($iniPos[0] != NOMATCHPOS &&
		    $finPos[$#subWt] != NOMATCHPOS &&
			_eqArray(\@strWt, \@subWt, $lev)) {
		my $temp = $iniPos[0] + $pos;

		if ($grob) {
		    push @g_ret, [$temp, $finPos[$#subWt] - $iniPos[0]];
		    splice @strWt,  0, $#subWt;
		    splice @iniPos, 0, $#subWt;
		    splice @finPos, 0, $#subWt;
		}
		else {
		    return wantarray
			? ($temp, $finPos[$#subWt] - $iniPos[0])
			:  $temp;
		}
	    }
	    shift @strWt;
	    shift @iniPos;
	    shift @finPos;
	}
    }

    return $grob
	? @g_ret
	: wantarray ? () : NOMATCHPOS;
}

##
## scalarref to matching part = match(string, substring)
##
sub match
{
    my $self = shift;
    if (my($pos,$len) = $self->index($_[0], $_[1])) {
	my $temp = substr($_[0], $pos, $len);
	return wantarray ? $temp : \$temp;
	# An lvalue ref \substr should be avoided,
	# since its value is affected by modification of its referent.
    }
    else {
	return;
    }
}

##
## arrayref matching parts = gmatch(string, substring)
##
sub gmatch
{
    my $self = shift;
    my $str  = shift;
    my $sub  = shift;
    return map substr($str, $_->[0], $_->[1]),
		$self->index($str, $sub, 0, 'g');
}

##
## bool subst'ed = subst(string, substring, replace)
##
sub subst
{
    my $self = shift;
    my $code = ref $_[2] eq 'CODE' ? $_[2] : FALSE;

    if (my($pos,$len) = $self->index($_[0], $_[1])) {
	if ($code) {
	    my $mat = substr($_[0], $pos, $len);
	    substr($_[0], $pos, $len, $code->($mat));
	} else {
	    substr($_[0], $pos, $len, $_[2]);
	}
	return TRUE;
    }
    else {
	return FALSE;
    }
}

##
## int count = gsubst(string, substring, replace)
##
sub gsubst
{
    my $self = shift;
    my $code = ref $_[2] eq 'CODE' ? $_[2] : FALSE;
    my $cnt = 0;

    # Replacement is carried out from the end, then use reverse.
    for my $pos_len (reverse $self->index($_[0], $_[1], 0, 'g')) {
	if ($code) {
	    my $mat = substr($_[0], $pos_len->[0], $pos_len->[1]);
	    substr($_[0], $pos_len->[0], $pos_len->[1], $code->($mat));
	} else {
	    substr($_[0], $pos_len->[0], $pos_len->[1], $_[2]);
	}
	$cnt++;
    }
    return $cnt;
}

1;
__END__

=head1 NAME

Unicode::Collate - Unicode Collation Algorithm

=head1 SYNOPSIS

  use Unicode::Collate;

  #construct
  $Collator = Unicode::Collate->new(%tailoring);

  #sort
  @sorted = $Collator->sort(@not_sorted);

  #compare
  $result = $Collator->cmp($a, $b); # returns 1, 0, or -1.

  # If %tailoring is false (i.e. empty),
  # $Collator should do the default collation.

=head1 DESCRIPTION

This module is an implementation
of Unicode Technical Standard #10 (UTS #10)
"Unicode Collation Algorithm."

=head2 Constructor and Tailoring

The C<new> method returns a collator object.

   $Collator = Unicode::Collate->new(
      UCA_Version => $UCA_Version,
      alternate => $alternate, # deprecated: use of 'variable' is recommended.
      backwards => $levelNumber, # or \@levelNumbers
      entry => $element,
      hangul_terminator => $term_primary_weight,
      ignoreName => qr/$ignoreName/,
      ignoreChar => qr/$ignoreChar/,
      katakana_before_hiragana => $bool,
      level => $collationLevel,
      normalization  => $normalization_form,
      overrideCJK => \&overrideCJK,
      overrideHangul => \&overrideHangul,
      preprocess => \&preprocess,
      rearrange => \@charList,
      table => $filename,
      undefName => qr/$undefName/,
      undefChar => qr/$undefChar/,
      upper_before_lower => $bool,
      variable => $variable,
   );

=over 4

=item UCA_Version

If the tracking version number of the older UCA is given,
the older behavior of that tracking version is emulated on collating.
If omitted, the return value of C<UCA_Version()> is used.

The supported tracking version: 8, 9, or 11.

B<This parameter may be removed in the future version,
as switching the algorithm would affect the performance.>

=item backwards

-- see 3.1.2 French Accents, UTS #10.

     backwards => $levelNumber or \@levelNumbers

Weights in reverse order; ex. level 2 (diacritic ordering) in French.
If omitted, forwards at all the levels.

=item entry

-- see 3.1 Linguistic Features; 3.2.1 File Format, UTS #10.

If the same character (or a sequence of characters) exists
in the collation element table through C<table>,
mapping to collation elements is overrided.
If it does not exist, the mapping is defined additionally.

    entry => <<'ENTRY', # for DUCET v4.0.0 (allkeys-4.0.0.txt)
0063 0068 ; [.0E6A.0020.0002.0063] # ch
0043 0068 ; [.0E6A.0020.0007.0043] # Ch
0043 0048 ; [.0E6A.0020.0008.0043] # CH
006C 006C ; [.0F4C.0020.0002.006C] # ll
004C 006C ; [.0F4C.0020.0007.004C] # Ll
004C 004C ; [.0F4C.0020.0008.004C] # LL
006E 0303 ; [.0F7B.0020.0002.006E] # n-tilde
004E 0303 ; [.0F7B.0020.0008.004E] # N-tilde
ENTRY

    entry => <<'ENTRY', # for DUCET v4.0.0 (allkeys-4.0.0.txt)
00E6 ; [.0E33.0020.0002.00E6][.0E8B.0020.0002.00E6] # ae ligature as <a><e>
00C6 ; [.0E33.0020.0008.00C6][.0E8B.0020.0008.00C6] # AE ligature as <A><E>
ENTRY

B<NOTE:> The code point in the UCA file format (before C<';'>)
B<must> be a Unicode code point (defined as hexadecimal),
but not a native code point.
So C<0063> must always denote C<U+0063>,
but not a character of C<"\x63">.

Weighting may vary depending on collation element table.
So ensure the weights defined in C<entry> will be consistent with
those in the collation element table loaded via C<table>.

In DUCET v4.0.0, primary weight of C<C> is C<0E60>
and that of C<D> is C<0E6D>. So setting primary weight of C<CH> to C<0E6A>
(as a value between C<0E60> and C<0E6D>)
makes ordering as C<C E<lt> CH E<lt> D>.
Exactly speaking DUCET already has some characters between C<C> and C<D>:
C<small capital C> (C<U+1D04>) with primary weight C<0E64>,
C<c-hook/C-hook> (C<U+0188/U+0187>) with C<0E65>,
and C<c-curl> (C<U+0255>) with C<0E69>.
Then primary weight C<0E6A> for C<CH> makes C<CH>
ordered between C<c-curl> and C<D>.

=item hangul_terminator

-- see Condition B.2. in 7.1.4 Trailing Weights, UTS #10.

If a true value is given (non-zero but should be positive),
it will be added as a terminator primary weight to the end of
every standard Hangul syllable. Secondary and any higher weights
for terminator are set to zero.
If the value is false or C<hangul_terminator> key does not exist,
insertion of terminator weights will not be performed.

Boundaries of Hangul syllables are determined
according to conjoining Jamo behavior in F<the Unicode Standard>
and F<HangulSyllableType.txt>.

B<Implementation Note:>
(1) For expansion mapping (Unicode character mapped
to a sequence of collation elements), a terminator will not be added
between collation elements, even if Hangul syllable boundary exists there.
Addition of terminator is restricted to the next position
to the last collation element.

(2) Non-conjoining Hangul letters
(Compatibility Jamo, halfwidth Jamo, and enclosed letters) are not
automatically terminated with a terminator primary weight.
These characters may need terminator included in a collation element
table beforehand.

=item ignoreName

=item ignoreChar

-- see Completely Ignorable, 3.2.2 Variable Weighting, UTS #10.

Makes the entry in the table completely ignorable;
i.e. as if the weights were zero at all level.

E.g. when 'a' and 'e' are ignorable,
'element' is equal to 'lament' (or 'lmnt').

=item level

-- see 4.3 Form a sort key for each string, UTS #10.

Set the maximum level.
Any higher levels than the specified one are ignored.

  Level 1: alphabetic ordering
  Level 2: diacritic ordering
  Level 3: case ordering
  Level 4: tie-breaking (e.g. in the case when variable is 'shifted')

  ex.level => 2,

If omitted, the maximum is the 4th.

=item normalization

-- see 4.1 Normalize each input string, UTS #10.

If specified, strings are normalized before preparation of sort keys
(the normalization is executed after preprocess).

A form name C<Unicode::Normalize::normalize()> accepts will be applied
as C<$normalization_form>.
Acceptable names include C<'NFD'>, C<'NFC'>, C<'NFKD'>, and C<'NFKC'>.
See C<Unicode::Normalize::normalize()> for detail.
If omitted, C<'NFD'> is used.

C<normalization> is performed after C<preprocess> (if defined).

Furthermore, special values, C<undef> and C<"prenormalized">, can be used,
though they are not concerned with C<Unicode::Normalize::normalize()>.

If C<undef> (not a string C<"undef">) is passed explicitly
as the value for this key,
any normalization is not carried out (this may make tailoring easier
if any normalization is not desired). Under C<(normalization =E<gt> undef)>,
only contiguous contractions are resolved;
e.g. even if C<A-ring> (and C<A-ring-cedilla>) is ordered after C<Z>,
C<A-cedilla-ring> would be primary equal to C<A>.
In this point,
C<(normalization =E<gt> undef, preprocess =E<gt> sub { NFD(shift) })>
B<is not> equivalent to C<(normalization =E<gt> 'NFD')>.

In the case of C<(normalization =E<gt> "prenormalized")>,
any normalization is not performed, but
non-contiguous contractions with combining characters are performed.
Therefore
C<(normalization =E<gt> 'prenormalized', preprocess =E<gt> sub { NFD(shift) })>
B<is> equivalent to C<(normalization =E<gt> 'NFD')>.
If source strings are finely prenormalized,
C<(normalization =E<gt> 'prenormalized')> may save time for normalization.

Except C<(normalization =E<gt> undef)>,
B<Unicode::Normalize> is required (see also B<CAVEAT>).

=item overrideCJK

-- see 7.1 Derived Collation Elements, UTS #10.

By default, CJK Unified Ideographs are ordered in Unicode codepoint order
(but C<CJK Unified Ideographs> [C<U+4E00> to C<U+9FA5>]  are lesser than
C<CJK Unified Ideographs Extension> [C<U+3400> to C<U+4DB5> and
C<U+20000> to C<U+2A6D6>].

Through C<overrideCJK>, ordering of CJK Unified Ideographs can be overrided.

ex. CJK Unified Ideographs in the JIS code point order.

  overrideCJK => sub {
      my $u = shift;             # get a Unicode codepoint
      my $b = pack('n', $u);     # to UTF-16BE
      my $s = your_unicode_to_sjis_converter($b); # convert
      my $n = unpack('n', $s);   # convert sjis to short
      [ $n, 0x20, 0x2, $u ];     # return the collation element
  },

ex. ignores all CJK Unified Ideographs.

  overrideCJK => sub {()}, # CODEREF returning empty list

   # where ->eq("Pe\x{4E00}rl", "Perl") is true
   # as U+4E00 is a CJK Unified Ideograph and to be ignorable.

If C<undef> is passed explicitly as the value for this key,
weights for CJK Unified Ideographs are treated as undefined.
But assignment of weight for CJK Unified Ideographs
in table or C<entry> is still valid.

=item overrideHangul

-- see 7.1 Derived Collation Elements, UTS #10.

By default, Hangul Syllables are decomposed into Hangul Jamo,
even if C<(normalization =E<gt> undef)>.
But the mapping of Hangul Syllables may be overrided.

This tag works like C<overrideCJK>, so see there for examples.

If you want to override the mapping of Hangul Syllables,
NFD, NFKD, and FCD are not appropriate,
since they will decompose Hangul Syllables before overriding.

If C<undef> is passed explicitly as the value for this key,
weight for Hangul Syllables is treated as undefined
without decomposition into Hangul Jamo.
But definition of weight for Hangul Syllables
in table or C<entry> is still valid.

=item preprocess

-- see 5.1 Preprocessing, UTS #10.

If specified, the coderef is used to preprocess
before the formation of sort keys.

ex. dropping English articles, such as "a" or "the".
Then, "the pen" is before "a pencil".

     preprocess => sub {
           my $str = shift;
           $str =~ s/\b(?:an?|the)\s+//gi;
           return $str;
        },

C<preprocess> is performed before C<normalization> (if defined).

=item rearrange

-- see 3.1.3 Rearrangement, UTS #10.

Characters that are not coded in logical order and to be rearranged.
By default,

    rearrange => [ 0x0E40..0x0E44, 0x0EC0..0x0EC4 ],

If you want to disallow any rearrangement,
pass C<undef> or C<[]> (a reference to an empty list)
as the value for this key.

B<According to the version 9 of UCA, this parameter shall not be used;
but it is not warned at present.>

=item table

-- see 3.2 Default Unicode Collation Element Table, UTS #10.

You can use another collation element table if desired.
The table file must be put into a directory
where F<Unicode/Collate.pm> is installed; e.g. into
F<perl/lib/Unicode/Collate/> if you have F<perl/lib/Unicode/Collate.pm>.

By default, the filename F<allkeys.txt> is used.

If C<undef> is passed explicitly as the value for this key,
no file is read (but you can define collation elements via C<entry>).

A typical way to define a collation element table
without any file of table:

   $onlyABC = Unicode::Collate->new(
       table => undef,
       entry => << 'ENTRIES',
0061 ; [.0101.0020.0002.0061] # LATIN SMALL LETTER A
0041 ; [.0101.0020.0008.0041] # LATIN CAPITAL LETTER A
0062 ; [.0102.0020.0002.0062] # LATIN SMALL LETTER B
0042 ; [.0102.0020.0008.0042] # LATIN CAPITAL LETTER B
0063 ; [.0103.0020.0002.0063] # LATIN SMALL LETTER C
0043 ; [.0103.0020.0008.0043] # LATIN CAPITAL LETTER C
ENTRIES
    );

=item undefName

=item undefChar

-- see 6.3.4 Reducing the Repertoire, UTS #10.

Undefines the collation element as if it were unassigned in the table.
This reduces the size of the table.
If an unassigned character appears in the string to be collated,
the sort key is made from its codepoint
as a single-character collation element,
as it is greater than any other assigned collation elements
(in the codepoint order among the unassigned characters).
But, it'd be better to ignore characters
unfamiliar to you and maybe never used.

=item katakana_before_hiragana

=item upper_before_lower

-- see 6.6 Case Comparisons; 7.3.1 Tertiary Weight Table, UTS #10.

By default, lowercase is before uppercase
and hiragana is before katakana.

If the tag is made true, this is reversed.

B<NOTE>: These tags simplemindedly assume
any lowercase/uppercase or hiragana/katakana distinctions
must occur in level 3, and their weights at level 3
must be same as those mentioned in 7.3.1, UTS #10.
If you define your collation elements which violate this requirement,
these tags don't work validly.

=item variable

=item alternate

-- see 3.2.2 Variable Weighting, UTS #10.

(the title in UCA version 8: Alternate Weighting)

This key allows to variable weighting for variable collation elements,
which are marked with an ASTERISK in the table
(NOTE: Many punction marks and symbols are variable in F<allkeys.txt>).

   variable => 'blanked', 'non-ignorable', 'shifted', or 'shift-trimmed'.

These names are case-insensitive.
By default (if specification is omitted), 'shifted' is adopted.

   'Blanked'        Variable elements are made ignorable at levels 1 through 3;
                    considered at the 4th level.

   'Non-Ignorable'  Variable elements are not reset to ignorable.

   'Shifted'        Variable elements are made ignorable at levels 1 through 3
                    their level 4 weight is replaced by the old level 1 weight.
                    Level 4 weight for Non-Variable elements is 0xFFFF.

   'Shift-Trimmed'  Same as 'shifted', but all FFFF's at the 4th level
                    are trimmed.

For backward compatibility, C<alternate> can be used as an alias
for C<variable>.

=back

=head2 Methods for Collation

=over 4

=item C<@sorted = $Collator-E<gt>sort(@not_sorted)>

Sorts a list of strings.

=item C<$result = $Collator-E<gt>cmp($a, $b)>

Returns 1 (when C<$a> is greater than C<$b>)
or 0 (when C<$a> is equal to C<$b>)
or -1 (when C<$a> is lesser than C<$b>).

=item C<$result = $Collator-E<gt>eq($a, $b)>

=item C<$result = $Collator-E<gt>ne($a, $b)>

=item C<$result = $Collator-E<gt>lt($a, $b)>

=item C<$result = $Collator-E<gt>le($a, $b)>

=item C<$result = $Collator-E<gt>gt($a, $b)>

=item C<$result = $Collator-E<gt>ge($a, $b)>

They works like the same name operators as theirs.

   eq : whether $a is equal to $b.
   ne : whether $a is not equal to $b.
   lt : whether $a is lesser than $b.
   le : whether $a is lesser than $b or equal to $b.
   gt : whether $a is greater than $b.
   ge : whether $a is greater than $b or equal to $b.

=item C<$sortKey = $Collator-E<gt>getSortKey($string)>

-- see 4.3 Form a sort key for each string, UTS #10.

Returns a sort key.

You compare the sort keys using a binary comparison
and get the result of the comparison of the strings using UCA.

   $Collator->getSortKey($a) cmp $Collator->getSortKey($b)

      is equivalent to

   $Collator->cmp($a, $b)

=item C<$sortKeyForm = $Collator-E<gt>viewSortKey($string)>

   use Unicode::Collate;
   my $c = Unicode::Collate->new();
   print $c->viewSortKey("Perl"),"\n";

   # output:
   # [0B67 0A65 0B7F 0B03 | 0020 0020 0020 0020 | 0008 0002 0002 0002 | FFFF FFFF FFFF FFFF]
   #  Level 1               Level 2               Level 3               Level 4

    (If C<UCA_Version> is 8, the output is slightly different.)

=back

=head2 Methods for Searching

B<DISCLAIMER:> If C<preprocess> or C<normalization> tag is true
for C<$Collator>, calling these methods (C<index>, C<match>, C<gmatch>,
C<subst>, C<gsubst>) is croaked,
as the position and the length might differ
from those on the specified string.
(And C<rearrange> and C<hangul_terminator> tags are neglected.)

The C<match>, C<gmatch>, C<subst>, C<gsubst> methods work
like C<m//>, C<m//g>, C<s///>, C<s///g>, respectively,
but they are not aware of any pattern, but only a literal substring.

=over 4

=item C<$position = $Collator-E<gt>index($string, $substring[, $position])>

=item C<($position, $length) = $Collator-E<gt>index($string, $substring[, $position])>

If C<$substring> matches a part of C<$string>, returns
the position of the first occurrence of the matching part in scalar context;
in list context, returns a two-element list of
the position and the length of the matching part.

If C<$substring> does not match any part of C<$string>,
returns C<-1> in scalar context and
an empty list in list context.

e.g. you say

  my $Collator = Unicode::Collate->new( normalization => undef, level => 1 );
                                     # (normalization => undef) is REQUIRED.
  my $str = "Ich muß studieren Perl.";
  my $sub = "MÜSS";
  my $match;
  if (my($pos,$len) = $Collator->index($str, $sub)) {
      $match = substr($str, $pos, $len);
  }

and get C<"muß"> in C<$match> since C<"muß">
is primary equal to C<"MÜSS">. 

=item C<$match_ref = $Collator-E<gt>match($string, $substring)>

=item C<($match)   = $Collator-E<gt>match($string, $substring)>

If C<$substring> matches a part of C<$string>, in scalar context, returns
B<a reference to> the first occurrence of the matching part
(C<$match_ref> is always true if matches,
since every reference is B<true>);
in list context, returns the first occurrence of the matching part.

If C<$substring> does not match any part of C<$string>,
returns C<undef> in scalar context and
an empty list in list context.

e.g.

    if ($match_ref = $Collator->match($str, $sub)) { # scalar context
	print "matches [$$match_ref].\n";
    } else {
	print "doesn't match.\n";
    }

     or 

    if (($match) = $Collator->match($str, $sub)) { # list context
	print "matches [$match].\n";
    } else {
	print "doesn't match.\n";
    }

=item C<@match = $Collator-E<gt>gmatch($string, $substring)>

If C<$substring> matches a part of C<$string>, returns
all the matching parts (or matching count in scalar context).

If C<$substring> does not match any part of C<$string>,
returns an empty list.

=item C<$count = $Collator-E<gt>subst($string, $substring, $replacement)>

If C<$substring> matches a part of C<$string>,
the first occurrence of the matching part is replaced by C<$replacement>
(C<$string> is modified) and return C<$count> (always equals to C<1>).

C<$replacement> can be a C<CODEREF>,
taking the matching part as an argument,
and returning a string to replace the matching part
(a bit similar to C<s/(..)/$coderef-E<gt>($1)/e>).

=item C<$count = $Collator-E<gt>gsubst($string, $substring, $replacement)>

If C<$substring> matches a part of C<$string>,
all the occurrences of the matching part is replaced by C<$replacement>
(C<$string> is modified) and return C<$count>.

C<$replacement> can be a C<CODEREF>,
taking the matching part as an argument,
and returning a string to replace the matching part
(a bit similar to C<s/(..)/$coderef-E<gt>($1)/eg>).

e.g.

  my $Collator = Unicode::Collate->new( normalization => undef, level => 1 );
                                     # (normalization => undef) is REQUIRED.
  my $str = "Camel ass came\x{301}l CAMEL horse cAm\0E\0L...";
  $Collator->gsubst($str, "camel", sub { "<b>$_[0]</b>" });

  # now $str is "<b>Camel</b> ass <b>came\x{301}l</b> <b>CAMEL</b> horse <b>cAm\0E\0L</b>...";
  # i.e., all the camels are made bold-faced.

=back

=head2 Other Methods

=over 4

=item C<%old_tailoring = $Collator-E<gt>change(%new_tailoring)>

Change the value of specified keys and returns the changed part.

    $Collator = Unicode::Collate->new(level => 4);

    $Collator->eq("perl", "PERL"); # false

    %old = $Collator->change(level => 2); # returns (level => 4).

    $Collator->eq("perl", "PERL"); # true

    $Collator->change(%old); # returns (level => 2).

    $Collator->eq("perl", "PERL"); # false

Not all C<(key,value)>s are allowed to be changed.
See also C<@Unicode::Collate::ChangeOK> and C<@Unicode::Collate::ChangeNG>.

In the scalar context, returns the modified collator
(but it is B<not> a clone from the original).

    $Collator->change(level => 2)->eq("perl", "PERL"); # true

    $Collator->eq("perl", "PERL"); # true; now max level is 2nd.

    $Collator->change(level => 4)->eq("perl", "PERL"); # false

=item C<$version = $Collator-E<gt>version()>

Returns the version number (a string) of the Unicode Standard
which the C<table> file used by the collator object is based on.
If the table does not include a version line (starting with C<@version>),
returns C<"unknown">.

=item C<UCA_Version()>

Returns the tracking version number of UTS #10 this module consults.

=item C<Base_Unicode_Version()>

Returns the version number of UTS #10 this module consults.

=back

=head2 EXPORT

None by default.

=head2 CAVEAT

Use of the C<normalization> parameter requires
the B<Unicode::Normalize> module.

If you need not it (say, in the case when you need not
handle any combining characters),
assign C<normalization =E<gt> undef> explicitly.

-- see 6.5 Avoiding Normalization, UTS #10.

=head2 Conformance Test

The Conformance Test for the UCA is available
under L<http://www.unicode.org/Public/UCA/>.

For F<CollationTest_SHIFTED.txt>,
a collator via C<Unicode::Collate-E<gt>new( )> should be used;
for F<CollationTest_NON_IGNORABLE.txt>, a collator via
C<Unicode::Collate-E<gt>new(variable =E<gt> "non-ignorable", level =E<gt> 3)>.

B<Unicode::Normalize is required to try The Conformance Test.>

=head1 AUTHOR

SADAHIRO Tomoyuki <SADAHIRO@cpan.org>

  http://homepage1.nifty.com/nomenclator/perl/

  Copyright(C) 2001-2003, SADAHIRO Tomoyuki. Japan. All rights reserved.

  This library is free software; you can redistribute it
  and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

=over 4

=item Unicode Collation Algorithm - UTS #10

L<http://www.unicode.org/reports/tr10/>

=item The Default Unicode Collation Element Table (DUCET)

L<http://www.unicode.org/Public/UCA/latest/allkeys.txt>

=item The conformance test for the UCA

L<http://www.unicode.org/Public/UCA/latest/CollationTest.html>

L<http://www.unicode.org/Public/UCA/latest/CollationTest.zip>

=item Hangul Syllable Type

L<http://www.unicode.org/Public/UNIDATA/HangulSyllableType.txt>

=item Unicode Normalization Forms - UAX #15

L<http://www.unicode.org/reports/tr15/>

=item L<Unicode::Normalize>

=back

=cut
