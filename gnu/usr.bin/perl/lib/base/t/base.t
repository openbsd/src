#!/usr/bin/perl -w

use strict;
use Test::More tests => 10;

use_ok('base');


package No::Version;

use vars qw($Foo);
sub VERSION { 42 }

package Test::Version;

use base qw(No::Version);
::ok( $No::Version::VERSION =~ /set by base\.pm/,          '$VERSION bug' );

# Test Inverse of $VERSION bug base.pm should not clobber existing $VERSION
package Has::Version;

BEGIN { $Has::Version::VERSION = '42' };

package Test::Version2;

use base qw(Has::Version);
::is( $Has::Version::VERSION, 42 );

package main;

my $eval1 = q{
  {
    package Eval1;
    {
      package Eval2;
      use base 'Eval1';
      $Eval2::VERSION = "1.02";
    }
    $Eval1::VERSION = "1.01";
  }
};

eval $eval1;
is( $@, '' );

is( $Eval1::VERSION, 1.01 );

is( $Eval2::VERSION, 1.02 );


eval q{use base 'reallyReAlLyNotexists'};
like( $@, qr/^Base class package "reallyReAlLyNotexists" is empty./,
                                          'base with empty package');

eval q{use base 'reallyReAlLyNotexists'};
like( $@, qr/^Base class package "reallyReAlLyNotexists" is empty./,
                                          '  still empty on 2nd load');

BEGIN { $Has::Version_0::VERSION = 0 }

package Test::Version3;

use base qw(Has::Version_0);
::is( $Has::Version_0::VERSION, 0, '$VERSION==0 preserved' );


package Test::SIGDIE;

{ 
    local $SIG{__DIE__} = sub { 
        ::fail('sigdie not caught, this test should not run') 
    };
    eval {
      'base'->import(qw(Huh::Boo));
    };

    ::like($@, qr/^Base class package "Huh::Boo" is empty/, 
         'Base class empty error message');

}
