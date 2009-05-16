#!/usr/bin/perl -w
# $Id$

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
use Test::More tests => 20;
use TieOut;

BEGIN { $^W = 1; }

my $warnings = '';
local $SIG{__WARN__} = sub { $warnings .= join '', @_ };

my $TB = Test::Builder->new;
sub no_warnings {
    $TB->is_eq($warnings, '', '  no warnings');
    $warnings = '';
}

sub warnings_is {
    $TB->is_eq($warnings, $_[0]);
    $warnings = '';
}

sub warnings_like {
    $TB->like($warnings, $_[0]);
    $warnings = '';
}


my $Filename = quotemeta $0;
   

is( undef, undef,           'undef is undef');
no_warnings;

isnt( undef, 'foo',         'undef isnt foo');
no_warnings;

isnt( undef, '',            'undef isnt an empty string' );
isnt( undef, 0,             'undef isnt zero' );

Test::More->builder->is_num(undef, undef, 'is_num()');
Test::More->builder->isnt_num(23, undef,  'isnt_num()');

#line 45
like( undef, '/.*/',        'undef is like anything' );
warnings_like(qr/Use of uninitialized value.* at $Filename line 45\.\n/);

eq_array( [undef, undef], [undef, 23] );
no_warnings;

eq_hash ( { foo => undef, bar => undef },
          { foo => undef, bar => 23 } );
no_warnings;

eq_set  ( [undef, undef, 12], [29, undef, undef] );
no_warnings;


eq_hash ( { foo => undef, bar => { baz => undef, moo => 23 } },
          { foo => undef, bar => { baz => undef, moo => 23 } } );
no_warnings;


#line 64
cmp_ok( undef, '<=', 2, '  undef <= 2' );
warnings_like(qr/Use of uninitialized value.* at cmp_ok \[from $Filename line 64\] line 1\.\n/);



my $tb = Test::More->builder;

use TieOut;
my $caught = tie *CATCH, 'TieOut';
my $old_fail = $tb->failure_output;
$tb->failure_output(\*CATCH);
diag(undef);
$tb->failure_output($old_fail);

is( $caught->read, "# undef\n" );
no_warnings;


$tb->maybe_regex(undef);
is( $caught->read, '' );
no_warnings;
