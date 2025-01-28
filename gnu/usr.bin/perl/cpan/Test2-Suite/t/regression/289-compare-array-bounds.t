use Test2::V0;

my $foo = [qw(a b c d e f g )];

is($foo, array {
    # Uses the next index, in this case index 0;
    item 'a';

    # Gets index 1 automatically
    item 'b';

    # Specify the index
    item 2 => 'c';

    # We skipped index 3, which means we don't care what it is.
    item 4 => 'e';

    # Gets index 5.
    item 'f';

    # Set checks that apply to all items. Can be done multiple times, and
    # each call can define multiple checks, all will be run.
    all_items match qr/[a-z]/;
    #all_items match qr/x/;

    # Of the remaining items (after the filter is applied) the next one
    # (which is now index 6) should be 'g'.
    item 6 => 'g';

    item 7 => DNE; # Ensure index 7 does not exist.

    end(); # Ensure no other indexes exist.
});

done_testing;
