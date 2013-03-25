#!./perl -w

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require './test.pl';
}

use strict;
no  warnings 'syntax';

{
    # Bug #77084 points out a corruption problem when scalar //g is used
    # on overloaded objects.

    my @realloc;
    my $TAG = "foo:bar";
    use overload '""' => sub {$TAG};

    my $o = bless [];
    my ($one) = $o =~ /(.*)/g;
    push @realloc, "xxxxxx"; # encourage realloc of SV and PVX
    is $one, $TAG, "list context //g against overloaded object";


    my $r = $o =~ /(.*)/g;
    push @realloc, "yyyyyy"; # encourage realloc of SV and PVX
    is $1, $TAG, "scalar context //g against overloaded object";
    pos ($o) = 0;  # Reset pos, as //g in scalar context sets it to non-0.

    $o =~ /(.*)/g;
    push @realloc, "zzzzzz"; # encourage realloc of SV and PVX
    is $1, $TAG, "void context //g against overloaded object";
}

done_testing();
