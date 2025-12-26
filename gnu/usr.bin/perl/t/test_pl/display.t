BEGIN {
    chdir 't' if -d 't';
    require './test.pl';
}

# This test used to die. See GH #22537.
my $t = eval { display("ABC \x{20AC}") };
is $@, '', "display() doesn't die on read-only strings";
is $t, 'ABC \\x{20ac}', 'display() escapes Unicode characters correctly';

done_testing();
