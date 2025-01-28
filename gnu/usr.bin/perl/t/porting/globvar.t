#!perl -w

use TestInit qw(T);
use strict;
use Config;

require './t/test.pl';

skip_all("Code to read symbols not ported to $^O")
    if $^O eq 'VMS' or $^O eq 'MSWin32';

# Not investigated *why* we don't export these, but we don't, and we've not
# received any bug reports about it causing problems:
my %skip = map { ("PL_$_", 1) }
    qw(
	  DBcv bitcount cshname generation lastgotoprobe
	  mod_latin1_uc modcount no_symref_sv uudmap
	  watchaddr watchok warn_uninit_sv hash_chars
     );

$skip{PL_hash_rand_bits}= $skip{PL_hash_rand_bits_enabled}= 1; # we can be compiled without these, so skip testing them
$skip{PL_warn_locale}= 1; # we can be compiled without locales, so skip testing them

# -P is POSIX and defines an expected format, while the default
# output will vary from platform to platform
my $trial = "$Config{nm} -P globals$Config{_o} 2>&1";
my $yes = `$trial`;

skip_all("Could not run `$trial`") if $?;

my $defined = qr/\s+[^Uu]\s+/m;

skip_all("Could not spot definition of PL_Yes in output of `$trial`")
    unless $yes =~ /^_?PL_Yes${defined}/m;

my %exported;
open my $fh, '-|', $^X, '-Ilib', './makedef.pl', 'PLATFORM=test'
    or die "Can't run makedef.pl";

while (<$fh>) {
    next unless /^PL_/;
    chomp;
    ++$exported{$_};
}

close $fh or die "Problem running makedef.pl";

# AIX can list a symbol as both a local and a global symbol
# so collect all of the symbols *then* process them
my %defined;
foreach my $file (map {$_ . $Config{_o}} qw(globals regcomp)) {
    open $fh, '-|', $Config{nm}, "-P", $file
	or die "Can't run nm $file";

    while (<$fh>) {
	next unless /^_?(PL_\S+)${defined}/;
        $defined{$1} = 1;
    }
    close $fh or die "Problem running nm $file";
}

my %unexported;
for my $name (sort keys %defined) {
    if (delete $exported{$name}) {
        note("Seen definition of $name");
        next;
    }
    ++$unexported{$name};
}

unless ($Config{d_double_has_inf}) {
    $skip{PL_inf}++;
}
unless ($Config{d_double_has_nan}) {
    $skip{PL_nan}++;
}

foreach (sort keys %exported) {
 SKIP: {
    skip("We dont't export '$_' (Perl not built with this enabled?)",1) if $skip{$_};
    fail("Attempting to export '$_' which is never defined");
 }
}

$::TODO = $::TODO; # silence uninitialized warnings
foreach (sort keys %unexported) {
 SKIP: {
        skip("We don't export '$_'", 1) if $skip{$_};
        TODO: {
            local $::TODO = "HPUX exports everything" if $^O eq "hpux";
            fail("'$_' is defined, but we do not export it");
        }
    }
}

done_testing();
