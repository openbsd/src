#!./perl

BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
    set_up_inc('../lib');
    skip_all_without_perlio();
    skip_all_without_dynamic_extension('Fcntl'); # how did you get this far?
}

use strict;
use warnings;

plan tests => 11;

use Fcntl qw(:seek);

{
    ok((open my $fh, "+>", undef), "open my \$fh, '+>', undef");
    print $fh "the right write stuff";
    ok(seek($fh, 0, SEEK_SET), "seek to zero");
    my $data = <$fh>;
    is($data, "the right write stuff", "found the right stuff");
}

{
    ok((open my $fh, "+<", undef), "open my \$fh, '+<', undef");
    print $fh "the right read stuff";
    ok(seek($fh, 0, SEEK_SET), "seek to zero");
    my $data = <$fh>;
    is($data, "the right read stuff", "found the right stuff");
}

SKIP:
{
    ok((open my $fh, "+>>", undef), "open my \$fh, '+>>', undef")
      or skip "can't open temp for append: $!", 3;
    print $fh "abc";
    ok(seek($fh, 0, SEEK_SET), "seek to zero");
    print $fh "xyz";
    ok(seek($fh, 0, SEEK_SET), "seek to zero again");
    my $data = <$fh>;
    is($data, "abcxyz", "check the second write appended");
}

{
    # minimal-reproduction moral equivalent of the autodie wrapper for open()
    # because APIs that wrap open() should be able to expose this behaviour
    sub wrapped_open (*;$@) { open $_[0], $_[1], $_[2] }
    ok((wrapped_open my $fh, "+>", undef), "wrapped_open my \$fh, '+>', undef");
}
