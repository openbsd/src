package Unicode::Collate;

BEGIN {
    if (ord("A") == 193) {
	die "Unicode::Collate not ported to EBCDIC\n";
    }
}

use 5.006;
use strict;
use warnings;
use Carp;
use File::Spec;

require Exporter;

our $VERSION = '0.12';
our $PACKAGE = __PACKAGE__;

our @ISA = qw(Exporter);

our %EXPORT_TAGS = ();
our @EXPORT_OK = ();
our @EXPORT = ();

(our $Path = $INC{'Unicode/Collate.pm'}) =~ s/\.pm$//;
our $KeyFile = "allkeys.txt";

our $UNICODE_VERSION;

eval { require Unicode::UCD };

unless ($@) {
    $UNICODE_VERSION = Unicode::UCD::UnicodeVersion();
}
else { # XXX, Perl 5.6.1
    my($f, $fh);
    foreach my $d (@INC) {
	use File::Spec;
	$f = File::Spec->catfile($d, "unicode", "Unicode.301");
	if (open($fh, $f)) {
	    $UNICODE_VERSION = '3.0.1';
	    close $fh;
	    last;
	}
    }
}

our $getCombinClass; # coderef for combining class from Unicode::Normalize

use constant Min2      => 0x20;   # minimum weight at level 2
use constant Min3      => 0x02;   # minimum weight at level 3
use constant UNDEFINED => 0xFF80; # special value for undefined CE's

our $DefaultRearrange = [ 0x0E40..0x0E44, 0x0EC0..0x0EC4 ];

sub UCA_Version { "8.0" }

sub Base_Unicode_Version { $UNICODE_VERSION || 'unknown' }

##
## constructor
##
sub new
{
    my $class = shift;
    my $self = bless { @_ }, $class;

    # alternate lowercased
    $self->{alternate} =
	! exists $self->{alternate} ? 'shifted' : lc($self->{alternate});

    croak "$PACKAGE unknown alternate tag name: $self->{alternate}"
	unless $self->{alternate} eq 'blanked'
	    || $self->{alternate} eq 'non-ignorable'
	    || $self->{alternate} eq 'shifted'
	    || $self->{alternate} eq 'shift-trimmed';

    # collation level
    $self->{level} ||= 4;

    croak "Illegal level lower than 1 (passed $self->{level})."
	if $self->{level} < 1;
    croak "A level higher than 4 (passed $self->{level}) is not supported."
	if 4 < $self->{level};

    # overrideHangul and -CJK
    # If true: CODEREF used; '': default; undef: derived elements
    $self->{overrideHangul} = ''
	if ! exists $self->{overrideHangul};
    $self->{overrideCJK} = ''
	if ! exists $self->{overrideCJK};

    # normalization form
    $self->{normalization} = 'D'
	if ! exists $self->{normalization};
    $self->{UNF} = undef;

    if (defined $self->{normalization}) {
	eval { require Unicode::Normalize };
	croak "Unicode/Normalize.pm is required to normalize strings: $@"
	    if $@;

	Unicode::Normalize->import();
	$getCombinClass = \&Unicode::Normalize::getCombinClass
	    if ! $getCombinClass;

	$self->{UNF} =
	    $self->{normalization} =~ /^(?:NF)?C$/  ? \&NFC :
	    $self->{normalization} =~ /^(?:NF)?D$/  ? \&NFD :
	    $self->{normalization} =~ /^(?:NF)?KC$/ ? \&NFKC :
	    $self->{normalization} =~ /^(?:NF)?KD$/ ? \&NFKD :
	  croak "$PACKAGE unknown normalization form name: "
		. $self->{normalization};
    }

    # Open a table file.
    # If undef is passed explicitly, no file is read.
    $self->{table} = $KeyFile
	if ! exists $self->{table};
    $self->read_table
	if defined $self->{table};

    if ($self->{entry}) {
	$self->parseEntry($_) foreach split /\n/, $self->{entry};
    }

    # backwards
    $self->{backwards} ||= [ ];
    $self->{backwards} = [ $self->{backwards} ]
	if ! ref $self->{backwards};

    # rearrange
    $self->{rearrange} = $DefaultRearrange
	if ! exists $self->{rearrange};
    $self->{rearrange} = []
	if ! defined $self->{rearrange};
    croak "$PACKAGE: A list for rearrangement must be store in an ARRAYREF"
	if ! ref $self->{rearrange};

    # keys of $self->{rearrangeHash} are $self->{rearrange}.
    $self->{rearrangeHash} = undef;

    if (@{ $self->{rearrange} }) {
	@{ $self->{rearrangeHash} }{ @{ $self->{rearrange} } } = ();
    }

    return $self;
}

