#!/usr/bin/perl -w

BEGIN {
   if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = qw(../lib);
    }
}

use strict;
use Test::More tests => 11;

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
like( $@, qr/^Base class package "reallyReAlLyNotexists" is empty\./,
                                          'base with empty package');

eval q{use base 'reallyReAlLyNotexists'};
like( $@, qr/^Base class package "reallyReAlLyNotexists" is empty\./,
                                          '  still empty on 2nd load');
{
    my $warning;
    local $SIG{__WARN__} = sub { $warning = shift };
    eval q{package HomoGenous; use base 'HomoGenous';};
    like($warning, qr/^Class 'HomoGenous' tried to inherit from itself/,
                                          '  self-inheriting');
}

{
    BEGIN { $Has::Version_0::VERSION = 0 }

    package Test::Version3;

    use base qw(Has::Version_0);
    ::is( $Has::Version_0::VERSION, 0, '$VERSION==0 preserved' );
}


{
    package Schlozhauer;
    use constant FIELDS => 6;

    package Basilisco;
    eval q{ use base 'Schlozhauer' };
    ::is( $@, '', 'Can coexist with a FIELDS constant' );
}
