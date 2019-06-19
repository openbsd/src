
BEGIN {
    unless ('A' eq pack('U', 0x41)) {
	print "1..0 # Unicode::Collate cannot pack a Unicode code point\n";
	exit 0;
    }
    unless (0x41 == unpack('U', 'A')) {
	print "1..0 # Unicode::Collate cannot get a Unicode code point\n";
	exit 0;
    }
    if ($ENV{PERL_CORE}) {
	chdir('t') if -d 't';
	@INC = $^O eq 'MacOS' ? qw(::lib) : qw(../lib);
    }
}

use strict;
use warnings;
BEGIN { $| = 1; print "1..134\n"; }
my $count = 0;
sub ok ($;$) {
    my $p = my $r = shift;
    if (@_) {
	my $x = shift;
	$p = !defined $x ? !defined $r : !defined $r ? 0 : $r eq $x;
    }
    print $p ? "ok" : "not ok", ' ', ++$count, "\n";
}

use Unicode::Collate::Locale;

ok(1);

#########################

our (@listEs, @listEsT, @listFr);

@listEs = qw(
    cambio camella camello camelo Camer�n
    chico chile Chile CHILE chocolate
    cielo curso espacio espanto espa�ol esperanza lama l�quido
    llama Llama LLAMA llamar luz nos nueve �u ojo
);

@listEsT = qw(
    cambio camelo camella camello Camer�n cielo curso
    chico chile Chile CHILE chocolate
    espacio espanto espa�ol esperanza lama l�quido luz
    llama Llama LLAMA llamar nos nueve �u ojo
);

@listFr = (
  qw(
    cadurcien c�cum c�CUM C�CUM C�CUM caennais c�sium cafard
    coercitif cote c�te C�te cot� Cot� c�t� C�t� coter
    �l�ve �lev� g�ne g�ne M�CON ma�on
    p�che P�CHE p�che P�CHE p�ch� P�CH� p�cher p�cher
    rel�ve relev� r�v�le r�v�l�
    sur�l�vation s�rement sur�minent s�ret�
    vice-consul vicennal vice-pr�sident vice-roi vic�simal),
  "vice versa", "vice-versa",
);

ok(@listEs,  27);
ok(@listEsT, 27);
ok(@listFr,  46);

ok(Unicode::Collate::Locale::_locale('es_MX'), 'es');
ok(Unicode::Collate::Locale::_locale('en_CA'), 'default');

# 6

my $Collator = Unicode::Collate::Locale->
    new(normalization => undef);
ok($Collator->getlocale, 'default');

ok(
  join(':', $Collator->sort(
    qw/ lib strict Carp ExtUtils CGI Time warnings Math overload Pod CPAN /
  ) ),
  join(':',
    qw/ Carp CGI CPAN ExtUtils lib Math overload Pod strict Time warnings /
  ),
);

ok($Collator->cmp("", ""), 0);
ok($Collator->eq("", ""));
ok($Collator->cmp("", "perl"), -1);
ok($Collator->gt("PERL", "perl"));

# 12

$Collator->change(level => 2);

ok($Collator->eq("PERL", "perl"));

my $objEs  = Unicode::Collate::Locale->new
    (normalization => undef, locale => 'ES');
ok($objEs->getlocale, 'es');

my $objEsT = Unicode::Collate::Locale->new
    (normalization => undef, locale => 'es_ES_traditional');
ok($objEsT->getlocale, 'es__traditional');

my $objFr  = Unicode::Collate::Locale->new
    (normalization => undef, locale => 'FR_CA');
ok($objFr->getlocale, 'fr_CA');

# 16

sub randomize { my %hash; @hash{@_} = (); keys %hash; } # ?!

for (my $i = 0; $i < $#listEs; $i++) {
    ok($objEs->lt($listEs[$i], $listEs[$i+1]));
}
# 42

for (my $i = 0; $i < $#listEsT; $i++) {
    ok($objEsT->lt($listEsT[$i], $listEsT[$i+1]));
}
# 68

for (my $i = 0; $i < $#listFr; $i++) {
    ok($objFr->lt($listFr[$i], $listFr[$i+1]));
}
# 113

our @randEs = randomize(@listEs);
our @sortEs = $objEs->sort(@randEs);
ok("@sortEs" eq "@listEs");

our @randEsT = randomize(@listEsT);
our @sortEsT = $objEsT->sort(@randEsT);
ok("@sortEsT" eq "@listEsT");

our @randFr = randomize(@listFr);
our @sortFr = $objFr->sort(@randFr);
ok("@sortFr" eq "@listFr");

# 116

{
    my $keyXS = '__useXS'; # see Unicode::Collate internal
    my $noLoc = Unicode::Collate->new(normalization => undef);
    my $UseXS = ref($noLoc->{$keyXS});
    ok(ref($Collator->{$keyXS}), $UseXS);
    ok(ref($objFr   ->{$keyXS}), $UseXS);
    ok(ref($objEs   ->{$keyXS}), $UseXS);
    ok(ref($objEsT  ->{$keyXS}), $UseXS);
}
# 120

ok(Unicode::Collate::Locale::_locale('sr'),            'sr');
ok(Unicode::Collate::Locale::_locale('sr_Cyrl'),       'sr');
ok(Unicode::Collate::Locale::_locale('sr_Latn'),       'sr_Latn');
ok(Unicode::Collate::Locale::_locale('sr_LATN'),       'sr_Latn');
ok(Unicode::Collate::Locale::_locale('sr_latn'),       'sr_Latn');
ok(Unicode::Collate::Locale::_locale('de'),            'default');
ok(Unicode::Collate::Locale::_locale('de_phone'),      'de__phonebook');
ok(Unicode::Collate::Locale::_locale('de__phonebook'), 'de__phonebook');
ok(Unicode::Collate::Locale::_locale('de-phonebk'),    'de__phonebook');
ok(Unicode::Collate::Locale::_locale('de--phonebk'),   'de__phonebook');

# 130

my $objEs2  = Unicode::Collate::Locale->new
    (normalization => undef, locale => 'ES',
     level => 1,
     entry => << 'ENTRIES',
0000      ; [.FFFE.0020.0005.0000]
00F1      ; [.0010.0020.0002.00F1] # LATIN SMALL LETTER N WITH TILDE
006E 0303 ; [.0010.0020.0002.00F1] # LATIN SMALL LETTER N WITH TILDE
ENTRIES
);

ok($objEs2->lt("abc\x{4E00}", "abc\0"));
ok($objEs2->lt("abc\x{FFFD}", "abc\0"));
ok($objEs2->lt("abc\x{FFFD}", "abc\0"));
ok($objEs2->lt("n\x{303}", "N\x{303}"));

# 134

