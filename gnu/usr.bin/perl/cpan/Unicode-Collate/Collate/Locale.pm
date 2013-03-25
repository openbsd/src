package Unicode::Collate::Locale;

use strict;
use Carp;
use base qw(Unicode::Collate);

our $VERSION = '0.89';

my $PL_EXT  = '.pl';

my %LocaleFile = map { ($_, $_) } qw(
   af ar as az be bg bn ca cs cy da eo es et fa fi fil fo fr
   gu ha haw hi hr hu hy ig is ja kk kl kn ko kok ln lt lv
   mk ml mr mt nb nn nso om or pa pl ro ru sa se si sk sl sq
   sr sv ta te th tn to tr uk ur vi wae wo yo zh
);
   $LocaleFile{'default'} = '';
# aliases
   $LocaleFile{'bs'}      = 'hr';
   $LocaleFile{'sr_Latn'} = 'hr';
# short file names
   $LocaleFile{'de__phonebook'}   = 'de_phone';
   $LocaleFile{'es__traditional'} = 'es_trad';
   $LocaleFile{'fi__phonebook'}   = 'fi_phone';
   $LocaleFile{'si__dictionary'}  = 'si_dict';
   $LocaleFile{'sv__reformed'}    = 'sv_refo';
   $LocaleFile{'zh__big5han'}     = 'zh_big5';
   $LocaleFile{'zh__gb2312han'}   = 'zh_gb';
   $LocaleFile{'zh__pinyin'}      = 'zh_pin';
   $LocaleFile{'zh__stroke'}      = 'zh_strk';

my %TypeAlias = qw(
    phone     phonebook
    phonebk   phonebook
    dict      dictionary
    reform    reformed
    trad      traditional
    big5      big5han
    gb2312    gb2312han
);

sub _locale {
    my $locale = shift;
    if ($locale) {
	$locale = lc $locale;
	$locale =~ tr/\-\ \./_/;
	$locale =~ s/_([0-9a-z]+)\z/$TypeAlias{$1} ?
				  "_$TypeAlias{$1}" : "_$1"/e;
	$LocaleFile{$locale} and return $locale;

	my @code = split /_/, $locale;
	my $lan = shift @code;
	my $scr = @code && length $code[0] == 4 ? ucfirst shift @code : '';
	my $reg = @code && length $code[0] <  4 ? uc      shift @code : '';
	my $var = @code                         ?         shift @code : '';

	my @list;
	push @list, (
	    "${lan}_${scr}_${reg}_$var",
	    "${lan}_${scr}__$var", # empty $scr should not be ${lan}__$var.
	    "${lan}_${reg}_$var",  # empty $reg may be ${lan}__$var.
	    "${lan}__$var",
	) if $var ne '';
	push @list, (
	    "${lan}_${scr}_${reg}",
	    "${lan}_${scr}",
	    "${lan}_${reg}",
	     ${lan},
	);
	for my $loc (@list) {
	    $LocaleFile{$loc} and return $loc;
	}
    }
    return 'default';
}

sub getlocale {
    return shift->{accepted_locale};
}

sub locale_version {
    return shift->{locale_version};
}

sub _fetchpl {
    my $accepted = shift;
    my $f = $LocaleFile{$accepted};
    return if !$f;
    $f .= $PL_EXT;

    # allow to search @INC
#   use File::Spec;
#   my $path = File::Spec->catfile('Unicode', 'Collate', 'Locale', $f);
    my $path = "Unicode/Collate/Locale/$f";
    my $h = do $path;
    croak "Unicode/Collate/Locale/$f can't be found" if !$h;
    return $h;
}

sub new {
    my $class = shift;
    my %hash = @_;
    $hash{accepted_locale} = _locale($hash{locale});

    if (exists $hash{table}) {
	croak "your table can't be used with Unicode::Collate::Locale";
    }

    my $href = _fetchpl($hash{accepted_locale});
    while (my($k,$v) = each %$href) {
	if (exists $hash{$k}) {
	    croak "$k is reserved by $hash{locale}, can't be overwritten";
	}
	$hash{$k} = $v;
    }
    return $class->SUPER::new(%hash);
}

1;
__END__

MEMORANDA for developing

