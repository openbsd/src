use strict;
use warnings;

my $is_ebcdic = ord("A") == 193;
my $os = lc $^O;

binmode STDOUT, ':utf8';
binmode STDERR, ':utf8';

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    unshift @INC, '.';
    require './test.pl';    # for fresh_perl_is() etc
}

fresh_perl_like("use source::encoding '';",
                qr/Bad argument for source::encoding: ''/,
                { }, "Fails on empty encoding name");

fresh_perl_like("use source::encoding 'asci';",
                qr/Bad argument for source::encoding: 'asci'/,
                { }, "Fails on unknown encoding name");

fresh_perl_like(<<~'EOT',
                 use source::encoding 'ascii';
                 my $var = "¶";
                 EOT
                qr/Use of non-ASCII character 0x[[:xdigit:]]{2} illegal/,
                { }, "Fails on non-ASCII input when ASCII is required");

if (fresh_perl_like(<<~'EOT',
                 use v5.41.0;
                 my $var = "¶";
                 EOT
                qr/Use of non-ASCII character 0x[[:xdigit:]]{2} illegal/,
                { }, ">= 'use 5.39' implies use source::encoding 'ascii'")
   ) {
    fresh_perl_is(<<~'EOT',
                    use v5.41.0;
                    my $var = "A";
                    no source::encoding;
                    $var = "¶";
                    EOT
                "",
                { }, "source encoding can be turned off");
}
else { # Above test depends on the previous one; if that failed, use this
       # alternate one
    fresh_perl_is(<<~'EOT',
                    use source::encoding 'ascii';
                    my $var = "A";
                    no source::encoding;
                    $var = "¶";
                    EOT
                "",
                { }, "source encoding can be turned off");
}

fresh_perl_is(<<~'EOT',
               my $var = "A";
               {
                  use source::encoding 'ascii';
                  $var = "B";
               }
               $var = "¶";
               EOT
              "",
              { }, "source encoding is lexically scoped");

# The test suite is full of tests with ASCII-only code, and with UTF-8 code.
# No need to repeat any of them here.  But we do need to verify that this
# pragma acts like 'use utf8'.
fresh_perl_is(<<~'EOT',
                 use source::encoding 'utf8';
                 my $var = "¶";
                 EOT
                "",
                { }, "Succeeds on non-ASCII input when UTF-8 is allowed");

fresh_perl_is(<<~'EOT',
                 use source::encoding 'utf8';
                 my $var = "¶";
                 no utf8;
                 $var = "¶";
                 EOT
                "",
                { }, "'no utf8' turns off source encoding");

done_testing();
