#!/usr/bin/perl -w

use strict;
use lib 't/lib';

use Test::More;

use constant LIBS  => 'lib/';
use constant FIRST => 'TAP::Parser';

read_manifest( 'MANIFEST',             my $manifest             = {} );
read_manifest( 'MANIFEST.CUMMULATIVE', my $manifest_cummulative = {} );

my @classes = uniq(
    FIRST,
    map { file_to_mod($_) } filter_lib( keys %$manifest )
);

plan tests => @classes * 2 + 1;

for my $class (@classes) {
    use_ok $class or BAIL_OUT("Could not load $class");
    is $class->VERSION, TAP::Parser->VERSION,
      "... and $class should have the correct version";
}

my @orphans = diff(
    [ filter_lib( keys %$manifest ) ],
    [ filter_lib( keys %$manifest_cummulative ) ]
);
my @waifs = intersection( \@orphans, [ keys %INC ] );
unless ( ok 0 == @waifs, 'no old versions loaded' ) {
    diag "\nThe following modules were loaded in error:\n";
    for my $waif ( sort @waifs ) {
        diag sprintf "  %s (%s)\n", file_to_mod($waif), $INC{$waif};
    }
    diag "\n";
}

diag("Testing Test::Harness $Test::Harness::VERSION, Perl $], $^X")
  unless $ENV{PERL_CORE};

sub intersection {
    my ( $la, $lb ) = @_;
    my %seen = map { $_ => 1 } @$la;
    return grep { $seen{$_} } @$lb;
}

sub diff {
    my ( $la, $lb ) = @_;
    my %seen = map { $_ => 1 } @$la;
    return grep { !$seen{$_}++ } @$lb;
}

sub uniq {
    my %seen = ();
    grep { !$seen{$_}++ } @_;
}

sub lib_matcher {
    my @libs = @_;
    my $re = join ')|(', map quotemeta, @libs;
    return qr{^($re)};
}

sub filter_lib {
    my $matcher = lib_matcher(LIBS);
    return map { s{$matcher}{}; $_ }
      grep {m{$matcher.+?\.pm$}} sort @_;
}

sub mod_to_file {
    my $mod = shift;
    $mod =~ s{::}{/}g;
    return "$mod.pm";
}

sub file_to_mod {
    my $file = shift;
    $file =~ s{/}{::}g;
    $file =~ s{\.pm$}{};
    return $file;
}

sub read_manifest {
    my ( $file, $into ) = @_;
    open my $fh, '<', $file or die "Can't read $file: $!";
    while (<$fh>) {
        chomp;
        s/\s*#.*//;
        $into->{$_}++ if length $_;
    }
    return;
}

