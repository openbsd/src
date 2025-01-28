use strict;
use warnings;
use Test::More tests => 1;

BEGIN {
  require FindBin;
  unshift @INC, $FindBin::Bin . '/lib';
}
use helpers qw(f);

my $d;
#use Pod::Simple::Debug (\$d,0);
#use Pod::Simple::Debug (10);

use Pod::Simple::DumpAsXML;
use Pod::Simple::XMLOutStream;
print "# Pod::Simple version $Pod::Simple::VERSION\n";

sub nowhine {
#  $_[0]->{'no_whining'} = 1;
  $_[0]->accept_targets("*");
}

local $Pod::Simple::XMLOutStream::SORT_ATTRS = 1;
&is(f(
    \&nowhine,
"=begin :foo\n\n=begin :bar\n\nZaz\n\n",
"=begin :foo\n\n=begin :bar\n\nZaz\n\n=end :bar\n\n=end :foo\n\n",
));
