#!/usr/bin/perl -w

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
use Test::More tests => 14;
use TieOut;

BEGIN { $^W = 1; }

my $warnings = '';
local $SIG{__WARN__} = sub { $warnings .= join '', @_ };

is( undef, undef,           'undef is undef');
is( $warnings, '',          '  no warnings' );

isnt( undef, 'foo',         'undef isnt foo');
is( $warnings, '',          '  no warnings' );

isnt( undef, '',            'undef isnt an empty string' );
isnt( undef, 0,             'undef isnt zero' );

like( undef, '/.*/',        'undef is like anything' );
is( $warnings, '',          '  no warnings' );

eq_array( [undef, undef], [undef, 23] );
is( $warnings, '',          'eq_array()  no warnings' );

eq_hash ( { foo => undef, bar => undef },
          { foo => undef, bar => 23 } );
is( $warnings, '',          'eq_hash()   no warnings' );

eq_set  ( [undef, undef, 12], [29, undef, undef] );
is( $warnings, '',          'eq_set()    no warnings' );


eq_hash ( { foo => undef, bar => { baz => undef, moo => 23 } },
          { foo => undef, bar => { baz => undef, moo => 23 } } );
is( $warnings, '',          'eq_hash()   no warnings' );


my $tb = Test::More->builder;

use TieOut;
my $caught = tie *CATCH, 'TieOut';
my $old_fail = $tb->failure_output;
$tb->failure_output(\*CATCH);
diag(undef);
$tb->failure_output($old_fail);

is( $caught->read, "# undef\n" );
is( $warnings, '',          'diag(undef)  no warnings' );
