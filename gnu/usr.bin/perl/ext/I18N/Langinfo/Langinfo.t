#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ m!\bI18N/Langinfo\b! ||
	$Config{'extensions'} !~ m!\bPOSIX\b!)
    {
	print "1..0 # skip: I18N::Langinfo or POSIX unavailable\n";
	exit 0;
    }
}
    
use I18N::Langinfo qw(langinfo);
use POSIX qw(setlocale LC_ALL);

setlocale(LC_ALL, $ENV{LC_ALL} = $ENV{LANG} = "C");

print "1..1\n"; # We loaded okay.  That's about all we can hope for.
print "ok 1\n";
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

