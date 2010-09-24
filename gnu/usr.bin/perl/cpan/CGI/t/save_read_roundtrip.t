
use strict;
use warnings;

# Reference: RT#13158: Needs test: empty name/value, when saved, prevents proper restore from filehandle.
#                      https://rt.cpan.org/Ticket/Display.html?id=13158

use Test::More tests => 3;

use IO::File;
use CGI;

my $cgi = CGI->new('a=1;=;b=2;=3');
ok eq_set (['a', '', 'b'], [$cgi->param]);

# not File::Temp, since that wasn't in core at 5.6.0
my $tmp = IO::File->new_tmpfile;
$cgi->save($tmp);
$tmp->seek(0,0);

$cgi = CGI->new($tmp);
ok eq_set (['a', '', 'b'], [$cgi->param]);
is $cgi->param(''), 3; # '=' is lost, '=3' is retained

