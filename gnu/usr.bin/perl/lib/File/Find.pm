package File::Find;
require 5.000;
require Exporter;
use Config;
require Cwd;
require File::Basename;


=head1 NAME

find - traverse a file tree

finddepth - traverse a directory structure depth-first

=head1 SYNOPSIS

    use File::Find;
    find(\&wanted, '/foo','/bar');
    sub wanted { ... }
    
    use File::Find;
    finddepth(\&wanted, '/foo','/bar');
    sub wanted { ... }

=head1 DESCRIPTION

The wanted() function does whatever verifications you want.
$File::Find::dir contains the current directory name, and $_ the
current filename within that directory.  $File::Find::name contains
C<"$File::Find::dir/$_">.  You are chdir()'d to $File::Find::dir when
the function is called.  The function may set $File::Find::prune to
prune the tree.

This library is primarily for the C<find2perl> tool, which when fed, 

    find2perl / -name .nfs\* -mtime +7 \
	-exec rm -f {} \; -o -fstype nfs -prune

produces something like:

    sub wanted {
        /^\.nfs.*$/ &&
        (($dev,$ino,$mode,$nlink,$uid,$gid) = lstat($_)) &&
        int(-M _) > 7 &&
        unlink($_)
        ||
        ($nlink || (($dev,$ino,$mode,$nlink,$uid,$gid) = lstat($_))) &&
        $dev < 0 &&
        ($File::Find::prune = 1);
    }

Set the variable $File::Find::dont_use_nlink if you're using AFS,
since AFS cheats.

C<finddepth> is just like C<find>, except that it does a depth-first
search.

Here's another interesting wanted function.  It will find all symlinks
that don't resolve:

    sub wanted {
	-l && !-e && print "bogus link: $File::Find::name\n";
    } 

=cut

@ISA = qw(Exporter);
@EXPORT = qw(find finddepth);


sub find {
    my $wanted = shift;
    my $cwd = Cwd::fastcwd();
    my ($topdir,$topdev,$topino,$topmode,$topnlink);
    foreach $topdir (@_) {
	(($topdev,$topino,$topmode,$topnlink) = stat($topdir))
	  || (warn("Can't stat $topdir: $!\n"), next);
	if (-d _) {
	    if (chdir($topdir)) {
		($dir,$_) = ($topdir,'.');
		$name = $topdir;
		&$wanted;
		my $fixtopdir = $topdir;
	        $fixtopdir =~ s,/$,, ;
		$fixtopdir =~ s/\.dir$// if $Is_VMS; ;
		&finddir($wanted,$fixtopdir,$topnlink);
	    }
	    else {
		warn "Can't cd to $topdir: $!\n";
	    }
	}
	else {
	    unless (($dir,$_) = File::Basename::fileparse($topdir)) {
		($dir,$_) = ('.', $topdir);
	    }
	    $name = $topdir;
	    chdir $dir && &$wanted;
	}
	chdir $cwd;
    }
}

sub finddir {
    my($wanted, $nlink);
    local($dir, $name);
    ($wanted, $dir, $nlink) = @_;

    my($dev, $ino, $mode, $subcount);

    # Get the list of files in the current directory.
    opendir(DIR,'.') || (warn "Can't open $dir: $!\n", return);
    my(@filenames) = readdir(DIR);
    closedir(DIR);

    if ($nlink == 2 && !$dont_use_nlink) {  # This dir has no subdirectories.
	for (@filenames) {
	    next if $_ eq '.';
	    next if $_ eq '..';
	    $name = "$dir/$_";
	    $nlink = 0;
	    &$wanted;
	}
    }
    else {                    # This dir has subdirectories.
	$subcount = $nlink - 2;
	for (@filenames) {
	    next if $_ eq '.';
	    next if $_ eq '..';
	    $nlink = $prune = 0;
	    $name = "$dir/$_";
	    &$wanted;
	    if ($subcount > 0 || $dont_use_nlink) {    # Seen all the subdirs?

		# Get link count and check for directoriness.

		($dev,$ino,$mode,$nlink) = ($Is_VMS ? stat($_) : lstat($_));
		    # unless ($nlink || $dont_use_nlink);
		
		if (-d _) {

		    # It really is a directory, so do it recursively.

		    if (!$prune && chdir $_) {
			$name =~ s/\.dir$// if $Is_VMS;
			&finddir($wanted,$name,$nlink);
			chdir '..';
		    }
		    --$subcount;
		}
	    }
	}
    }
}


sub finddepth {
    my $wanted = shift;

    $cwd = Cwd::fastcwd();;

    my($topdir, $topdev, $topino, $topmode, $topnlink);
    foreach $topdir (@_) {
	(($topdev,$topino,$topmode,$topnlink) = stat($topdir))
	  || (warn("Can't stat $topdir: $!\n"), next);
	if (-d _) {
	    if (chdir($topdir)) {
		my $fixtopdir = $topdir;
		$fixtopdir =~ s,/$,, ;
		$fixtopdir =~ s/\.dir$// if $Is_VMS;
		&finddepthdir($wanted,$fixtopdir,$topnlink);
		($dir,$_) = ($fixtopdir,'.');
		$name = $fixtopdir;
		&$wanted;
	    }
	    else {
		warn "Can't cd to $topdir: $!\n";
	    }
	}
	else {
	    unless (($dir,$_) = File::Basename::fileparse($topdir)) {
		($dir,$_) = ('.', $topdir);
	    }
	    chdir $dir && &$wanted;
	}
	chdir $cwd;
    }
}

sub finddepthdir {
    my($wanted, $nlink);
    local($dir, $name);
    ($wanted,$dir,$nlink) = @_;
    my($dev, $ino, $mode, $subcount);

    # Get the list of files in the current directory.
    opendir(DIR,'.') || warn "Can't open $dir: $!\n";
    my(@filenames) = readdir(DIR);
    closedir(DIR);

    if ($nlink == 2 && !$dont_use_nlink) {   # This dir has no subdirectories.
	for (@filenames) {
	    next if $_ eq '.';
	    next if $_ eq '..';
	    $name = "$dir/$_";
	    $nlink = 0;
	    &$wanted;
	}
    }
    else {                    # This dir has subdirectories.
	$subcount = $nlink - 2;
	for (@filenames) {
	    next if $_ eq '.';
	    next if $_ eq '..';
	    $nlink = 0;
	    $name = "$dir/$_";
	    if ($subcount > 0 || $dont_use_nlink) {    # Seen all the subdirs?

		# Get link count and check for directoriness.

		($dev,$ino,$mode,$nlink) = ($Is_VMS ? stat($_) : lstat($_));
		
		if (-d _) {

		    # It really is a directory, so do it recursively.

		    if (chdir $_) {
			$name =~ s/\.dir$// if $Is_VMS;
			&finddepthdir($wanted,$name,$nlink);
			chdir '..';
		    }
		    --$subcount;
		}
	    }
	    &$wanted;
	}
    }
}

# Set dont_use_nlink in your hint file if your system's stat doesn't
# report the number of links in a directory as an indication
# of the number of files.
# See, e.g. hints/machten.sh for MachTen 2.2.
$dont_use_nlink = 1 if ($Config::Config{'dont_use_nlink'});

# These are hard-coded for now, but may move to hint files.
if ($^O eq 'VMS') {
  $Is_VMS = 1;
  $dont_use_nlink = 1;
}

$dont_use_nlink = 1 if $^O eq 'os2';
$dont_use_nlink = 1 if $^O =~ m:^mswin32$:i ;

1;

