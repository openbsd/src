BEGIN {
    if($ENV{PERL_CORE}) {
        chdir 't';
        @INC = '../lib';
    }
}

use strict;
use Test;
BEGIN { plan tests => 5 };

my $d;
#use Pod::Simple::Debug (\$d,0);

ok 1;

use Pod::Simple::DumpAsXML;
use Pod::Simple::XMLOutStream;
print "# Pod::Simple version $Pod::Simple::VERSION\n";
sub e     ($$) { Pod::Simple::XMLOutStream::->_duo(\&nowhine, @_) }

sub nowhine {
  $_[0]->{'no_whining'} = 1;
}

&ok(e(
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=head1 SVUP\n\nMyup.",
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=back\n\n=head1 SVUP\n\nMyup.",
));

&ok(e(
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=head2 SVUP\n\nMyup.",
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=back\n\n=head2 SVUP\n\nMyup.",
));

&ok(e(
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=head3 SVUP\n\nMyup.",
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=back\n\n=head3 SVUP\n\nMyup.",
));

&ok(e(
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=head4 SVUP\n\nMyup.",
"=head2 BLOOP\n\nHoopbehwo!\n\n=over\n\n=item Stuff.  Um.\n\nBrop.\n\n=back\n\n=head4 SVUP\n\nMyup.",
));


__END__