sub read_table {
    my $self = shift;
    my $file = $self->{table} ne '' ? $self->{table} : $KeyFile;

    my $filepath = File::Spec->catfile($Path, $file);
    open my $fk, "<$filepath"
	or croak "File does not exist at $filepath";

    while (<$fk>) {
	next if /^\s*#/;
	if (/^\s*\@/) {
	    if (/^\@version\s*(\S*)/) {
		$self->{version} ||= $1;
	    }
	    elsif (/^\@alternate\s+(.*)/) {
		$self->{alternate} ||= $1;
	    }
	    elsif (/^\@backwards\s+(.*)/) {
		push @{ $self->{backwards} }, $1;
	    }
	    elsif (/^\@rearrange\s+(.*)/) {
		push @{ $self->{rearrange} }, _getHexArray($1);
	    }
	    next;
	}
	$self->parseEntry($_);
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
    my($name, $ele, @key);

    return if $line !~ /^\s*[0-9A-Fa-f]/;

    # removes comment and gets name
    $name = $1
	if $line =~ s/[#%]\s*(.*)//;
    return if defined $self->{undefName} && $name =~ /$self->{undefName}/;

    # gets element
    my($e, $k) = split /;/, $line;
    croak "Wrong Entry: <charList> must be separated by ';' from <collElement>"
	if ! $k;

    my @e = _getHexArray($e);
    $ele = pack('U*', @e);
    return if defined $self->{undefChar} && $ele =~ /$self->{undefChar}/;

    # get sort key
    if (defined $self->{ignoreName} && $name =~ /$self->{ignoreName}/ ||
	defined $self->{ignoreChar} && $ele  =~ /$self->{ignoreChar}/)
    {
	$self->{entries}{$ele} = $self->{ignored}{$ele} = 1;
    }
    else {
	my $combining = 1; # primary = 0, secondary != 0;

	foreach my $arr ($k =~ /\[([^\[\]]+)\]/g) { # SPACEs allowed
	    my $var = $arr =~ /\*/; # exactly /^\*/ but be lenient.
	    push @key, $self->altCE($var, _getHexArray($arr));
	    $combining = 0 unless $key[-1][0] == 0 && $key[-1][1] != 0;
	}
	$self->{entries}{$ele} = \@key;
	$self->{combining}{$ele} = 1 if $combining;
    }
    $self->{maxlength}{ord $ele} = scalar @e if @e > 1;
}


##
## arrayref CE = altCE(bool variable?, list[num] weights)
##
sub altCE
{
    my $self = shift;
    my $var  = shift;
    my @c    = @_;

    $self->{alternate} eq 'blanked' ?
	$var ? [0,0,0,$c[3]] : \@c :
    $self->{alternate} eq 'non-ignorable' ?
	\@c :
    $self->{alternate} eq 'shifted' ?
	$var ? [0,0,0,$c[0] ] : [ @c[0..2], $c[0]+$c[1]+$c[2] ? 0xFFFF : 0 ] :
    $self->{alternate} eq 'shift-trimmed' ?
	$var ? [0,0,0,$c[0] ] : [ @c[0..2], 0 ] :
        croak "$PACKAGE unknown alternate name: $self->{alternate}";
}

##
## string hex_sortkey = splitCE(string arg)
##
sub viewSortKey
{
    my $self = shift;
    my $key  = $self->getSortKey(@_);
    my $view = join " ", map sprintf("%04X", $_), unpack 'n*', $key;
    $view =~ s/ ?0000 ?/|/g;
    return "[$view]";
}


##
## list[strings] elements = splitCE(string arg)
##
sub splitCE
{
    my $self = shift;
    my $code = $self->{preprocess};
    my $norm = $self->{UNF};
    my $ent  = $self->{entries};
    my $max  = $self->{maxlength};
    my $reH  = $self->{rearrangeHash};

    my $str = ref $code ? &$code(shift) : shift;
    $str = &$norm($str) if ref $norm;

    my @src = unpack('U*', $str);
    my @buf;

    # rearrangement
    if ($reH) {
	for (my $i = 0; $i < @src; $i++) {
	    if (exists $reH->{ $src[$i] } && $i + 1 < @src) {
		($src[$i], $src[$i+1]) = ($src[$i+1], $src[$i]);
		$i++;
	    }
	}
    }

    for (my $i = 0; $i < @src; $i++) {
	my $ch;
	my $u = $src[$i];

	# non-characters
	next unless defined $u;
	next if $u < 0 || 0x10FFFF < $u    # out of range
	    || (0xD800 <= $u && $u <= 0xDFFF); # unpaired surrogates
	my $four = $u & 0xFFFF; 
	next if $four == 0xFFFE || $four == 0xFFFF;

	if ($max->{$u}) { # contract
	    for (my $j = $max->{$u}; $j >= 1; $j--) {
		next unless $i+$j-1 < @src;
		$ch = pack 'U*', @src[$i .. $i+$j-1];
		$i += $j-1, last if $ent->{$ch};
	    }
	} else {
	    $ch = pack('U', $u);
	}

	# with Combining Char (UTS#10, 4.2.1), here requires Unicode::Normalize.
	if ($getCombinClass && defined $ch) {
	    for (my $j = $i+1; $j < @src; $j++) {
		next unless defined $src[$j];
		last unless $getCombinClass->( $src[$j] );
		my $comb = pack 'U', $src[$j];
		next if ! $ent->{ $ch.$comb };
		$ch .= $comb;
		$src[$j] = undef;
	    }
	}
	push @buf, $ch;
    }
    wantarray ? @buf : \@buf;
}


##
## list[arrayrefs] weight = getWt(string element)
##
sub getWt
{
    my $self = shift;
    my $ch   = shift;
    my $ent  = $self->{entries};
    my $ign  = $self->{ignored};
    my $cjk  = $self->{overrideCJK};
    my $hang = $self->{overrideHangul};

    return if !defined $ch || $ign->{$ch}; # ignored
    return @{ $ent->{$ch} } if $ent->{$ch};
    my $u = unpack('U', $ch);

    if (0xAC00 <= $u && $u <= 0xD7A3) { # is_Hangul
	return $hang
	    ? &$hang($u)
	    : defined $hang
		? map({
			my $v = $_;
			my $ar = $ent->{pack('U', $v)};
			$ar ? @$ar : map($self->altCE(0,@$_), _derivCE($v));
		    } _decompHangul($u))
		: map($self->altCE(0,@$_), _derivCE($u));
    }
    elsif (0x3400 <= $u && $u <= 0x4DB5 ||
	   0x4E00 <= $u && $u <= 0x9FA5 ||
	   0x20000 <= $u && $u <= 0x2A6D6) { # is_CJK
	return $cjk
	    ? &$cjk($u)
	    : defined $cjk && $u <= 0xFFFF
		? $self->altCE(0, ($u, 0x20, 0x02, $u))
		: map($self->altCE(0,@$_), _derivCE($u));
    }
    else {
	return map($self->altCE(0,@$_), _derivCE($u));
    }
}

##
## int = index(string, substring)
##
sub index
{
    my $self = shift;
    my $lev  = $self->{level};
    my $comb = $self->{combining};
    my $str  = $self->splitCE(shift);
    my $sub  = $self->splitCE(shift);

    return wantarray ? (0,0) : 0 if ! @$sub;
    return wantarray ?  ()  : -1 if ! @$str;

    my @subWt = grep _ignorableAtLevel($_,$lev),
		map $self->getWt($_), @$sub;

    my(@strWt,@strPt);
    my $count = 0;
    for (my $i = 0; $i < @$str; $i++) {
	my $go_ahead = 0;

	my @tmp = grep _ignorableAtLevel($_,$lev), $self->getWt($str->[$i]);
	$go_ahead += length $str->[$i];

	# /*XXX*/ still broken.
	# index("e\x{300}", "e") should be 'no match' at level 2 or higher
	# as "e\x{300}" is a *single* grapheme cluster and not equal to "e".

	# go ahead as far as we find a combining character;
	while ($i + 1 < @$str &&
	      (! defined $str->[$i+1] || $comb->{ $str->[$i+1] }) ) {
	    $i++;
	    $go_ahead += length $str->[$i];
	    next if ! defined $str->[$i];
	    push @tmp,
		grep _ignorableAtLevel($_,$lev), $self->getWt($str->[$i]);
	}

	push @strWt, @tmp;
	push @strPt, ($count) x @tmp;
	$count += $go_ahead;

	while (@strWt >= @subWt) {
	    if (_eqArray(\@strWt, \@subWt, $lev)) {
		my $pos = $strPt[0];
		return wantarray ? ($pos, $count-$pos) : $pos;
	    }
	    shift @strWt;
	    shift @strPt;
	}
    }
    return wantarray ? () : -1;
}

##
## bool _eqArray(arrayref, arrayref, level)
##
sub _eqArray($$$)
{
    my $a   = shift; # length $a >= length $b;
    my $b   = shift;
    my $lev = shift;
    for my $v (0..$lev-1) {
	for my $c (0..@$b-1){
	    return if $a->[$c][$v] != $b->[$c][$v];
	}
    }
    return 1;
}


##
## bool _ignorableAtLevel(CE, level)
##
sub _ignorableAtLevel($$)
{
    my $ce = shift;
    return unless defined $ce;
    my $lv = shift;
    return ! grep { ! $ce->[$_] } 0..$lv-1;
}


##
## string sortkey = getSortKey(string arg)
##
sub getSortKey
{
    my $self = shift;
    my $lev  = $self->{level};
    my $rCE  = $self->splitCE(shift); # get an arrayref

    # weight arrays
    my @buf = grep defined(), map $self->getWt($_), @$rCE;

    # make sort key
    my @ret = ([],[],[],[]);
    foreach my $v (0..$lev-1) {
	foreach my $b (@buf) {
	    push @{ $ret[$v] }, $b->[$v] if $b->[$v];
	}
    }
    foreach (@{ $self->{backwards} }) {
	my $v = $_ - 1;
	@{ $ret[$v] } = reverse @{ $ret[$v] };
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
    join "\0\0", map pack('n*', @$_), @ret;
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

##
## list[arrayrefs] CE = _derivCE(int codepoint)
##
sub _derivCE {
    my $code = shift;
    my $a = UNDEFINED + ($code >> 15); # ok
    my $b = ($code & 0x7FFF) | 0x8000; # ok
#   my $a = 0xFFC2 + ($code >> 15);    # ng
#   my $b = $code & 0x7FFF | 0x1000;   # ng
    $b ? ([$a,2,1,$code],[$b,0,0,$code]) : [$a,2,1,$code];
}

##
## "hhhh hhhh hhhh" to (dddd, dddd, dddd)
##
sub _getHexArray { map hex, $_[0] =~ /([0-9a-fA-F]+)/g }

#
# $code must be in Hangul syllable.
# Check it before you enter here.
#
sub _decompHangul {
    my $code = shift;
    my $SIndex = $code - 0xAC00;
    my $LIndex = int( $SIndex / 588);
    my $VIndex = int(($SIndex % 588) / 28);
    my $TIndex =      $SIndex % 28;
    return (
	0x1100 + $LIndex,
	0x1161 + $VIndex,
	$TIndex ? (0x11A7 + $TIndex) : (),
    );
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

=head1 DESCRIPTION

=head2 Constructor and Tailoring

The C<new> method returns a collator object.

   $Collator = Unicode::Collate->new(
      alternate => $alternate,
      backwards => $levelNumber, # or \@levelNumbers
      entry => $element,
      normalization  => $normalization_form,
      ignoreName => qr/$ignoreName/,
      ignoreChar => qr/$ignoreChar/,
      katakana_before_hiragana => $bool,
      level => $collationLevel,
      overrideCJK => \&overrideCJK,
      overrideHangul => \&overrideHangul,
      preprocess => \&preprocess,
      rearrange => \@charList,
      table => $filename,
      undefName => qr/$undefName/,
      undefChar => qr/$undefChar/,
      upper_before_lower => $bool,
   );
   # if %tailoring is false (i.e. empty),
   # $Collator should do the default collation.

=over 4

=item alternate

-- see 3.2.2 Alternate Weighting, UTR #10.

This key allows to alternate weighting for variable collation elements,
which are marked with an ASTERISK in the table
(NOTE: Many punction marks and symbols are variable in F<allkeys.txt>).

   alternate => 'blanked', 'non-ignorable', 'shifted', or 'shift-trimmed'.

These names are case-insensitive.
By default (if specification is omitted), 'shifted' is adopted.

   'Blanked'        Variable elements are ignorable at levels 1 through 3;
                    considered at the 4th level.

   'Non-ignorable'  Variable elements are not reset to ignorable.

   'Shifted'        Variable elements are ignorable at levels 1 through 3
                    their level 4 weight is replaced by the old level 1 weight.
                    Level 4 weight for Non-Variable elements is 0xFFFF.

   'Shift-Trimmed'  Same as 'shifted', but all FFFF's at the 4th level
                    are trimmed.

=item backwards

-- see 3.1.2 French Accents, UTR #10.

     backwards => $levelNumber or \@levelNumbers

Weights in reverse order; ex. level 2 (diacritic ordering) in French.
If omitted, forwards at all the levels.

=item entry

-- see 3.1 Linguistic Features; 3.2.1 File Format, UTR #10.

Overrides a default order or defines additional collation elements

  entry => <<'ENTRIES', # use the UCA file format
00E6 ; [.0861.0020.0002.00E6] [.08B1.0020.0002.00E6] # ligature <ae> as <a><e>
0063 0068 ; [.0893.0020.0002.0063]      # "ch" in traditional Spanish
0043 0068 ; [.0893.0020.0008.0043]      # "Ch" in traditional Spanish
ENTRIES

=item ignoreName

=item ignoreChar

-- see Completely Ignorable, 3.2.2 Alternate Weighting, UTR #10.

Makes the entry in the table ignorable.
If a collation element is ignorable,
it is ignored as if the element had been deleted from there.

E.g. when 'a' and 'e' are ignorable,
'element' is equal to 'lament' (or 'lmnt').

=item level

-- see 4.3 Form a sort key for each string, UTR #10.

Set the maximum level.
Any higher levels than the specified one are ignored.

  Level 1: alphabetic ordering
  Level 2: diacritic ordering
  Level 3: case ordering
  Level 4: tie-breaking (e.g. in the case when alternate is 'shifted')

  ex.level => 2,

If omitted, the maximum is the 4th.

=item normalization

-- see 4.1 Normalize each input string, UTR #10.

If specified, strings are normalized before preparation of sort keys
(the normalization is executed after preprocess).

As a form name, one of the following names must be used.

  'C'  or 'NFC'  for Normalization Form C
  'D'  or 'NFD'  for Normalization Form D
  'KC' or 'NFKC' for Normalization Form KC
  'KD' or 'NFKD' for Normalization Form KD

If omitted, the string is put into Normalization Form D.

If C<undef> is passed explicitly as the value for this key,
any normalization is not carried out (this may make tailoring easier
if any normalization is not desired).

see B<CAVEAT>.

=item overrideCJK

-- see 7.1 Derived Collation Elements, UTR #10.

By default, mapping of CJK Unified Ideographs
uses the Unicode codepoint order.
But the mapping of CJK Unified Ideographs may be overrided.

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
in table or L<entry> is still valid.

=item overrideHangul

-- see 7.1 Derived Collation Elements, UTR #10.

By default, Hangul Syllables are decomposed into Hangul Jamo.
But the mapping of Hangul Syllables may be overrided.

This tag works like L<overrideCJK>, so see there for examples.

If you want to override the mapping of Hangul Syllables,
the Normalization Forms D and KD are not appropriate
(they will be decomposed before overriding).

If C<undef> is passed explicitly as the value for this key,
weight for Hangul Syllables is treated as undefined
without decomposition into Hangul Jamo.
But definition of weight for Hangul Syllables
in table or L<entry> is still valid.

=item preprocess

-- see 5.1 Preprocessing, UTR #10.

If specified, the coderef is used to preprocess
before the formation of sort keys.

ex. dropping English articles, such as "a" or "the".
Then, "the pen" is before "a pencil".

     preprocess => sub {
           my $str = shift;
           $str =~ s/\b(?:an?|the)\s+//gi;
           $str;
        },

=item rearrange

-- see 3.1.3 Rearrangement, UTR #10.

Characters that are not coded in logical order and to be rearranged.
By default,

    rearrange => [ 0x0E40..0x0E44, 0x0EC0..0x0EC4 ],

If you want to disallow any rearrangement,
pass C<undef> or C<[]> (a reference to an empty list)
as the value for this key.

=item table

-- see 3.2 Default Unicode Collation Element Table, UTR #10.

You can use another element table if desired.
The table file must be in your C<lib/Unicode/Collate> directory.

By default, the file C<lib/Unicode/Collate/allkeys.txt> is used.

If C<undef> is passed explicitly as the value for this key,
no file is read (but you can define collation elements via L<entry>).

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

-- see 6.3.4 Reducing the Repertoire, UTR #10.

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

-- see 6.6 Case Comparisons; 7.3.1 Tertiary Weight Table, UTR #10.

By default, lowercase is before uppercase
and hiragana is before katakana.

If the tag is made true, this is reversed.

B<NOTE>: These tags simplemindedly assume
any lowercase/uppercase or hiragana/katakana distinctions
should occur in level 3, and their weights at level 3
should be same as those mentioned in 7.3.1, UTR #10.
If you define your collation elements which violates this,
these tags doesn't work validly.

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

-- see 4.3 Form a sort key for each string, UTR #10.

Returns a sort key.

You compare the sort keys using a binary comparison
and get the result of the comparison of the strings using UCA.

   $Collator->getSortKey($a) cmp $Collator->getSortKey($b)

      is equivalent to

   $Collator->cmp($a, $b)

=item C<$sortKeyForm = $Collator-E<gt>viewSortKey($string)>

Returns a string formalized to display a sort key.
Weights are enclosed with C<'['> and C<']'>
and level boundaries are denoted by C<'|'>.

   use Unicode::Collate;
   my $c = Unicode::Collate->new();
   print $c->viewSortKey("Perl"),"\n";

    # output:
    # [09B3 08B1 09CB 094F|0020 0020 0020 0020|0008 0002 0002 0002|FFFF FFFF FFFF FFFF]
    #  Level 1             Level 2             Level 3             Level 4

=item C<$position = $Collator-E<gt>index($string, $substring)>

=item C<($position, $length) = $Collator-E<gt>index($string, $substring)>

-- see 6.8 Searching, UTR #10.

If C<$substring> matches a part of C<$string>, returns
the position of the first occurrence of the matching part in scalar context;
in list context, returns a two-element list of
the position and the length of the matching part.

B<Notice> that the length of the matching part may differ from
the length of C<$substring>.

B<Note> that the position and the length are counted on the string
after the process of preprocess, normalization, and rearrangement.
Therefore, in case the specified string is not binary equal to
the preprocessed/normalized/rearranged string, the position and the length
may differ form those on the specified string. But it is guaranteed
that, if matched, it returns a non-negative value as C<$position>.

If C<$substring> does not match any part of C<$string>,
returns C<-1> in scalar context and
an empty list in list context.

e.g. you say

  my $Collator = Unicode::Collate->new( normalization => undef, level => 1 );
  my $str = "Ich mu\x{00DF} studieren.";
  my $sub = "m\x{00FC}ss";
  my $match;
  if (my($pos,$len) = $Collator->index($str, $sub)) {
      $match = substr($str, $pos, $len);
  }

and get C<"mu\x{00DF}"> in C<$match> since C<"mu>E<223>C<">
is primary equal to C<"m>E<252>C<ss">. 

=back

=head2 Other Methods

=over 4

=item UCA_Version

Returns the version number of Unicode Technical Standard 10
this module consults.

=item Base_Unicode_Version

Returns the version number of the Unicode Standard
this module is based on.

=back

=head2 EXPORT

None by default.

=head2 TODO

Unicode::Collate has not been ported to EBCDIC.  The code mostly would
work just fine but a decision needs to be made: how the module should
work in EBCDIC?  Should the low 256 characters be understood as
Unicode or as EBCDIC code points?  Should one be chosen or should
there be a way to do either?  Or should such translation be left
outside the module for the user to do, for example by using
Encode::from_to()?
(or utf8::unicode_to_native()/utf8::native_to_unicode()?)

=head2 CAVEAT

Use of the C<normalization> parameter requires
the B<Unicode::Normalize> module.

If you need not it (say, in the case when you need not
handle any combining characters),
assign C<normalization =E<gt> undef> explicitly.

-- see 6.5 Avoiding Normalization, UTR #10.

=head2 BUGS

C<index()> is an experimental method and
its return value may be unreliable.
The correct implementation for C<index()> must be based
on Locale-Sensitive Support: Level 3 in UTR #18,
F<Unicode Regular Expression Guidelines>.

See also 4.2 Locale-Dependent Graphemes in UTR #18.

=head1 AUTHOR

SADAHIRO Tomoyuki, E<lt>SADAHIRO@cpan.orgE<gt>

  http://homepage1.nifty.com/nomenclator/perl/

  Copyright(C) 2001-2002, SADAHIRO Tomoyuki. Japan. All rights reserved.

  This library is free software; you can redistribute it
  and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

=over 4

=item http://www.unicode.org/unicode/reports/tr10/

Unicode Collation Algorithm - UTR #10

=item http://www.unicode.org/unicode/reports/tr10/allkeys.txt

The Default Unicode Collation Element Table

=item http://www.unicode.org/unicode/reports/tr15/

Unicode Normalization Forms - UAX #15

=item http://www.unicode.org/unicode/reports/tr18

Unicode Regular Expression Guidelines - UTR #18

=item L<Unicode::Normalize>

=back

=cut
