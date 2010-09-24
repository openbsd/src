#!perl

# Tests for the query_string() method.

use Test::More 'no_plan';
use CGI;

{
    my $q1 = CGI->new('b=2;a=1;a=1');
    my $q2 = CGI->new('b=2&a=1&a=1');

    is($q1->query_string
        ,$q2->query_string
        , "query string format is returned with the same delimiter regardless of input.");
}