locale		based CLDR
----------------------------------------------------------------------------
af		2.0 = 1.8.1
ar		2.0
as		2.0 = 1.8.1
az		2.0 = 1.8.1 (type="standard")
be		2.0
bg		2.0
bn		2.0.1 (type="standard")
bs		2.0 (alias source="hr")
ca		2.0 = 1.8.1 (alt="proposed" type="standard")
cs		2.0 = 1.8.1 (type="standard")
cy		2.0 = 1.8.1
da		2.0 = 1.8.1 (type="standard") [modify aA to pass CLDR tests]
de__phonebook	2.0 (type="phonebook")
eo		2.0 = 1.8.1
es		2.0 (type="standard")
es__traditional	2.0 = 1.8.1 (type="traditional")
et		2.0 = 1.8.1
fa		2.0 = 1.8.1
fi		2.0 = 1.8.1 (type="standard" alt="proposed")
fi__phonebook	2.0 = 1.8.1 (type="phonebook")
fil		2.0 (type="standard") = 1.8.1
fo		2.0 = 1.8.1 (alt="proposed" type="standard")
fr		2.0 (fr_CA, backwards="on")
gu		2.0 (type="standard")
ha		2.0
haw		2.0 = 1.8.1
hi		2.0 (type="standard")
hr		2.0 (type="standard")
hu		2.0 = 1.8.1 (alt="proposed" type="standard")
hy		2.0 = 1.8.1
ig		2.0 = 1.8.1
is		2.0 = 1.8.1 (type="standard")
ja		2.0 = 1.8.1 (type="standard")
kk		2.0
kl		2.0 = 1.8.1 (type="standard")
kn		2.0 (type="standard")
ko		2.0 = 1.8.1 (type="standard")
kok		2.0 = 1.8.1
ln		2.0 (type="standard") = 1.8.1
lt		2.0
lv		2.0 (type="standard") = 1.8.1
mk		2.0
ml		2.0
mr		2.0 = 1.8.1
mt		2.0
nb		2.0 (type="standard")
nn		2.0 (type="standard")
nso		2.0 = 1.8.1
om		2.0 = 1.8.1
or		2.0
pa		2.0 = 1.8.1
pl		2.0 = 1.8.1
ro		2.0 (type="standard")
ru		2.0
sa		1.8.1 (type="standard" alt="proposed") [currently in /seed]
se		2.0 = 1.8.1 (type="standard")
si		2.0 (type="standard")
si__dictionary	2.0 (type="dictionary")
sk		2.0 (type="standard")
sl		2.0 = 1.8.1 (type="standard" alt="proposed")
sq		2.0 = 1.8.1 (alt="proposed" type="standard")
sr		2.0 (type="standard")
sr_Latn		2.0 = 1.8.1 (alias source="hr")
sv		2.0 (type="standard")
sv__reformed	2.0 = 1.8.1 (type="reformed")
ta		2.0
te		2.0
th		2.0 (type="standard")
tn		2.0 = 1.8.1
to		2.0 = 1.8.1 (type="standard" alt="proposed")
tr		2.0 = 1.8.1 (type="standard")
uk		2.0
ur		2.0
vi		2.0 = 1.8.1
wae		2.0
wo		1.8.1 [currently in /seed]
yo		2.0 = 1.8.1
zh		2.0 = 1.8.1 (type="standard")
zh__big5han	2.0 = 1.8.1 (type="big5han")
zh__gb2312han	2.0 = 1.8.1 (type="gb2312han")
zh__pinyin	2.0 (type='pinyin' alt='short')
zh__stroke	2.0 = 1.9.1 (type='stroke' alt='short')
----------------------------------------------------------------------------

=head1 NAME

Unicode::Collate::Locale - Linguistic tailoring for DUCET via Unicode::Collate

=head1 SYNOPSIS

  use Unicode::Collate::Locale;

  #construct
  $Collator = Unicode::Collate::Locale->
      new(locale => $locale_name, %tailoring);

  #sort
  @sorted = $Collator->sort(@not_sorted);

  #compare
  $result = $Collator->cmp($a, $b); # returns 1, 0, or -1.

