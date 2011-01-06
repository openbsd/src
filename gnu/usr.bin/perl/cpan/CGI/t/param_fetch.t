#!perl

# Tests for the param_fetch() method.

use Test::More 'no_plan';
use CGI;

{
    my $q = CGI->new('b=baz;a=foo;a=bar');

    is $q->param_fetch('a')->[0] => 'foo', 'first "a" is "foo"';
    is $q->param_fetch( -name => 'a' )->[0] => 'foo',
      'first "a" is "foo", with -name';
    is $q->param_fetch('a')->[1] => 'bar', 'second "a" is "bar"';
    is_deeply $q->param_fetch('a') => [qw/ foo bar /], 'a is array ref';
    is_deeply $q->param_fetch( -name => 'a' ) => [qw/ foo bar /],
      'a is array ref, w/ name';

    is $q->param_fetch('b')->[0] => 'baz', '"b" is "baz"';
    is_deeply $q->param_fetch('b') => [qw/ baz /], 'b is array ref too';

    is_deeply $q->param_fetch, [], "param_fetch without parameters";

    is_deeply $q->param_fetch( 'a', 'b' ), [qw/ foo bar /],
      "param_fetch only take first argument";
}
