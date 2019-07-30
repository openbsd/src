#!./perl -w
# Test that there are no missing authors in AUTHORS

BEGIN {
    @INC = '..' if -f '../TestInit.pm';
}
use TestInit qw(T); # T is chdir to the top level
use strict;

require 't/test.pl';
find_git_or_skip('all');

# This is the subset of "pretty=fuller" that checkAUTHORS.pl actually needs:
my $quote = $^O =~ /^mswin/i ? q(") : q(');
system("git log --pretty=format:${quote}Author: %an <%ae>%n${quote} | $^X Porting/checkAUTHORS.pl --tap -");

# EOF
