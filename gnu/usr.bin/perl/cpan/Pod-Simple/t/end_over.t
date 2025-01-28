# head ends over
use strict;
use warnings;
use Test::More tests => 6;

BEGIN {
  require FindBin;
  unshift @INC, $FindBin::Bin . '/lib';
}
use helpers qw(f);

my $d;
#use Pod::Simple::Debug (\$d,0);

use Pod::Simple::DumpAsXML;
use Pod::Simple::XMLOutStream;
print "# Pod::Simple version $Pod::Simple::VERSION\n";

sub nowhine {
  $_[0]->{'no_whining'} = 1;
}

&is(f(
\&nowhine,
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=head1 SVUP\n\nMyup.",
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=back\n\n=head1 SVUP\n\nMyup.",
));

&is(f(
\&nowhine,
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=head2 SVUP\n\nMyup.",
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=back\n\n=head2 SVUP\n\nMyup.",
));

&is(f(
\&nowhine,
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=head3 SVUP\n\nMyup.",
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=back\n\n=head3 SVUP\n\nMyup.",
));

&is(f(
\&nowhine,
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=head4 SVUP\n\nMyup.",
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=back\n\n=head4 SVUP\n\nMyup.",
));

&is(f(
\&nowhine,
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=head5 SVUP\n\nMyup.",
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=back\n\n=head5 SVUP\n\nMyup.",
));

&is(f(
\&nowhine,
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=head6 SVUP\n\nMyup.",
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=back\n\n=head6 SVUP\n\nMyup.",
));
