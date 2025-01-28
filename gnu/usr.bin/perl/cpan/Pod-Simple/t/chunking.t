use strict;
use warnings;
use Test::More tests => 9;

#use Pod::Simple::Debug (2);

BEGIN {
  require FindBin;
  unshift @INC, $FindBin::Bin . '/lib';
}
use helpers;

use Pod::Simple::DumpAsXML;
use Pod::Simple::XMLOutStream;
print "# Pod::Simple version $Pod::Simple::VERSION\n";

is( Pod::Simple::XMLOutStream->_out("=head1 =head1"),
    '<Document><head1>=head1</head1></Document>'
);

is( Pod::Simple::XMLOutStream->_out("\n=head1 =head1"),
    '<Document><head1>=head1</head1></Document>'
);

is( Pod::Simple::XMLOutStream->_out("\n=head1 =head1\n"),
    '<Document><head1>=head1</head1></Document>'
);

is( Pod::Simple::XMLOutStream->_out("\n=head1 =head1\n\n"),
    '<Document><head1>=head1</head1></Document>'
);

&is(e "\n=head1 =head1\n\n" , "\n=head1 =head1\n\n");

&is(e "\n=head1\n=head1\n\n", "\n=head1 =head1\n\n");

&is(e "\n=pod\n\nCha cha cha\n\n" , "\n=pod\n\nCha cha cha\n\n");
&is(e "\n=pod\n\nCha\tcha  cha\n\n" , "\n=pod\n\nCha cha cha\n\n");
&is(e "\n=pod\n\nCha\ncha  cha\n\n" , "\n=pod\n\nCha cha cha\n\n");
