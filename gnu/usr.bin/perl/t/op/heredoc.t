# tests for heredocs besides what is tested in base/lex.t

BEGIN {
   chdir 't' if -d 't';
   @INC = '../lib';
   require './test.pl';
}

use strict;
plan(tests => 39);


# heredoc without newline (#65838)
{
    my $string = <<'HEREDOC';
testing for 65838
HEREDOC

    my $code = "<<'HEREDOC';\n${string}HEREDOC";  # HD w/o newline, in eval-string
    my $hd = eval $code or warn "$@ ---";
    is($hd, $string, "no terminating newline in string-eval");
}


# here-doc edge cases
{
    my $string = "testing for 65838";

    fresh_perl_is(
        "print <<'HEREDOC';\n${string}\nHEREDOC",
        $string,
        {},
        "heredoc at EOF without trailing newline"
    );

    fresh_perl_is(
        "print <<;\n$string\n",
        $string,
        { switches => ['-X'] },
        "blank-terminated heredoc at EOF"
    );
    fresh_perl_is(
        "print <<\n$string\n",
        $string,
        { switches => ['-X'] },
        "blank-terminated heredoc at EOF and no semicolon"
    );
    fresh_perl_is(
        "print <<foo\r\nick and queasy\r\nfoo\r\n",
        'ick and queasy',
        { switches => ['-X'] },
        "crlf-terminated heredoc"
    );
    fresh_perl_is(
        "print qq|\${\\<<foo}|\nick and queasy\nfoo\n",
        'ick and queasy',
        { switches => ['-w'], stderr => 1 },
        'no warning for qq|${\<<foo}| in file'
    );
}


# here-doc parse failures
{
    fresh_perl_like(
        "print <<HEREDOC;\nwibble\n HEREDOC",
        qr/find string terminator/,
        {},
        "string terminator must start at newline"
    );

    # Loop over various lengths to try to force at least one to cause a
    # reallocation in S_scan_heredoc()
    # Timing on a modern machine suggests that this loop executes in less than
    # 0.1s, so it's a very small cost for the default build. The benefit is
    # that building with ASAN will reveal the bug and any related regressions.
    for (1..31) {
        fresh_perl_like(
            "print <<;\n" . "x" x $_,
            qr/find string terminator/,
            { switches => ['-X'] },
            "empty string terminator still needs a newline (length $_)"
        );
    }

    fresh_perl_like(
        "print <<ThisTerminatorIsLongerThanTheData;\nno more newlines",
        qr/find string terminator/,
        {},
        "long terminator fails correctly"
    );
}
