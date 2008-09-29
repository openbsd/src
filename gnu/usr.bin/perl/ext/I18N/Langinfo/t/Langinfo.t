#!perl -T

BEGIN {
    if ($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }
}

use strict;
use Config;
use Test::More;

plan skip_all => "I18N::Langinfo or POSIX unavailable" 
    if $Config{'extensions'} !~ m!\bI18N/Langinfo\b!;

my @constants = qw(ABDAY_1 DAY_1 ABMON_1 MON_1 RADIXCHAR AM_STR THOUSEP D_T_FMT D_FMT T_FMT);

plan tests => 1 + 3 * @constants;

use_ok('I18N::Langinfo', 'langinfo', @constants);

for my $constant (@constants) {
    SKIP: {
        my $string = eval { langinfo(eval "$constant()") };
        is( $@, '', "calling langinfo() with $constant" );
        skip "returned string was empty, skipping next two tests", 2 unless $string;
        ok( defined $string, "checking if the returned string is defined" );
        cmp_ok( length($string), '>=', 1, "checking if the returned string has a positive length" );
    }
}

exit(0);

# Background: the langinfo() (in C known as nl_langinfo()) interface
# is supposed to be a portable way to fetch various language/country
# (locale) dependent constants like "the first day of the week" or
# "the decimal separator".  Give a portable (numeric) constant,
# get back a language-specific string.  That's a comforting fantasy.
# Now tune in for blunt reality: vendors seem to have implemented for
# those constants whatever they felt like implementing.  The UNIX
# standard says that one should have the RADIXCHAR constant for the
# decimal separator.  Not so for many Linux and BSD implementations.
# One should have the CODESET constant for returning the current
# codeset (say, ISO 8859-1).  Not so.  So let's give up any real
# testing (leave the old testing code here for old times' sake,
# though.) --jhi

my %want =
    (
     ABDAY_1	=> "Sun",
     DAY_1	=> "Sunday",
     ABMON_1	=> "Jan",
     MON_1	=> "January",
     RADIXCHAR	=> ".",
     AM_STR	=> qr{^(?:am|a\.m\.)$}i,
     THOUSEP	=> "",
     D_T_FMT	=> qr{^%a %b %[de] %H:%M:%S %Y$},
     D_FMT	=> qr{^%m/%d/%y$},
     T_FMT	=> qr{^%H:%M:%S$},
     );

    
my @want = sort keys %want;

print "1..", scalar @want, "\n";
    
for my $i (1..@want) {
    my $try = $want[$i-1];
    eval { I18N::Langinfo->import($try) };
    unless ($@) {
	my $got = langinfo(&$try);
	if (ref $want{$try} && $got =~ $want{$try} || $got eq $want{$try}) {
	    print qq[ok $i - $try is "$got"\n];
	} else {
	    print qq[not ok $i - $try is "$got" not "$want{$try}"\n];
	}
    } else {
	print qq[ok $i - Skip: $try not defined\n];
    }
}

