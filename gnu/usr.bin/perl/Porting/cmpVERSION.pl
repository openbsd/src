#!/usr/bin/perl -w

#
# cmpVERSION - compare the current Perl source tree and a given tag
# for modules that have identical version numbers but different contents.
#
# with -d option, output the diffs too
# with -x option, exclude files from modules where blead is not upstream
#
# (after all, there are tools like core-cpan-diff that can already deal with
# them)
#
# Original by slaven@rezic.de, modified by jhi and matt.w.johnson@gmail.com.
# Adaptation to produce TAP by Abigail, folded back into this file by Nicholas

use strict;
use 5.006;

use ExtUtils::MakeMaker;
use File::Spec::Functions qw(devnull);
use Getopt::Long;

my ($diffs, $exclude_upstream, $tag_to_compare, $tap);
unless (GetOptions('diffs' => \$diffs,
		   'exclude|x' => \$exclude_upstream,
		   'tag=s' => \$tag_to_compare,
		   'tap' => \$tap,
		   ) && @ARGV == 0) {
    die "usage: $0 [ -d -x --tag TAG --tap]";
}

die "$0: This does not look like a Perl directory\n"
    unless -f "perl.h" && -d "Porting";
die "$0: 'This is a Perl directory but does not look like Git working directory\n"
    unless -d ".git";

my $null = devnull();

unless (defined $tag_to_compare) {
    # Thanks to David Golden for this suggestion.

    $tag_to_compare = `git describe --abbrev=0`;
    chomp $tag_to_compare;
}

my $tag_exists = `git --no-pager tag -l $tag_to_compare 2>$null`;
chomp $tag_exists;

unless ($tag_exists eq $tag_to_compare) {
    die "$0: '$tag_to_compare' is not a known Git tag\n" unless $tap;
    print "1..0 # SKIP: '$tag_to_compare' is not a known Git tag\n";
    exit 0;
}

my %upstream_files;
if ($exclude_upstream) {
    unshift @INC, 'Porting';
    require Maintainers;

    for my $m (grep {!defined $Maintainers::Modules{$_}{UPSTREAM}
			 or $Maintainers::Modules{$_}{UPSTREAM} ne 'blead'}
	       keys %Maintainers::Modules) {
	$upstream_files{$_} = 1 for Maintainers::get_module_files($m);
    }
}

# Files to skip from the check for one reason or another,
# usually because they pull in their version from some other file.
my %skip;
@skip{
    'lib/Carp/Heavy.pm',
    'lib/Config.pm',		# no version number but contents will vary
    'lib/Exporter/Heavy.pm',
    'win32/FindExt.pm',
} = ();

# Files to skip just for particular version(s),
# usually due to some # mix-up

my %skip_versions = (
	   # 'some/sample/file.pm' => [ '1.23', '1.24' ],
	   'dist/threads/lib/threads.pm' => [ '1.83' ],
	  );

my $skip_dirs = qr|^t/lib|;

sub pm_file_from_xs {
    my $xs = shift;

    # First try a .pm at the same level as the .xs file, with the same basename
    my $pm = $xs;
    $pm =~ s/xs\z/pm/;
    return $pm if -f $pm;

    # Try for a (different) .pm at the same level, based on the directory name:
    my ($path) = $xs =~ m!^(.*)/!;
    my ($last) = $path =~ m!([^-/]+)\z!;
    $pm = "$path/$last.pm";
    return $pm if -f $pm;

    # Try to work out the extension's full package, and look for a .pm in lib/
    # based on that:
    ($last) = $path =~ m!([^/]+)\z!;
    $last =~ tr !-!/!;
    $pm = "$path/lib/$last.pm";
    return $pm if -f $pm;

    die "No idea which .pm file corresponds to '$xs', so aborting";
}

# Key is the .pm file from which we check the version.
# Value is a reference to an array of files to check for differences
# The trivial case is a pure perl module, where the array holds one element,
# the perl module's file. The "fun" comes with XS modules, and the real fun
# with XS modules with more than one XS file, and "interesting" layouts.

my %module_diffs;

foreach (`git --no-pager diff --name-only $tag_to_compare --diff-filter=ACMRTUXB`) {
    chomp;
    next unless m/^(.*)\//;
    my $this_dir = $1;
    next if $this_dir =~ $skip_dirs || exists $skip{$_};
    next if exists $upstream_files{$_};
    if (/\.pm\z/ || m|^lib/.*\.pl\z|) {
	push @{$module_diffs{$_}}, $_;
    } elsif (/\.xs\z/ && !/\bt\b/) {
	push @{$module_diffs{pm_file_from_xs($_)}}, $_;
    }
}

unless (%module_diffs) {
    print "1..1\nok 1 - No difference found\n" if $tap;
    exit;
}

printf "1..%d\n" => scalar keys %module_diffs if $tap;

my $count;
my $diff_cmd = "git --no-pager diff $tag_to_compare ";
my (@diff);

foreach my $pm_file (sort keys %module_diffs) {
    # git has already told us that the files differ, so no need to grab each as
    # a blob from git, and do the comparison ourselves.
    my $pm_version = eval {MM->parse_version($pm_file)};
    my $orig_pm_content = get_file_from_git($pm_file, $tag_to_compare);
    my $orig_pm_version = eval {MM->parse_version(\$orig_pm_content)};
    
    if ((!defined $pm_version || !defined $orig_pm_version)
	|| ($pm_version eq 'undef' || $orig_pm_version eq 'undef') # sigh
	|| ($pm_version ne $orig_pm_version) # good
       ) {
        printf "ok %d - %s\n", ++$count, $pm_file if $tap;
    } else {
	if ($tap) {
	    foreach (sort @{$module_diffs{$pm_file}}) {
		print "# $_" for `$diff_cmd '$_'`;
	    }
	    if (exists $skip_versions{$pm_file}
		and grep $pm_version eq $_, @{$skip_versions{$pm_file}}) {
		printf "ok %d - SKIP $pm_file version $pm_version\n", ++$count;
	    } else {
		printf "not ok %d - %s\n", ++$count, $pm_file;
	    }
	} else {
	    push @diff, @{$module_diffs{$pm_file}};
	    print "$pm_file\n";
	}
    }
}

sub get_file_from_git {
    my ($file, $tag) = @_;
    local $/;
    return scalar `git --no-pager show $tag:$file 2>$null`;
}

if ($diffs) {
    for (sort @diff) {
	print "\n";
	system "$diff_cmd '$_'";
    }
}
