package Unicode::UCD;

use strict;
use warnings;

our $VERSION = '0.27';

use Storable qw(dclone);

require Exporter;

our @ISA = qw(Exporter);

our @EXPORT_OK = qw(charinfo
		    charblock charscript
		    charblocks charscripts
		    charinrange
		    general_categories bidi_types
		    compexcl
		    casefold casespec
		    namedseq);

use Carp;

=head1 NAME

Unicode::UCD - Unicode character database

=head1 SYNOPSIS

    use Unicode::UCD 'charinfo';
    my $charinfo   = charinfo($codepoint);

    use Unicode::UCD 'casefold';
    my $casefold = casefold(0xFB00);

    use Unicode::UCD 'casespec';
    my $casespec = casespec(0xFB00);

    use Unicode::UCD 'charblock';
    my $charblock  = charblock($codepoint);

    use Unicode::UCD 'charscript';
    my $charscript = charscript($codepoint);

    use Unicode::UCD 'charblocks';
    my $charblocks = charblocks();

    use Unicode::UCD 'charscripts';
    my $charscripts = charscripts();

    use Unicode::UCD qw(charscript charinrange);
    my $range = charscript($script);
    print "looks like $script\n" if charinrange($range, $codepoint);

    use Unicode::UCD qw(general_categories bidi_types);
    my $categories = general_categories();
    my $types = bidi_types();

    use Unicode::UCD 'compexcl';
    my $compexcl = compexcl($codepoint);

    use Unicode::UCD 'namedseq';
    my $namedseq = namedseq($named_sequence_name);

    my $unicode_version = Unicode::UCD::UnicodeVersion();

=head1 DESCRIPTION

The Unicode::UCD module offers a series of functions that
provide a simple interface to the Unicode
Character Database.

=head2 code point argument

Some of the functions are called with a I<code point argument>, which is either
a decimal or a hexadecimal scalar designating a Unicode code point, or C<U+>
followed by hexadecimals designating a Unicode code point.  In other words, if
you want a code point to be interpreted as a hexadecimal number, you must
prefix it with either C<0x> or C<U+>, because a string like e.g. C<123> will be
interpreted as a decimal code point.  Also note that Unicode is B<not> limited
to 16 bits (the number of Unicode code points is open-ended, in theory
unlimited): you may have more than 4 hexdigits.
=cut

my $UNICODEFH;
my $BLOCKSFH;
my $SCRIPTSFH;
my $VERSIONFH;
my $COMPEXCLFH;
my $CASEFOLDFH;
my $CASESPECFH;
my $NAMEDSEQFH;

sub openunicode {
    my ($rfh, @path) = @_;
    my $f;
    unless (defined $$rfh) {
	for my $d (@INC) {
	    use File::Spec;
	    $f = File::Spec->catfile($d, "unicore", @path);
	    last if open($$rfh, $f);
	    undef $f;
	}
	croak __PACKAGE__, ": failed to find ",
              File::Spec->catfile(@path), " in @INC"
	    unless defined $f;
    }
    return $f;
}

=head2 B<charinfo()>

    use Unicode::UCD 'charinfo';

    my $charinfo = charinfo(0x41);

This returns information about the input L</code point argument>
as a reference to a hash of fields as defined by the Unicode
standard.  If the L</code point argument> is not assigned in the standard
(i.e., has the general category C<Cn> meaning C<Unassigned>)
or is a non-character (meaning it is guaranteed to never be assigned in
the standard),
B<undef> is returned.

Fields that aren't applicable to the particular code point argument exist in the
returned hash, and are empty. 

The keys in the hash with the meanings of their values are:

=over

=item B<code>

the input L</code point argument> expressed in hexadecimal, with leading zeros
added if necessary to make it contain at least four hexdigits

=item B<name>

name of I<code>, all IN UPPER CASE.
Some control-type code points do not have names.
This field will be empty for C<Surrogate> and C<Private Use> code points,
and for the others without a name,
it will contain a description enclosed in angle brackets, like
C<E<lt>controlE<gt>>.


=item B<category>

The short name of the general category of I<code>.
This will match one of the keys in the hash returned by L</general_categories()>.

=item B<combining>

the combining class number for I<code> used in the Canonical Ordering Algorithm.
For Unicode 5.1, this is described in Section 3.11 C<Canonical Ordering Behavior>
available at
L<http://www.unicode.org/versions/Unicode5.1.0/>

=item B<bidi>

bidirectional type of I<code>.
This will match one of the keys in the hash returned by L</bidi_types()>.

=item B<decomposition>

is empty if I<code> has no decomposition; or is one or more codes
(separated by spaces) that taken in order represent a decomposition for
I<code>.  Each has at least four hexdigits.
The codes may be preceded by a word enclosed in angle brackets then a space,
like C<E<lt>compatE<gt> >, giving the type of decomposition

=item B<decimal>

if I<code> is a decimal digit this is its integer numeric value

=item B<digit>