B<Note:> Strings in C<@not_sorted>, C<$a> and C<$b> are interpreted
according to Perl's Unicode support. See L<perlunicode>,
L<perluniintro>, L<perlunitut>, L<perlunifaq>, L<utf8>.
Otherwise you can use C<preprocess> (cf. C<Unicode::Collate>)
or should decode them before.

=head1 DESCRIPTION

This module provides linguistic tailoring for it
taking advantage of C<Unicode::Collate>.

=head2 Constructor

The C<new> method returns a collator object.

A parameter list for the constructor is a hash, which can include
a special key C<locale> and its value (case-insensitive) standing
for a Unicode base language code (two or three-letter).
For example, C<Unicode::Collate::Locale-E<gt>new(locale =E<gt> 'FR')>
returns a collator tailored for French.

C<$locale_name> may be suffixed with a Unicode script code (four-letter),
a Unicode region code, a Unicode language variant code. These codes are
case-insensitive, and separated with C<'_'> or C<'-'>.
E.g. C<en_US> for English in USA,
C<az_Cyrl> for Azerbaijani in the Cyrillic script,
C<es_ES_traditional> for Spanish in Spain (Traditional).

If C<$locale_name> is not available,
fallback is selected in the following order:

    1. language with a variant code
    2. language with a script code
    3. language with a region code
    4. language
    5. default

Tailoring tags provided by C<Unicode::Collate> are allowed as long as
they are not used for C<locale> support.  Esp. the C<table> tag
is always untailorable, since it is reserved for DUCET.

E.g. a collator for French, which ignores diacritics and case difference
(i.e. level 1), with reversed case ordering and no normalization.

    Unicode::Collate::Locale->new(
        level => 1,
        locale => 'fr',
        upper_before_lower => 1,
        normalization => undef
    )

Overriding a behavior already tailored by C<locale> is disallowed
if such a tailoring is passed to C<new()>.

    Unicode::Collate::Locale->new(
        locale => 'da',
        upper_before_lower => 0, # causes error as reserved by 'da'
    )

However C<change()> inherited from C<Unicode::Collate> allows
such a tailoring that is reserved by C<locale>. Examples:

    new(locale => 'ca')->change(backwards => undef)
    new(locale => 'da')->change(upper_before_lower => 0)
    new(locale => 'ja')->change(overrideCJK => undef)

=head2 Methods

C<Unicode::Collate::Locale> is a subclass of C<Unicode::Collate>
and methods other than C<new> are inherited from C<Unicode::Collate>.

Here is a list of additional methods:

=over 4

=item C<$Collator-E<gt>getlocale>

Returns a language code accepted and used actually on collation.
If linguistic tailoring is not provided for a language code you passed
(intensionally for some languages, or due to the incomplete implementation),
this method returns a string C<'default'> meaning no special tailoring.

=item C<$Collator-E<gt>locale_version>

