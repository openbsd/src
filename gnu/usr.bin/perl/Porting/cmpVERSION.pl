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
    unless (-d ".git" || (exists $ENV{GIT_DIR} && -d $ENV{GIT_DIR}));

my $null = devnull();

unless (defined $tag_to_compare) {
    my $check = 'HEAD';
    while(1) {
        $check = `git describe --abbrev=0 $check 2>$null`;
        chomp $check;
        last unless $check =~ /-RC/;
        $check .= '~1';
    }
    $tag_to_compare = $check;
    # Thanks to David Golden for this suggestion.

}

unless (length $tag_to_compare) {
    die "$0: Git found, but no Git tags found\n"
	unless $tap;
    print "1..0 # SKIP: Git found, but no Git tags found\n";
    exit 0;
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
    'cpan/ExtUtils-MakeMaker/t/lib/MakeMaker/Test/Setup/BFD.pm', # just a test module
    'cpan/ExtUtils-MakeMaker/t/lib/MakeMaker/Test/Setup/XS.pm',  # just a test module
    'cpan/Module-Build/t/lib/DistGen.pm', # just a test module
    'cpan/Module-Build/t/lib/MBTest.pm',  # just a test module
    'cpan/Module-Metadata/t/lib/DistGen.pm',    # just a test module
    'cpan/Module-Metadata/t/lib/MBTest.pm',     # just a test module
    'cpan/Module-Metadata/t/lib/Tie/CPHash.pm', # just a test module
    'dist/Attribute-Handlers/demo/MyClass.pm', # it's just demonstration code
    'dist/Exporter/lib/Exporter/Heavy.pm',
    'lib/Carp/Heavy.pm',
    'lib/Config.pm',		# no version number but contents will vary
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

    foreach my $try (sub {
			 # First try a .pm at the same level as the .xs file
			 # with the same basename
			 return shift =~ s/\.xs\z//r;
		     },
		     sub {
			 # Try for a (different) .pm at the same level, based
			 # on the directory name:
			 my ($path) = shift =~ m!^(.*)/!;
			 my ($last) = $path =~ m!([^-/]+)\z!;
			 return "$path/$last";
		     },
		     sub {
			 # Try to work out the extension's full package, and
			 # look for a .pm in lib/ based on that:
			 my ($path) = shift =~ m!^(.*)/!;
			 my ($last) = $path =~ m!([^/]+)\z!;
			 $last = 'List-Util' if $last eq 'Scalar-List-Utils';
			 $last =~ tr !-!/!;
			 return "$path/lib/$last";
		     }) {
	# For all cases, first look to see if the .pm file is generated.
	my $base = $try->($xs);
	return "${base}_pm.PL" if -f "${base}_pm.PL";
	return "${base}.pm" if -f "${base}.pm";
    }

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
    if (/\.pm\z/ || m|^lib/.*\.pl\z| || /_pm\.PL\z/) {
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
my $q = ($^O eq 'MSWin32' || $^O eq 'NetWare' || $^O eq 'VMS') ? '"' : "'";
my (@diff);

foreach my $pm_file (sort keys %module_diffs) {
    # git has already told us that the files differ, so no need to grab each as
    # a blob from git, and do the comparison ourselves.
    my $pm_version = eval {MM->parse_version($pm_file)};
    my $orig_pm_content = get_file_from_git($pm_file, $tag_to_compare);
    my $orig_pm_version = eval {MM->parse_version(\$orig_pm_content)};
    ++$count;

    if (!defined $orig_pm_version || $orig_pm_version eq 'undef') { # sigh
        print "ok $count - SKIP Can't parse \$VERSION in $pm_file\n"
          if $tap;
    } elsif (!defined $pm_version || $pm_version eq 'undef') {
        print "not ok $count - in $pm_file version was $orig_pm_version, now unparsable\n" if $tap;
    } elsif ($pm_version ne $orig_pm_version) { # good
        print "ok $count - $pm_file\n" if $tap;
    } else {
	if ($tap) {
	    foreach (sort @{$module_diffs{$pm_file}}) {
		print "# $_" for `$diff_cmd $q$_$q`;
	    }
	    if (exists $skip_versions{$pm_file}
		and grep $pm_version eq $_, @{$skip_versions{$pm_file}}) {
		print "ok $count - SKIP $pm_file version $pm_version\n";
	    } else {
		print "not ok $count - $pm_file version $pm_version\n";
	    }
	} else {
	    push @diff, @{$module_diffs{$pm_file}};
	    print "$pm_file version $pm_version\n";
	}
    }
}

sub get_file_from_git {
    my ($file, $tag) = @_;
    local $/;

    use open IN => ':raw';
    return scalar `git --no-pager show $tag:$file 2>$null`;
}

if ($diffs) {
    for (sort @diff) {
	print "\n";
	system "$diff_cmd $q$_$q";
    }
}
