#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

chdir 't';

use MakeMaker::Test::Utils;
use Test::More tests => 6;
use File::Spec;

my $TB = Test::More->builder;

BEGIN { use_ok('ExtUtils::MM') }

my $mm = bless { NAME => "Foo" }, 'MM';
isa_ok($mm, 'ExtUtils::MakeMaker');
isa_ok($mm, 'ExtUtils::MM_Any');


sub try_oneliner {
    my($code, $switches, $expect, $name) = @_;
    my $cmd = $mm->oneliner($code, $switches);
    $cmd =~ s{\$\(PERLRUN\)}{$^X};

    # VMS likes to put newlines at the end of commands if there isn't
    # one already.
    $expect =~ s/([^\n])\z/$1\n/ if $^O eq 'VMS';

    $TB->is_eq(scalar `$cmd`, $expect, $name) || $TB->diag("oneliner:\n$cmd");
}

# Lets see how it deals with quotes.
try_oneliner(q{print "foo'o", ' bar"ar'}, [],  q{foo'o bar"ar},  'quotes');

# How about dollar signs?
try_oneliner(q{$PATH = 'foo'; print $PATH},[], q{foo},   'dollar signs' );

# switches?
try_oneliner(q{print 'foo'}, ['-l'],           "foo\n",       'switches' );

# XXX gotta rethink the newline test.  The Makefile does newline
# escaping, then the shell.

