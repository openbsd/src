#!/usr/bin/perl -w
# $Id: note.t,v 1.1 2009/05/16 21:42:57 simon Exp $

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;
use warnings;

use TieOut;

use Test::More tests => 2;

{
    my $test = Test::More->builder;

    my $output          = tie *FAKEOUT, "TieOut";
    my $fail_output     = tie *FAKEERR, "TieOut";
    $test->output        (*FAKEOUT);
    $test->failure_output(*FAKEERR);

    note("foo");

    $test->reset_outputs;

    is $output->read,      "# foo\n";
    is $fail_output->read, '';
}