if I<code> represents a whole number, this is its integer numeric value

=item B<numeric>

if I<code> represents a whole or rational number, this is its numeric value.
Rational values are expressed as a string like C<1/4>.

=item B<mirrored>

C<Y> or C<N> designating if I<code> is mirrored in bidirectional text

=item B<unicode10>

name of I<code> in the Unicode 1.0 standard if one
existed for this code point and is different from the current name

=item B<comment>

ISO 10646 comment field.
It appears in parentheses in the ISO 10646 names list,
or contains an asterisk to indicate there is
a note for this code point in Annex P of that standard.

=item B<upper>

is empty if there is no single code point uppercase mapping for I<code>;
otherwise it is that mapping expressed as at least four hexdigits.
(L</casespec()> should be used in addition to B<charinfo()>
for case mappings when the calling program can cope with multiple code point
mappings.)

=item B<lower>

is empty if there is no single code point lowercase mapping for I<code>;
otherwise it is that mapping expressed as at least four hexdigits.
(L</casespec()> should be used in addition to B<charinfo()>
for case mappings when the calling program can cope with multiple code point
mappings.)

=item B<title>

is empty if there is no single code point titlecase mapping for I<code>;
otherwise it is that mapping expressed as at least four hexdigits.
(L</casespec()> should be used in addition to B<charinfo()>
for case mappings when the calling program can cope with multiple code point
mappings.)

=item B<block>

block I<code> belongs to (used in \p{In...}).
See L</Blocks versus Scripts>.


=item B<script>

script I<code> belongs to.
See L</Blocks versus Scripts>.

=back

Note that you cannot do (de)composition and casing based solely on the
I<decomposition>, I<combining>, I<lower>, I<upper>, and I<title> fields;
you will need also the L</compexcl()>, and L</casespec()> functions.

=cut

# NB: This function is duplicated in charnames.pm
sub _getcode {
    my $arg = shift;

    if ($arg =~ /^[1-9]\d*$/) {
	return $arg;
    } elsif ($arg =~ /^(?:[Uu]\+|0[xX])?([[:xdigit:]]+)$/) {
	return hex($1);
    }

    return;
}

# Lingua::KO::Hangul::Util not part of the standard distribution
# but it will be used if available.

eval { require Lingua::KO::Hangul::Util };
my $hasHangulUtil = ! $@;
if ($hasHangulUtil) {
    Lingua::KO::Hangul::Util->import();
}

sub hangul_decomp { # internal: called from charinfo
    if ($hasHangulUtil) {
	my @tmp = decomposeHangul(shift);
	return sprintf("%04X %04X",      @tmp) if @tmp == 2;
	return sprintf("%04X %04X %04X", @tmp) if @tmp == 3;
    }
    return;
}

sub hangul_charname { # internal: called from charinfo
    return sprintf("HANGUL SYLLABLE-%04X", shift);
}

sub han_charname { # internal: called from charinfo
    return sprintf("CJK UNIFIED IDEOGRAPH-%04X", shift);
}

# Overwritten by data in file
my %first_last = (
   'CJK Ideograph Extension A' => [ 0x3400,   0x4DB5   ],
   'CJK Ideograph'             => [ 0x4E00,   0x9FA5   ],
   'CJK Ideograph Extension B' => [ 0x20000,  0x2A6D6  ],
);

get_charinfo_ranges();

sub get_charinfo_ranges {
   my @blocks = keys %first_last;
   
   my $fh;
   openunicode( \$fh, 'UnicodeData.txt' );
   if( defined $fh ){
      while( my $line = <$fh> ){
         next unless $line =~ /(?:First|Last)/;
         if( grep{ $line =~ /[^;]+;<$_\s*,\s*(?:First|Last)>/ }@blocks ){
            my ($number,$block,$type);
            ($number,$block) = split /;/, $line;
            $block =~ s/<|>//g;
            ($block,$type) = split /, /, $block;
            my $index = $type eq 'First' ? 0 : 1;
            $first_last{ $block }->[$index] = hex $number;
         }
      }
   }
}

my @CharinfoRanges = (
# block name
# [ first, last, coderef to name, coderef to decompose ],
# CJK Ideographs Extension A
  [ @{ $first_last{'CJK Ideograph Extension A'} },        \&han_charname,   undef  ],
# CJK Ideographs
  [ @{ $first_last{'CJK Ideograph'} },                    \&han_charname,   undef  ],
# Hangul Syllables
  [ 0xAC00,   0xD7A3,   $hasHangulUtil ? \&getHangulName : \&hangul_charname,  \&hangul_decomp ],
# Non-Private Use High Surrogates
  [ 0xD800,   0xDB7F,   undef,   undef  ],
# Private Use High Surrogates
  [ 0xDB80,   0xDBFF,   undef,   undef  ],
# Low Surrogates
  [ 0xDC00,   0xDFFF,   undef,   undef  ],
# The Private Use Area
  [ 0xE000,   0xF8FF,   undef,   undef  ],
# CJK Ideographs Extension B
  [ @{ $first_last{'CJK Ideograph Extension B'} },        \&han_charname,   undef  ],
# Plane 15 Private Use Area
  [ 0xF0000,  0xFFFFD,  undef,   undef  ],
# Plane 16 Private Use Area
  [ 0x100000, 0x10FFFD, undef,   undef  ],
);

