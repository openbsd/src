package Unicode::Normalize;

BEGIN {
    unless ("A" eq pack('U', 0x41)) {
	die "Unicode::Normalize cannot stringify a Unicode code point\n";
    }
}

use 5.006;
use strict;
use warnings;
use Carp;

no warnings 'utf8';

our $VERSION = '0.28';
our $PACKAGE = __PACKAGE__;

require Exporter;
require DynaLoader;

our @ISA = qw(Exporter DynaLoader);
our @EXPORT = qw( NFC NFD NFKC NFKD );
our @EXPORT_OK = qw(
    normalize decompose reorder compose
    checkNFD checkNFKD checkNFC checkNFKC check
    getCanon getCompat getComposite getCombinClass
    isExclusion isSingleton isNonStDecomp isComp2nd isComp_Ex
    isNFD_NO isNFC_NO isNFC_MAYBE isNFKD_NO isNFKC_NO isNFKC_MAYBE
    FCD checkFCD FCC checkFCC composeContiguous
    splitOnLastStarter
);
our %EXPORT_TAGS = (
    all       => [ @EXPORT, @EXPORT_OK ],
    normalize => [ @EXPORT, qw/normalize decompose reorder compose/ ],
    check     => [ qw/checkNFD checkNFKD checkNFC checkNFKC check/ ],
    fast      => [ qw/FCD checkFCD FCC checkFCC composeContiguous/ ],
);

######

bootstrap Unicode::Normalize $VERSION;

######

sub pack_U {
    return pack('U*', @_);
}

sub unpack_U {
    return unpack('U*', pack('U*').shift);
}


##
## normalization forms
##

use constant COMPAT => 1;

sub NFD  ($) { reorder(decompose($_[0])) }
sub NFKD ($) { reorder(decompose($_[0], COMPAT)) }
sub NFC  ($) { compose(reorder(decompose($_[0]))) }
sub NFKC ($) { compose(reorder(decompose($_[0], COMPAT))) }

sub FCD ($) {
    my $str = shift;
    return checkFCD($str) ? $str : NFD($str);
}
sub FCC ($) { composeContiguous(reorder(decompose($_[0]))) }

our %formNorm = (
    NFC  => \&NFC,	C  => \&NFC,
    NFD  => \&NFD,	D  => \&NFD,
    NFKC => \&NFKC,	KC => \&NFKC,
    NFKD => \&NFKD,	KD => \&NFKD,
    FCD  => \&FCD,	FCC => \&FCC,
);

sub normalize($$)
{
    my $form = shift;
    my $str = shift;
    return exists $formNorm{$form} 
	? $formNorm{$form}->($str)
	: croak $PACKAGE."::normalize: invalid form name: $form";
}


##
## quick check
##

our %formCheck = (
    NFC  => \&checkNFC, 	C  => \&checkNFC,
    NFD  => \&checkNFD, 	D  => \&checkNFD,
    NFKC => \&checkNFKC,	KC => \&checkNFKC,
    NFKD => \&checkNFKD,	KD => \&checkNFKD,
    FCD  => \&checkFCD, 	FCC => \&checkFCC,
);

sub check($$)
{
    my $form = shift;
    my $str = shift;
    return exists $formCheck{$form} 
	? $formCheck{$form}->($str)
	: croak $PACKAGE."::check: invalid form name: $form";
}

1;
__END__

=head1 NAME

Unicode::Normalize - Unicode Normalization Forms

=head1 SYNOPSIS

  use Unicode::Normalize;

  $NFD_string  = NFD($string);  # Normalization Form D
  $NFC_string  = NFC($string);  # Normalization Form C
  $NFKD_string = NFKD($string); # Normalization Form KD
  $NFKC_string = NFKC($string); # Normalization Form KC

   or

  use Unicode::Normalize 'normalize';

  $NFD_string  = normalize('D',  $string);  # Normalization Form D
  $NFC_string  = normalize('C',  $string);  # Normalization Form C
  $NFKD_string = normalize('KD', $string);  # Normalization Form KD
  $NFKC_string = normalize('KC', $string);  # Normalization Form KC

=head1 DESCRIPTION

Parameters:

C<$string> is used as a string under character semantics
(see F<perlunicode>).

