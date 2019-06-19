#!./perl -w
# Test that there are no missing authors in AUTHORS

BEGIN {
    chdir 't' if -d 't';
    require "./test.pl";
    set_up_inc('../lib', '..');
}

use TestInit qw(T);    # T is chdir to the top level
use strict;

find_git_or_skip('all');
skip_all(
    "This distro may have modified some files in cpan/. Skipping validation.")
  if $ENV{'PERL_BUILD_PACKAGING'};

my $revision_range = ''; # could use 'v5.22.0..' as default, no reason to recheck all previous commits...
if ( $ENV{TRAVIS} && defined $ENV{TRAVIS_COMMIT_RANGE} ) {
	# travisci is adding a merge commit when smoking a pull request
	#	unfortunately it's going to use the default GitHub email from the author
	#	which can differ from the one the author wants to use as part of the pull request
	#	let's simply use the TRAVIS_COMMIT_RANGE which list the commits we want to check
	#	all the more a pull request should not be impacted by blead being incorrect
	$revision_range = $ENV{TRAVIS_COMMIT_RANGE};
}

# This is the subset of "pretty=fuller" that checkAUTHORS.pl actually needs:
print qx{git log --pretty=format:"Author: %an <%ae>" $revision_range | $^X Porting/checkAUTHORS.pl --tap -};

# EOF
