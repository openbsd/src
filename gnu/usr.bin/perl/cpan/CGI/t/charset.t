#!perl

use strict;
use warnings;

use Test::More 'no_plan';

use CGI;

my $q = CGI->new;

like( $q->header
    , qr/charset=ISO-8859-1/, "charset ISO-8859-1 is set by default for default content-type");
like( $q->header('application/json')
    , qr/charset=ISO-8859-1/, "charset ISO-8859-1 is set by default for application/json content-type");

{
    $q->charset('UTF-8');
    my $out = $q->header('text/plain');
    like($out, qr{Content-Type: text/plain; charset=UTF-8}, "setting charset alters header of text/plain");
}
{
    $q->charset('UTF-8');
    my $out = $q->header('application/json');
    like($out, qr{Content-Type: application/json; charset=UTF-8}, "setting charset alters header of application/json");
}