C<$codepoint> should be an unsigned integer
representing a Unicode code point.

Note: Between XS edition and pure Perl edition,
interpretation of C<$codepoint> as a decimal number has incompatibility.
XS converts C<$codepoint> to an unsigned integer, but pure Perl does not.
Do not use a floating point nor a negative sign in C<$codepoint>.

=head2 Normalization Forms

=over 4

=item C<$NFD_string = NFD($string)>

returns the Normalization Form D (formed by canonical decomposition).

=item C<$NFC_string = NFC($string)>

returns the Normalization Form C (formed by canonical decomposition
followed by canonical composition).

=item C<$NFKD_string = NFKD($string)>

returns the Normalization Form KD (formed by compatibility decomposition).

=item C<$NFKC_string = NFKC($string)>

returns the Normalization Form KC (formed by compatibility decomposition
followed by B<canonical> composition).

=item C<$FCD_string = FCD($string)>

If the given string is in FCD ("Fast C or D" form; cf. UTN #5),
returns it without modification; otherwise returns an FCD string.

Note: FCD is not always unique, then plural forms may be equivalent
each other. C<FCD()> will return one of these equivalent forms.

=item C<$FCC_string = FCC($string)>

returns the FCC form ("Fast C Contiguous"; cf. UTN #5).

Note: FCC is unique, as well as four normalization forms (NF*).

=item C<$normalized_string = normalize($form_name, $string)>

As C<$form_name>, one of the following names must be given.

  'C'  or 'NFC'  for Normalization Form C  (UAX #15)
  'D'  or 'NFD'  for Normalization Form D  (UAX #15)
  'KC' or 'NFKC' for Normalization Form KC (UAX #15)
  'KD' or 'NFKD' for Normalization Form KD (UAX #15)

  'FCD'          for "Fast C or D" Form  (UTN #5)
  'FCC'          for "Fast C Contiguous" (UTN #5)

=back

=head2 Decomposition and Composition

=over 4

=item C<$decomposed_string = decompose($string)>

=item C<$decomposed_string = decompose($string, $useCompatMapping)>

Decomposes the specified string and returns the result.

If the second parameter (a boolean) is omitted or false, decomposes it
using the Canonical Decomposition Mapping.
If true, decomposes it using the Compatibility Decomposition Mapping.

The string returned is not always in NFD/NFKD.
Reordering may be required.

    $NFD_string  = reorder(decompose($string));       # eq. to NFD()
    $NFKD_string = reorder(decompose($string, TRUE)); # eq. to NFKD()

=item C<$reordered_string  = reorder($string)>

Reorders the combining characters and the like in the canonical ordering
and returns the result.

E.g., when you have a list of NFD/NFKD strings,
you can get the concatenated NFD/NFKD string from them, saying

    $concat_NFD  = reorder(join '', @NFD_strings);
    $concat_NFKD = reorder(join '', @NFKD_strings);

=item C<$composed_string   = compose($string)>

Returns the string where composable pairs are composed.

E.g., when you have a NFD/NFKD string,
you can get its NFC/NFKC string, saying

    $NFC_string  = compose($NFD_string);
    $NFKC_string = compose($NFKD_string);

=back

=head2 Quick Check

(see Annex 8, UAX #15; and F<DerivedNormalizationProps.txt>)

The following functions check whether the string is in that normalization form.

The result returned will be:

    YES     The string is in that normalization form.
    NO      The string is not in that normalization form.
    MAYBE   Dubious. Maybe yes, maybe no.

=over 4

=item C<$result = checkNFD($string)>

returns C<YES> (C<1>) or C<NO> (C<empty string>).

=item C<$result = checkNFC($string)>

returns C<YES> (C<1>), C<NO> (C<empty string>), or C<MAYBE> (C<undef>).

=item C<$result = checkNFKD($string)>

returns C<YES> (C<1>) or C<NO> (C<empty string>).

=item C<$result = checkNFKC($string)>

returns C<YES> (C<1>), C<NO> (C<empty string>), or C<MAYBE> (C<undef>).

=item C<$result = checkFCD($string)>

returns C<YES> (C<1>) or C<NO> (C<empty string>).

=item C<$result = checkFCC($string)>

returns C<YES> (C<1>), C<NO> (C<empty string>), or C<MAYBE> (C<undef>).

If a string is not in FCD, it must not be in FCC.
So C<checkFCC($not_FCD_string)> should return C<NO>.

=item C<$result = check($form_name, $string)>

returns C<YES> (C<1>), C<NO> (C<empty string>), or C<MAYBE> (C<undef>).

C<$form_name> is alike to that for C<normalize()>.

=back

B<Note>

In the cases of NFD, NFKD, and FCD, the answer must be
either C<YES> or C<NO>. The answer C<MAYBE> may be returned
in the cases of NFC, NFKC, and FCC.

A C<MAYBE> string should contain at least one combining character
or the like. For example, C<COMBINING ACUTE ACCENT> has
the MAYBE_NFC/MAYBE_NFKC property.

Both C<checkNFC("A\N{COMBINING ACUTE ACCENT}")>
and C<checkNFC("B\N{COMBINING ACUTE ACCENT}")> will return C<MAYBE>.
C<"A\N{COMBINING ACUTE ACCENT}"> is not in NFC
(its NFC is C<"\N{LATIN CAPITAL LETTER A WITH ACUTE}">),
while C<"B\N{COMBINING ACUTE ACCENT}"> is in NFC.

If you want to check exactly, compare the string with its NFC/NFKC/FCC;
i.e.,

    $string eq NFC($string)    # thorough than checkNFC($string)
    $string eq NFKC($string)   # thorough than checkNFKC($string)
    $string eq FCC($string)    # thorough than checkFCC($string)

=head2 Character Data

These functions are interface of character data used internally.
If you want only to get Unicode normalization forms, you don't need
call them yourself.

=over 4

=item C<$canonical_decomposed = getCanon($codepoint)>

If the character of the specified codepoint is canonically
decomposable (including Hangul Syllables),
returns the B<completely decomposed> string canonically equivalent to it.

If it is not decomposable, returns C<undef>.

=item C<$compatibility_decomposed = getCompat($codepoint)>

If the character of the specified codepoint is compatibility
decomposable (including Hangul Syllables),
returns the B<completely decomposed> string compatibility equivalent to it.

If it is not decomposable, returns C<undef>.

=item C<$codepoint_composite = getComposite($codepoint_here, $codepoint_next)>

If two characters here and next (as codepoints) are composable
(including Hangul Jamo/Syllables and Composition Exclusions),
returns the codepoint of the composite.

If they are not composable, returns C<undef>.

=item C<$combining_class = getCombinClass($codepoint)>

Returns the combining class of the character as an integer.

=item C<$is_exclusion = isExclusion($codepoint)>

Returns a boolean whether the character of the specified codepoint
is a composition exclusion.

=item C<$is_singleton = isSingleton($codepoint)>

Returns a boolean whether the character of the specified codepoint is
a singleton.

=item C<$is_non_starter_decomposition = isNonStDecomp($codepoint)>

Returns a boolean whether the canonical decomposition
of the character of the specified codepoint
is a Non-Starter Decomposition.

=item C<$may_be_composed_with_prev_char = isComp2nd($codepoint)>

Returns a boolean whether the character of the specified codepoint
may be composed with the previous one in a certain composition
(including Hangul Compositions, but excluding
Composition Exclusions and Non-Starter Decompositions).

=back

=head2 EXPORT

C<NFC>, C<NFD>, C<NFKC>, C<NFKD>: by default.

C<normalize> and other some functions: on request.

=head1 AUTHOR

SADAHIRO Tomoyuki, <SADAHIRO@cpan.org>

  http://homepage1.nifty.com/nomenclator/perl/

  Copyright(C) 2001-2003, SADAHIRO Tomoyuki. Japan. All rights reserved.

  This module is free software; you can redistribute it
  and/or modify it under the same terms as Perl itself.

=head1 SEE ALSO

=over 4

=item http://www.unicode.org/reports/tr15/

Unicode Normalization Forms - UAX #15

=item http://www.unicode.org/Public/UNIDATA/DerivedNormalizationProps.txt

Derived Normalization Properties

=item http://www.unicode.org/notes/tn5/

Canonical Equivalence in Applications - UTN #5

=back

=cut

