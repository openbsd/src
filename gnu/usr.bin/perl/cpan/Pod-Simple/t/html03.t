# Testing HTML titles
use strict;
use warnings;
use Test::More tests => 5;

#use Pod::Simple::Debug (10);

use Pod::Simple::HTML;

sub x { Pod::Simple::HTML->_out(
  #sub{  $_[0]->bare_output(1)  },
  "=pod\n\n$_[0]",
) }

# make sure empty file => empty output

is( x(''),'', "Contentlessness" );
like( x(qq{=pod\n\nThis is a paragraph}), qr{<title></title>}i );
like( x(qq{This is a paragraph}), qr{<title></title>}i );
like( x(qq{=head1 Prok\n\nThis is a paragraph}), qr{<title>Prok</title>}i );
like( x(qq{=head1 NAME\n\nProk -- stuff\n\nThis}), qr{<title>Prok</title>} );
