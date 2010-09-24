#!/usr/bin/perl -w

# In order for MakeMaker to build in the core, nothing can use
# Fcntl which includes POSIX.  devise_date()'s use of strftime()
# was replaced.  This tests that it's identical.

use strict;

use Test::More tests => 1;

use Pod::Man;
use POSIX qw(strftime);

my $parser = Pod::Man->new;
is $parser->devise_date, strftime("%Y-%m-%d", localtime);
