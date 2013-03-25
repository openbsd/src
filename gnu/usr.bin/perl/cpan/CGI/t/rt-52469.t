use strict;
use warnings;

use Test::More tests => 1;                      # last test to print

use CGI;

$ENV{REQUEST_METHOD} = 'PUT';

my $cgi = CGI->new;

pass 'new() returned';