sub charinfo {
    my $arg  = shift;
    my $code = _getcode($arg);
    croak __PACKAGE__, "::charinfo: unknown code '$arg'"
	unless defined $code;
    my $hexk = sprintf("%06X", $code);
    my($rcode,$rname,$rdec);
    foreach my $range (@CharinfoRanges){
      if ($range->[0] <= $code && $code <= $range->[1]) {
        $rcode = $hexk;
	$rcode =~ s/^0+//;
	$rcode =  sprintf("%04X", hex($rcode));
        $rname = $range->[2] ? $range->[2]->($code) : '';
        $rdec  = $range->[3] ? $range->[3]->($code) : '';
        $hexk  = sprintf("%06X", $range->[0]); # replace by the first
        last;
      }
    }
    openunicode(\$UNICODEFH, "UnicodeData.txt");
    if (defined $UNICODEFH) {
	use Search::Dict 1.02;
	if (look($UNICODEFH, "$hexk;", { xfrm => sub { $_[0] =~ /^([^;]+);(.+)/; sprintf "%06X;$2", hex($1) } } ) >= 0) {
	    my $line = <$UNICODEFH>;
	    return unless defined $line;
	    chomp $line;
	    my %prop;
	    @prop{qw(
		     code name category
		     combining bidi decomposition
		     decimal digit numeric
		     mirrored unicode10 comment
		     upper lower title
		    )} = split(/;/, $line, -1);
	    $hexk =~ s/^0+//;
	    $hexk =  sprintf("%04X", hex($hexk));
	    if ($prop{code} eq $hexk) {
		$prop{block}  = charblock($code);
		$prop{script} = charscript($code);
		if(defined $rname){
                    $prop{code} = $rcode;
                    $prop{name} = $rname;
                    $prop{decomposition} = $rdec;
                }
		return \%prop;
	    }
	}
    }
    return;
}

sub _search { # Binary search in a [[lo,hi,prop],[...],...] table.
    my ($table, $lo, $hi, $code) = @_;

    return if $lo > $hi;

    my $mid = int(($lo+$hi) / 2);

    if ($table->[$mid]->[0] < $code) {
	if ($table->[$mid]->[1] >= $code) {
	    return $table->[$mid]->[2];
	} else {
	    _search($table, $mid + 1, $hi, $code);
	}
    } elsif ($table->[$mid]->[0] > $code) {
	_search($table, $lo, $mid - 1, $code);
    } else {
	return $table->[$mid]->[2];
    }
}

