use Test::More 'no_plan';
use CGI;
my $cgi = CGI->new('<a>=<b>');
like($cgi->Dump, qr/\Q&lt;a&gt;/, 'param names are HTML escaped by Dump()');
like($cgi->Dump, qr/\Q&lt;b&gt;/, 'param values are HTML escaped by Dump()');