(Since Unicode::Collate::Locale 0.87)
Returns the version number (perhaps C</\d\.\d\d/>) of the locale, as that
of F<Locale/*.pl>.

B<Note:> F<Locale/*.pl> that a collator uses should be identified by
a combination of return values from C<getlocale> and C<locale_version>.

=back

=head2 A list of tailorable locales

      locale name       description
    --------------------------------------------------------------
      af                Afrikaans
      ar                Arabic
      as                Assamese
      az                Azerbaijani (Azeri)
      be                Belarusian
      bg                Bulgarian
      bn                Bengali
      bs                Bosnian
      ca                Catalan
      cs                Czech
      cy                Welsh
      da                Danish
      de__phonebook     German (umlaut as 'ae', 'oe', 'ue')
      eo                Esperanto
      es                Spanish
      es__traditional   Spanish ('ch' and 'll' as a grapheme)
      et                Estonian
      fa                Persian
      fi                Finnish (v and w are primary equal)
      fi__phonebook     Finnish (v and w as separate characters)
      fil               Filipino
      fo                Faroese
      fr                French
      gu                Gujarati
      ha                Hausa
      haw               Hawaiian
      hi                Hindi
      hr                Croatian
      hu                Hungarian
      hy                Armenian
      ig                Igbo
      is                Icelandic
      ja                Japanese [1]
      kk                Kazakh
      kl                Kalaallisut
      kn                Kannada
      ko                Korean [2]
      kok               Konkani
      ln                Lingala
      lt                Lithuanian
      lv                Latvian
      mk                Macedonian
      ml                Malayalam
      mr                Marathi
      mt                Maltese
      nb                Norwegian Bokmal
      nn                Norwegian Nynorsk
      nso               Northern Sotho
      om                Oromo
      or                Oriya
      pa                Punjabi
      pl                Polish
      ro                Romanian
      ru                Russian
      sa                Sanskrit
      se                Northern Sami
      si                Sinhala
      si__dictionary    Sinhala (U+0DA5 = U+0DA2,0DCA,0DA4)
      sk                Slovak
      sl                Slovenian
      sq                Albanian
      sr                Serbian
      sr_Latn           Serbian in Latin (tailored as Croatian)
      sv                Swedish (v and w are primary equal)
      sv__reformed      Swedish (v and w as separate characters)
      ta                Tamil
      te                Telugu
      th                Thai
      tn                Tswana
      to                Tonga
      tr                Turkish
      uk                Ukrainian
      ur                Urdu
      vi                Vietnamese
      wae               Walser
      wo                Wolof
      yo                Yoruba
      zh                Chinese
      zh__big5han       Chinese (ideographs: big5 order)
      zh__gb2312han     Chinese (ideographs: GB-2312 order)
      zh__pinyin        Chinese (ideographs: pinyin order) [3]
      zh__stroke        Chinese (ideographs: stroke order) [3]
    --------------------------------------------------------------

Locales according to the default UCA rules include
chr (Cherokee),
de (German),
en (English),
ga (Irish),
id (Indonesian),
it (Italian),
ka (Georgian),
ms (Malay),
nl (Dutch),
pt (Portuguese),
st (Southern Sotho),
sw (Swahili),
xh (Xhosa),
zu (Zulu).

B<Note>

[1] ja: Ideographs are sorted in JIS X 0208 order.
Fullwidth and halfwidth forms are identical to their normal form.
The difference between hiragana and katakana is at the 4th level,
the comparison also requires C<(variable =E<gt> 'Non-ignorable')>,
and then C<katakana_before_hiragana> has no effect.

[2] ko: Plenty of ideographs are sorted by their reading. Such
an ideograph is primary (level 1) equal to, and secondary (level 2)
greater than, the corresponding hangul syllable.

[3] zh__pinyin and zh__stroke: implemented alt='short', where
a smaller number of ideographs are tailored.

=head1 INSTALL

Installation of C<Unicode::Collate::Locale> requires F<Collate/Locale.pm>,
F<Collate/Locale/*.pm>, F<Collate/CJK/*.pm> and F<Collate/allkeys.txt>.
On building, C<Unicode::Collate::Locale> doesn't require any of F<data/*.txt>,
F<gendata/*>, and F<mklocale>.
Tests for C<Unicode::Collate::Locale> are named F<t/loc_*.t>.

=head1 CAVEAT

=over 4

=item tailoring is not maximum

Even if a certain letter is tailored, its equivalent would not always
tailored as well as it. For example, even though W is tailored,
fullwidth W (C<U+FF37>), W with acute (C<U+1E82>), etc. are not
tailored. The result may depend on whether source strings are
normalized or not, and whether decomposed or composed.
Thus C<(normalization =E<gt> undef)> is less preferred.

=back

=head1 AUTHOR

The Unicode::Collate::Locale module for perl was written
by SADAHIRO Tomoyuki, <SADAHIRO@cpan.org>.
This module is Copyright(C) 2004-2012, SADAHIRO Tomoyuki. Japan.
All rights reserved.

This module is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=head1 SEE ALSO

=over 4

=item Unicode Collation Algorithm - UTS #10

L<http://www.unicode.org/reports/tr10/>

=item The Default Unicode Collation Element Table (DUCET)

L<http://www.unicode.org/Public/UCA/latest/allkeys.txt>

=item Unicode Locale Data Markup Language (LDML) - UTS #35

L<http://www.unicode.org/reports/tr35/>

=item CLDR - Unicode Common Locale Data Repository

L<http://cldr.unicode.org/>

=item L<Unicode::Collate>

=item L<Unicode::Normalize>

=back

=cut
