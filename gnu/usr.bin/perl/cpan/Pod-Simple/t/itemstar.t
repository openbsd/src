use strict;
use warnings;
use Test::More tests => 4;

BEGIN {
  require FindBin;
  unshift @INC, $FindBin::Bin . '/lib';
  require helpers;
  helpers->import;
}
#my $d;
#use Pod::Simple::Debug (3);

use Pod::Simple::DumpAsXML;
use Pod::Simple::XMLOutStream;
print "# Pod::Simple version $Pod::Simple::VERSION\n";

my $x = 'Pod::Simple::XMLOutStream';

print "##### Tests for '=item * Foo' tolerance via class $x\n";

$Pod::Simple::XMLOutStream::ATTR_PAD   = ' ';
$Pod::Simple::XMLOutStream::SORT_ATTRS = 1; # for predictably testable output


print "#\n# Tests for simple =item *'s\n";
ok( $x->_out("\n=over\n\n=item * Stuff\n\n=item * Bar I<baz>!\n\n=back\n\n"),
    '<Document><over-bullet indent="4"><item-bullet>Stuff</item-bullet><item-bullet>Bar <I>baz</I>!</item-bullet></over-bullet></Document>'
);
ok( $x->_out("\n=over\n\n=item * Stuff\n\n=cut\n\nStuff\n\n=item *\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-bullet indent="4"><item-bullet>Stuff</item-bullet><item-bullet>Bar <I>baz</I>!</item-bullet></over-bullet></Document>'
);
ok( $x->_out("\n=over 10\n\n=item * Stuff\n\n=cut\n\nStuff\n\n=item *\n\nBar I<baz>!\n\n=back\n\n"),
    '<Document><over-bullet indent="10"><item-bullet>Stuff</item-bullet><item-bullet>Bar <I>baz</I>!</item-bullet></over-bullet></Document>'
);
ok( $x->_out("\n=over\n\n=item * Stuff I<things\num> hoo!\n=cut\nStuff\n\n=item *\n\nBar I<baz>!\n\n=back"),
    '<Document><over-bullet indent="4"><item-bullet>Stuff <I>things um</I> hoo!</item-bullet><item-bullet>Bar <I>baz</I>!</item-bullet></over-bullet></Document>'
);