sub charinrange {
    my ($range, $arg) = @_;
    my $code = _getcode($arg);
    croak __PACKAGE__, "::charinrange: unknown code '$arg'"
	unless defined $code;
    _search($range, 0, $#$range, $code);
}

=head2 B<charblock()>

    use Unicode::UCD 'charblock';

    my $charblock = charblock(0x41);
    my $charblock = charblock(1234);
    my $charblock = charblock(0x263a);
    my $charblock = charblock("U+263a");

    my $range     = charblock('Armenian');

With a L</code point argument> charblock() returns the I<block> the code point
belongs to, e.g.  C<Basic Latin>.
If the code point is unassigned, this returns the block it would belong to if
it were assigned (which it may in future versions of the Unicode Standard).

See also L</Blocks versus Scripts>.

If supplied with an argument that can't be a code point, charblock() tries
to do the opposite and interpret the argument as a code point block. The
return value is a I<range>: an anonymous list of lists that contain
I<start-of-range>, I<end-of-range> code point pairs. You can test whether
a code point is in a range using the L</charinrange()> function. If the
argument is not a known code point block, B<undef> is returned.

=cut

my @BLOCKS;
my %BLOCKS;

sub _charblocks {
    unless (@BLOCKS) {
	if (openunicode(\$BLOCKSFH, "Blocks.txt")) {
	    local $_;
	    while (<$BLOCKSFH>) {
		if (/^([0-9A-F]+)\.\.([0-9A-F]+);\s+(.+)/) {
		    my ($lo, $hi) = (hex($1), hex($2));
		    my $subrange = [ $lo, $hi, $3 ];
		    push @BLOCKS, $subrange;
		    push @{$BLOCKS{$3}}, $subrange;
		}
	    }
	    close($BLOCKSFH);
	}
    }
}

sub charblock {
    my $arg = shift;

    _charblocks() unless @BLOCKS;

    my $code = _getcode($arg);

    if (defined $code) {
	_search(\@BLOCKS, 0, $#BLOCKS, $code);
    } else {
	if (exists $BLOCKS{$arg}) {
	    return dclone $BLOCKS{$arg};
	} else {
	    return;
	}
    }
}

=head2 B<charscript()>

    use Unicode::UCD 'charscript';

    my $charscript = charscript(0x41);
    my $charscript = charscript(1234);
    my $charscript = charscript("U+263a");

    my $range      = charscript('Thai');

With a L</code point argument> charscript() returns the I<script> the
code point belongs to, e.g.  C<Latin>, C<Greek>, C<Han>.
If the code point is unassigned, it returns B<undef>

If supplied with an argument that can't be a code point, charscript() tries
to do the opposite and interpret the argument as a code point script. The
return value is a I<range>: an anonymous list of lists that contain
I<start-of-range>, I<end-of-range> code point pairs. You can test whether a
code point is in a range using the L</charinrange()> function. If the
argument is not a known code point script, B<undef> is returned.

See also L</Blocks versus Scripts>.

=cut

my @SCRIPTS;
my %SCRIPTS;

sub _charscripts {
    unless (@SCRIPTS) {
	if (openunicode(\$SCRIPTSFH, "Scripts.txt")) {
	    local $_;
	    while (<$SCRIPTSFH>) {
		if (/^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s+;\s+(\w+)/) {
		    my ($lo, $hi) = (hex($1), $2 ? hex($2) : hex($1));
		    my $script = lc($3);
		    $script =~ s/\b(\w)/uc($1)/ge;
		    my $subrange = [ $lo, $hi, $script ];
		    push @SCRIPTS, $subrange;
		    push @{$SCRIPTS{$script}}, $subrange;
		}
	    }
	    close($SCRIPTSFH);
	    @SCRIPTS = sort { $a->[0] <=> $b->[0] } @SCRIPTS;
	}
    }
}

sub charscript {
    my $arg = shift;

    _charscripts() unless @SCRIPTS;

    my $code = _getcode($arg);

    if (defined $code) {
	_search(\@SCRIPTS, 0, $#SCRIPTS, $code);
    } else {
	if (exists $SCRIPTS{$arg}) {
	    return dclone $SCRIPTS{$arg};
	} else {
	    return;
	}
    }
}

=head2 B<charblocks()>

    use Unicode::UCD 'charblocks';

    my $charblocks = charblocks();

charblocks() returns a reference to a hash with the known block names
as the keys, and the code point ranges (see L</charblock()>) as the values.

See also L</Blocks versus Scripts>.

=cut

sub charblocks {
    _charblocks() unless %BLOCKS;
    return dclone \%BLOCKS;
}

=head2 B<charscripts()>

    use Unicode::UCD 'charscripts';

    my $charscripts = charscripts();

charscripts() returns a reference to a hash with the known script
names as the keys, and the code point ranges (see L</charscript()>) as
the values.

See also L</Blocks versus Scripts>.

=cut

sub charscripts {
    _charscripts() unless %SCRIPTS;
    return dclone \%SCRIPTS;
}

=head2 B<charinrange()>

In addition to using the C<\p{In...}> and C<\P{In...}> constructs, you
can also test whether a code point is in the I<range> as returned by
L</charblock()> and L</charscript()> or as the values of the hash returned
by L</charblocks()> and L</charscripts()> by using charinrange():

    use Unicode::UCD qw(charscript charinrange);

    $range = charscript('Hiragana');
    print "looks like hiragana\n" if charinrange($range, $codepoint);

=cut

my %GENERAL_CATEGORIES =
 (
    'L'  =>         'Letter',
    'LC' =>         'CasedLetter',
    'Lu' =>         'UppercaseLetter',
    'Ll' =>         'LowercaseLetter',
    'Lt' =>         'TitlecaseLetter',
    'Lm' =>         'ModifierLetter',
    'Lo' =>         'OtherLetter',
    'M'  =>         'Mark',
    'Mn' =>         'NonspacingMark',
    'Mc' =>         'SpacingMark',
    'Me' =>         'EnclosingMark',
    'N'  =>         'Number',
    'Nd' =>         'DecimalNumber',
    'Nl' =>         'LetterNumber',
    'No' =>         'OtherNumber',
    'P'  =>         'Punctuation',
    'Pc' =>         'ConnectorPunctuation',
    'Pd' =>         'DashPunctuation',
    'Ps' =>         'OpenPunctuation',
    'Pe' =>         'ClosePunctuation',
    'Pi' =>         'InitialPunctuation',
    'Pf' =>         'FinalPunctuation',
    'Po' =>         'OtherPunctuation',
    'S'  =>         'Symbol',
    'Sm' =>         'MathSymbol',
    'Sc' =>         'CurrencySymbol',
    'Sk' =>         'ModifierSymbol',
    'So' =>         'OtherSymbol',
    'Z'  =>         'Separator',
    'Zs' =>         'SpaceSeparator',
    'Zl' =>         'LineSeparator',
    'Zp' =>         'ParagraphSeparator',
    'C'  =>         'Other',
    'Cc' =>         'Control',
    'Cf' =>         'Format',
    'Cs' =>         'Surrogate',
    'Co' =>         'PrivateUse',
    'Cn' =>         'Unassigned',
 );

sub general_categories {
    return dclone \%GENERAL_CATEGORIES;
}

=head2 B<general_categories()>

    use Unicode::UCD 'general_categories';

    my $categories = general_categories();

This returns a reference to a hash which has short
general category names (such as C<Lu>, C<Nd>, C<Zs>, C<S>) as keys and long
names (such as C<UppercaseLetter>, C<DecimalNumber>, C<SpaceSeparator>,
C<Symbol>) as values.  The hash is reversible in case you need to go
from the long names to the short names.  The general category is the
one returned from
L</charinfo()> under the C<category> key.

=cut

my %BIDI_TYPES =
 (
   'L'   => 'Left-to-Right',
   'LRE' => 'Left-to-Right Embedding',
   'LRO' => 'Left-to-Right Override',
   'R'   => 'Right-to-Left',
   'AL'  => 'Right-to-Left Arabic',
   'RLE' => 'Right-to-Left Embedding',
   'RLO' => 'Right-to-Left Override',
   'PDF' => 'Pop Directional Format',
   'EN'  => 'European Number',
   'ES'  => 'European Number Separator',
   'ET'  => 'European Number Terminator',
   'AN'  => 'Arabic Number',
   'CS'  => 'Common Number Separator',
   'NSM' => 'Non-Spacing Mark',
   'BN'  => 'Boundary Neutral',
   'B'   => 'Paragraph Separator',
   'S'   => 'Segment Separator',
   'WS'  => 'Whitespace',
   'ON'  => 'Other Neutrals',
 ); 

=head2 B<bidi_types()>

    use Unicode::UCD 'bidi_types';

    my $categories = bidi_types();

This returns a reference to a hash which has the short
bidi (bidirectional) type names (such as C<L>, C<R>) as keys and long
names (such as C<Left-to-Right>, C<Right-to-Left>) as values.  The
hash is reversible in case you need to go from the long names to the
short names.  The bidi type is the one returned from
L</charinfo()>
under the C<bidi> key.  For the exact meaning of the various bidi classes
the Unicode TR9 is recommended reading:
L<http://www.unicode.org/reports/tr9/>
(as of Unicode 5.0.0)

=cut

sub bidi_types {
    return dclone \%BIDI_TYPES;
}

=head2 B<compexcl()>

    use Unicode::UCD 'compexcl';

    my $compexcl = compexcl(0x09dc);

This returns B<true> if the
L</code point argument> should not be produced by composition normalization,
B<AND> if that fact is not otherwise determinable from the Unicode data base.
It currently does not return B<true> if the code point has a decomposition
consisting of another single code point, nor if its decomposition starts
with a code point whose combining class is non-zero.  Code points that meet
either of these conditions should also not be produced by composition
normalization.

It returns B<false> otherwise.

=cut

my %COMPEXCL;

sub _compexcl {
    unless (%COMPEXCL) {
	if (openunicode(\$COMPEXCLFH, "CompositionExclusions.txt")) {
	    local $_;
	    while (<$COMPEXCLFH>) {
		if (/^([0-9A-F]+)\s+\#\s+/) {
		    my $code = hex($1);
		    $COMPEXCL{$code} = undef;
		}
	    }
	    close($COMPEXCLFH);
	}
    }
}

sub compexcl {
    my $arg  = shift;
    my $code = _getcode($arg);
    croak __PACKAGE__, "::compexcl: unknown code '$arg'"
	unless defined $code;

    _compexcl() unless %COMPEXCL;

    return exists $COMPEXCL{$code};
}

=head2 B<casefold()>

    use Unicode::UCD 'casefold';

    my $casefold = casefold(0xDF);
    if (defined $casefold) {
        my @full_fold_hex = split / /, $casefold->{'full'};
        my $full_fold_string =
                    join "", map {chr(hex($_))} @full_fold_hex;
        my @turkic_fold_hex =
                        split / /, ($casefold->{'turkic'} ne "")
                                        ? $casefold->{'turkic'}
                                        : $casefold->{'full'};
        my $turkic_fold_string =
                        join "", map {chr(hex($_))} @turkic_fold_hex;
    }
    if (defined $casefold && $casefold->{'simple'} ne "") {
        my $simple_fold_hex = $casefold->{'simple'};
        my $simple_fold_string = chr(hex($simple_fold_hex));
    }

This returns the (almost) locale-independent case folding of the
character specified by the L</code point argument>.

If there is no case folding for that code point, B<undef> is returned.

If there is a case folding for that code point, a reference to a hash
with the following fields is returned:

=over

=item B<code>

the input L</code point argument> expressed in hexadecimal, with leading zeros
added if necessary to make it contain at least four hexdigits

=item B<full>

one or more codes (separated by spaces) that taken in order give the
code points for the case folding for I<code>.
Each has at least four hexdigits.

=item B<simple>

is empty, or is exactly one code with at least four hexdigits which can be used
as an alternative case folding when the calling program cannot cope with the
fold being a sequence of multiple code points.  If I<full> is just one code
point, then I<simple> equals I<full>.  If there is no single code point folding
defined for I<code>, then I<simple> is the empty string.  Otherwise, it is an
inferior, but still better-than-nothing alternative folding to I<full>.

=item B<mapping>

is the same as I<simple> if I<simple> is not empty, and it is the same as I<full>
otherwise.  It can be considered to be the simplest possible folding for
I<code>.  It is defined primarily for backwards compatibility.

=item B<status>

is C<C> (for C<common>) if the best possible fold is a single code point
(I<simple> equals I<full> equals I<mapping>).  It is C<S> if there are distinct
folds, I<simple> and I<full> (I<mapping> equals I<simple>).  And it is C<F> if
there only a I<full> fold (I<mapping> equals I<full>; I<simple> is empty).  Note
that this
describes the contents of I<mapping>.  It is defined primarily for backwards
compatibility.

On versions 3.1 and earlier of Unicode, I<status> can also be
C<I> which is the same as C<C> but is a special case for dotted uppercase I and
dotless lowercase i:

=over

=item B<*>

If you use this C<I> mapping, the result is case-insensitive,
but dotless and dotted I's are not distinguished

=item B<*>

If you exclude this C<I> mapping, the result is not fully case-insensitive, but
dotless and dotted I's are distinguished

=back

=item B<turkic>

contains any special folding for Turkic languages.  For versions of Unicode
starting with 3.2, this field is empty unless I<code> has a different folding
in Turkic languages, in which case it is one or more codes (separated by
spaces) that taken in order give the code points for the case folding for
I<code> in those languages.
Each code has at least four hexdigits.
Note that this folding does not maintain canonical equivalence without
additional processing.

For versions of Unicode 3.1 and earlier, this field is empty unless there is a
special folding for Turkic languages, in which case I<status> is C<I>, and
I<mapping>, I<full>, I<simple>, and I<turkic> are all equal.  

=back

Programs that want complete generality and the best folding results should use
the folding contained in the I<full> field.  But note that the fold for some
code points will be a sequence of multiple code points.

Programs that can't cope with the fold mapping being multiple code points can
use the folding contained in the I<simple> field, with the loss of some
generality.  In Unicode 5.1, about 7% of the defined foldings have no single
code point folding.

The I<mapping> and I<status> fields are provided for backwards compatibility for
existing programs.  They contain the same values as in previous versions of
this function.

Locale is not completely independent.  The I<turkic> field contains results to
use when the locale is a Turkic language.

For more information about case mappings see
L<http://www.unicode.org/unicode/reports/tr21>

=cut

my %CASEFOLD;

sub _casefold {
    unless (%CASEFOLD) {
	if (openunicode(\$CASEFOLDFH, "CaseFolding.txt")) {
	    local $_;
	    while (<$CASEFOLDFH>) {
		if (/^([0-9A-F]+); ([CFIST]); ([0-9A-F]+(?: [0-9A-F]+)*);/) {
		    my $code = hex($1);
		    $CASEFOLD{$code}{'code'} = $1;
		    $CASEFOLD{$code}{'turkic'} = "" unless
					    defined $CASEFOLD{$code}{'turkic'};
		    if ($2 eq 'C' || $2 eq 'I') {	# 'I' is only on 3.1 and
							# earlier Unicodes
							# Both entries there (I
							# only checked 3.1) are
							# the same as C, and
							# there are no other
							# entries for those
							# codepoints, so treat
							# as if C, but override
							# the turkic one for
							# 'I'.
			$CASEFOLD{$code}{'status'} = $2;
			$CASEFOLD{$code}{'full'} = $CASEFOLD{$code}{'simple'} =
			$CASEFOLD{$code}{'mapping'} = $3;
			$CASEFOLD{$code}{'turkic'} = $3 if $2 eq 'I';
		    } elsif ($2 eq 'F') {
			$CASEFOLD{$code}{'full'} = $3;
			unless (defined $CASEFOLD{$code}{'simple'}) {
				$CASEFOLD{$code}{'simple'} = "";
				$CASEFOLD{$code}{'mapping'} = $3;
				$CASEFOLD{$code}{'status'} = $2;
			}
		    } elsif ($2 eq 'S') {


			# There can't be a simple without a full, and simple
			# overrides all but full

			$CASEFOLD{$code}{'simple'} = $3;
			$CASEFOLD{$code}{'mapping'} = $3;
			$CASEFOLD{$code}{'status'} = $2;
		    } elsif ($2 eq 'T') {
			$CASEFOLD{$code}{'turkic'} = $3;
		    } # else can't happen because only [CIFST] are possible
		}
	    }
	    close($CASEFOLDFH);
	}
    }
}

sub casefold {
    my $arg  = shift;
    my $code = _getcode($arg);
    croak __PACKAGE__, "::casefold: unknown code '$arg'"
	unless defined $code;

    _casefold() unless %CASEFOLD;

    return $CASEFOLD{$code};
}

=head2 B<casespec()>

    use Unicode::UCD 'casespec';

    my $casespec = casespec(0xFB00);

This returns the potentially locale-dependent case mappings of the L</code point
argument>.  The mappings may be longer than a single code point (which the basic
Unicode case mappings as returned by L</charinfo()> never are).

If there are no case mappings for the L</code point argument>, or if all three
possible mappings (I<lower>, I<title> and I<upper>) result in single code
points and are locale independent and unconditional, B<undef> is returned
(which means that the case mappings, if any, for the code point are those
returned by L</charinfo()>).

Otherwise, a reference to a hash giving the mappings (or a reference to a hash
of such hashes, explained below) is returned with the following keys and their
meanings:

The keys in the bottom layer hash with the meanings of their values are:

=over

=item B<code>

the input L</code point argument> expressed in hexadecimal, with leading zeros
added if necessary to make it contain at least four hexdigits

=item B<lower>

one or more codes (separated by spaces) that taken in order give the
code points for the lower case of I<code>.
Each has at least four hexdigits.

=item B<title>

one or more codes (separated by spaces) that taken in order give the
code points for the title case of I<code>.
Each has at least four hexdigits.

=item B<lower>

one or more codes (separated by spaces) that taken in order give the
code points for the upper case of I<code>.
Each has at least four hexdigits.

=item B<condition>

the conditions for the mappings to be valid.
If B<undef>, the mappings are always valid.
When defined, this field is a list of conditions,
all of which must be true for the mappings to be valid.
The list consists of one or more
I<locales> (see below)
and/or I<contexts> (explained in the next paragraph),
separated by spaces.
(Other than as used to separate elements, spaces are to be ignored.)
Case distinctions in the condition list are not significant.
Conditions preceded by "NON_" represent the negation of the condition.

A I<context> is one of those defined in the Unicode standard.
For Unicode 5.1, they are defined in Section 3.13 C<Default Case Operations>
available at
L<http://www.unicode.org/versions/Unicode5.1.0/>.
These are for context-sensitive casing.

=back

The hash described above is returned for locale-independent casing, where
at least one of the mappings has length longer than one.  If B<undef> is 
returned, the code point may have mappings, but if so, all are length one,
and are returned by L</charinfo()>.
Note that when this function does return a value, it will be for the complete
set of mappings for a code point, even those whose length is one.

If there are additional casing rules that apply only in certain locales,
an additional key for each will be defined in the returned hash.  Each such key
will be its locale name, defined as a 2-letter ISO 3166 country code, possibly
followed by a "_" and a 2-letter ISO language code (possibly followed by a "_"
and a variant code).  You can find the lists of all possible locales, see
L<Locale::Country> and L<Locale::Language>.
(In Unicode 5.1, the only locales returned by this function
are C<lt>, C<tr>, and C<az>.)

Each locale key is a reference to a hash that has the form above, and gives
the casing rules for that particular locale, which take precedence over the
locale-independent ones when in that locale.

If the only casing for a code point is locale-dependent, then the returned
hash will not have any of the base keys, like C<code>, C<upper>, etc., but
will contain only locale keys.

For more information about case mappings see
L<http://www.unicode.org/unicode/reports/tr21/>

=cut

my %CASESPEC;

sub _casespec {
    unless (%CASESPEC) {
	if (openunicode(\$CASESPECFH, "SpecialCasing.txt")) {
	    local $_;
	    while (<$CASESPECFH>) {
		if (/^([0-9A-F]+); ([0-9A-F]+(?: [0-9A-F]+)*)?; ([0-9A-F]+(?: [0-9A-F]+)*)?; ([0-9A-F]+(?: [0-9A-F]+)*)?; (\w+(?: \w+)*)?/) {
		    my ($hexcode, $lower, $title, $upper, $condition) =
			($1, $2, $3, $4, $5);
		    my $code = hex($hexcode);
		    if (exists $CASESPEC{$code}) {
			if (exists $CASESPEC{$code}->{code}) {
			    my ($oldlower,
				$oldtitle,
				$oldupper,
				$oldcondition) =
				    @{$CASESPEC{$code}}{qw(lower
							   title
							   upper
							   condition)};
			    if (defined $oldcondition) {
				my ($oldlocale) =
				($oldcondition =~ /^([a-z][a-z](?:_\S+)?)/);
				delete $CASESPEC{$code};
				$CASESPEC{$code}->{$oldlocale} =
				{ code      => $hexcode,
				  lower     => $oldlower,
				  title     => $oldtitle,
				  upper     => $oldupper,
				  condition => $oldcondition };
			    }
			}
			my ($locale) =
			    ($condition =~ /^([a-z][a-z](?:_\S+)?)/);
			$CASESPEC{$code}->{$locale} =
			{ code      => $hexcode,
			  lower     => $lower,
			  title     => $title,
			  upper     => $upper,
			  condition => $condition };
		    } else {
			$CASESPEC{$code} =
			{ code      => $hexcode,
			  lower     => $lower,
			  title     => $title,
			  upper     => $upper,
			  condition => $condition };
		    }
		}
	    }
	    close($CASESPECFH);
	}
    }
}

sub casespec {
    my $arg  = shift;
    my $code = _getcode($arg);
    croak __PACKAGE__, "::casespec: unknown code '$arg'"
	unless defined $code;

    _casespec() unless %CASESPEC;

    return ref $CASESPEC{$code} ? dclone $CASESPEC{$code} : $CASESPEC{$code};
}

=head2 B<namedseq()>

    use Unicode::UCD 'namedseq';

    my $namedseq = namedseq("KATAKANA LETTER AINU P");
    my @namedseq = namedseq("KATAKANA LETTER AINU P");
    my %namedseq = namedseq();

If used with a single argument in a scalar context, returns the string
consisting of the code points of the named sequence, or B<undef> if no
named sequence by that name exists.  If used with a single argument in
a list context, it returns the list of the ordinals of the code points.  If used
with no
arguments in a list context, returns a hash with the names of the
named sequences as the keys and the named sequences as strings as
the values.  Otherwise, it returns B<undef> or an empty list depending
on the context.

This function only operates on officially approved (not provisional) named
sequences.

=cut

my %NAMEDSEQ;

sub _namedseq {
    unless (%NAMEDSEQ) {
	if (openunicode(\$NAMEDSEQFH, "NamedSequences.txt")) {
	    local $_;
	    while (<$NAMEDSEQFH>) {
		if (/^(.+)\s*;\s*([0-9A-F]+(?: [0-9A-F]+)*)$/) {
		    my ($n, $s) = ($1, $2);
		    my @s = map { chr(hex($_)) } split(' ', $s);
		    $NAMEDSEQ{$n} = join("", @s);
		}
	    }
	    close($NAMEDSEQFH);
	}
    }
}

sub namedseq {
    _namedseq() unless %NAMEDSEQ;
    my $wantarray = wantarray();
    if (defined $wantarray) {
	if ($wantarray) {
	    if (@_ == 0) {
		return %NAMEDSEQ;
	    } elsif (@_ == 1) {
		my $s = $NAMEDSEQ{ $_[0] };
		return defined $s ? map { ord($_) } split('', $s) : ();
	    }
	} elsif (@_ == 1) {
	    return $NAMEDSEQ{ $_[0] };
	}
    }
    return;
}

=head2 Unicode::UCD::UnicodeVersion

This returns the version of the Unicode Character Database, in other words, the
version of the Unicode standard the database implements.  The version is a
string of numbers delimited by dots (C<'.'>).

=cut

my $UNICODEVERSION;

sub UnicodeVersion {
    unless (defined $UNICODEVERSION) {
	openunicode(\$VERSIONFH, "version");
	chomp($UNICODEVERSION = <$VERSIONFH>);
	close($VERSIONFH);
	croak __PACKAGE__, "::VERSION: strange version '$UNICODEVERSION'"
	    unless $UNICODEVERSION =~ /^\d+(?:\.\d+)+$/;
    }
    return $UNICODEVERSION;
}

=head2 B<Blocks versus Scripts>

The difference between a block and a script is that scripts are closer
to the linguistic notion of a set of code points required to present
languages, while block is more of an artifact of the Unicode code point
numbering and separation into blocks of (mostly) 256 code points.

For example the Latin B<script> is spread over several B<blocks>, such
as C<Basic Latin>, C<Latin 1 Supplement>, C<Latin Extended-A>, and
C<Latin Extended-B>.  On the other hand, the Latin script does not
contain all the characters of the C<Basic Latin> block (also known as
ASCII): it includes only the letters, and not, for example, the digits
or the punctuation.

For blocks see L<http://www.unicode.org/Public/UNIDATA/Blocks.txt>

For scripts see UTR #24: L<http://www.unicode.org/unicode/reports/tr24/>

=head2 B<Matching Scripts and Blocks>

Scripts are matched with the regular-expression construct
C<\p{...}> (e.g. C<\p{Tibetan}> matches characters of the Tibetan script),
while C<\p{In...}> is used for blocks (e.g. C<\p{InTibetan}> matches
any of the 256 code points in the Tibetan block).


=head2 Implementation Note

The first use of charinfo() opens a read-only filehandle to the Unicode
Character Database (the database is included in the Perl distribution).
The filehandle is then kept open for further queries.  In other words,
if you are wondering where one of your filehandles went, that's where.

=head1 BUGS

Does not yet support EBCDIC platforms.

L</compexcl()> should give a complete list of excluded code points.

=head1 AUTHOR

Jarkko Hietaniemi

=cut

1;
