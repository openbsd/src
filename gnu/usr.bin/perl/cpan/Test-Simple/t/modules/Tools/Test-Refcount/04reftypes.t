#!/usr/bin/perl

use strict;
use warnings;

use Test2::API;
use Test2::Tools::Basic;
use Test2::API qw(intercept context);
use Test2::Tools::Compare qw/match subset array event like/;

use Symbol qw( gensym );

use Test2::Tools::Refcount;

my %refs = (
    SCALAR => do { my $var; \$var },
    ARRAY    => [],
    HASH     => +{},
    # This magic is to ensure the code ref is new, not shared. To be a new one
    # it has to contain a unique pad.
    CODE     => do { my $var; sub { $var } },
    GLOB     => gensym(),
    Regex    => qr/foo/,
);

foreach my $type (qw( SCALAR ARRAY HASH CODE GLOB Regex )) {
    SKIP: {
        if( $type eq "Regex" and $] >= 5.011 ) {
            # Perl v5.11 seems to have odd behaviour with Regexp references. They start
            # off with a refcount of 2. Not sure if this is a bug in Perl, or my
            # assumption. Until P5P have worked it out, we'll skip this. See also
            # similar skip logic in Devel-Refcount's tests
            skip "Bleadperl", 1;
        }

        like(
            intercept {
                is_refcount($refs{$type}, 1, "anon $type ref");
            },
            array {
                event Ok => { name => "anon $type ref", pass => 1 };
            },
            'anon ARRAY ref succeeds'
        );
    }
}

done_testing;
