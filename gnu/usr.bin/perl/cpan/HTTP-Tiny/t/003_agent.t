#!perl

use strict;
use warnings;

use Test::More tests => 3;
use HTTP::Tiny;

# a couple tests to ensure that we get the default agent expected, the coorect
# agent when specified, and the correct agent when specifified with a space at
# the end of the string (as LWP::UserAgent does)


my $default = 'HTTP-Tiny/' . (HTTP::Tiny->VERSION || 0);

{
    my $ua = HTTP::Tiny->new();
    is $ua->agent, $default, 'default agent string is as expected';
}

{
    my $ua = HTTP::Tiny->new(agent => 'something else');
    is $ua->agent, 'something else', 'agent string is as expected';
}

{
    my $ua = HTTP::Tiny->new(agent => 'something else ');
    is
        $ua->agent,
        "something else $default",
        'agent string is as properly appended to',
        ;
}
