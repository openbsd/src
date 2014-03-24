#!./perl -w

# What does this test?
# This checks that all the perl "utils" don't have embarrassing syntax errors
#
# Why do we test this?
# Right now, without this, it's possible to pass the all the regression tests
# even if one has introduced syntax errors into scripts such as installperl
# or installman. No tests fail, so it's fair game to push the commit.
# Obviously this breaks installing perl, but we won't spot this.
# Whilst we can't easily test that the various scripts *work*, we can at least
# check that we've not made any trivial screw ups.
#
# It's broken - how do I fix it?
# Presumably it's failed because some (other) code that you changed was (also)
# used by one of the utility scripts. So you'll have to manually test that
# script.

BEGIN {
    @INC = '..' if -f '../TestInit.pm';
}
use TestInit qw(T); # T is chdir to the top level
use strict;

require 't/test.pl';

# It turns out that, since the default @INC will include your old 5.x libs, if
# you have them, the Porting utils might load a library that no longer compiles
# clean.  This actually happened, with Local::Maketext::Lexicon from a 5.10.0
# preventing 5.16.0-RC0 from testing successfully.  This test is really only
# needed for porters, anyway.  -- rjbs, 2012-05-10
find_git_or_skip('all');

my @maybe;

open my $fh, '<', 'MANIFEST' or die "Can't open MANIFEST: $!";
while (<$fh>) {
    push @maybe, $1 if m!^(Porting/\S+)!;
}
close $fh or die $!;

open $fh, '<', 'utils.lst' or die "Can't open utils.lst: $!";
while (<$fh>) {
    die unless  m!^(\S+)!;
    push @maybe, $1;
    $maybe[$#maybe] .= '.com' if $^O eq 'VMS';
}
close $fh or die $!;

my @victims = (qw(installman installperl regen_perly.pl));
my %excuses = (
               'Porting/git-deltatool' => 'Git::Wrapper',
               'Porting/podtidy' => 'Pod::Tidy',
               'Porting/leakfinder.pl' => 'XS::APItest',
              );

foreach (@maybe) {
    if (/\.p[lm]$/) {
        push @victims, $_;
    } elsif ($_ !~ m{^x2p/a2p}) {
        # test_prep doesn't (yet) have a dependency on a2p, so it seems a bit
        # silly adding one (and forcing it to be built) just so that we can open
        # it and determine that it's *not* a perl program, and hence of no
        # further interest to us.
        open $fh, '<', $_ or die "Can't open '$_': $!";
        my $line = <$fh>;
        if ($line =~ m{^#!(?:\S*|/usr/bin/env\s+)perl}
	    || $^O eq 'VMS' && $line =~ m{^\$ perl}) {
            push @victims, $_;
        } else {
            print "# $_ isn't a Perl script\n";
        }
    }
}

printf "1..%d\n", scalar @victims;

foreach my $victim (@victims) {
 SKIP: {
        # Not clear to me *why* it needs the BEGIN block, given what it
        # does, but not in an easy position to change it.
        skip("$victim executes code in a BEGIN block which fails for empty \@ARGV")
            if $victim =~ m{^utils/cpanp-run-perl};

        skip ("$victim uses $excuses{$victim}, so can't test with just core modules")
            if $excuses{$victim};

        my $got = runperl(switches => ['-c'], progfile => $victim, stderr => 1);
        is($got, "$victim syntax OK\n", "$victim compiles");
    }
}

# Local variables:
# cperl-indent-level: 4
# indent-tabs-mode: nil
# End:
#
# ex: set ts=8 sts=4 sw=4 et:
