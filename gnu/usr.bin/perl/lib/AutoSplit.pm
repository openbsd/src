package AutoSplit;

require 5.000;
require Exporter;

use Config;
use Carp;
use File::Path qw(mkpath);

@ISA = qw(Exporter);
@EXPORT = qw(&autosplit &autosplit_lib_modules);
@EXPORT_OK = qw($Verbose $Keep $Maxlen $CheckForAutoloader $CheckModTime);

=head1 NAME

AutoSplit - split a package for autoloading

=head1 SYNOPSIS

 perl -e 'use AutoSplit; autosplit_lib_modules(@ARGV)' ...

 use AutoSplit; autosplit($file, $dir, $keep, $check, $modtime);

for perl versions 5.002 and later:

 perl -MAutoSplit -e 'autosplit($ARGV[0], $ARGV[1], $k, $chk, $modtime)' ...

=head1 DESCRIPTION

This function will split up your program into files that the AutoLoader
module can handle. It is used by both the standard perl libraries and by
the MakeMaker utility, to automatically configure libraries for autoloading.

The C<autosplit> interface splits the specified file into a hierarchy 
rooted at the directory C<$dir>. It creates directories as needed to reflect
class hierarchy, and creates the file F<autosplit.ix>. This file acts as
both forward declaration of all package routines, and as timestamp for the
last update of the hierarchy.

The remaining three arguments to C<autosplit> govern other options to the
autosplitter. If the third argument, I<$keep>, is false, then any pre-existing
C<*.al> files in the autoload directory are removed if they are no longer
part of the module (obsoleted functions). The fourth argument, I<$check>,
instructs C<autosplit> to check the module currently being split to ensure
that it does include a C<use> specification for the AutoLoader module, and
skips the module if AutoLoader is not detected. Lastly, the I<$modtime>
argument specifies that C<autosplit> is to check the modification time of the
module against that of the C<autosplit.ix> file, and only split the module
if it is newer.

Typical use of AutoSplit in the perl MakeMaker utility is via the command-line
with:

 perl -e 'use AutoSplit; autosplit($ARGV[0], $ARGV[1], 0, 1, 1)'

Defined as a Make macro, it is invoked with file and directory arguments;
C<autosplit> will split the specified file into the specified directory and
delete obsolete C<.al> files, after checking first that the module does use
the AutoLoader, and ensuring that the module is not already currently split
in its current form (the modtime test).

The C<autosplit_lib_modules> form is used in the building of perl. It takes
as input a list of files (modules) that are assumed to reside in a directory
B<lib> relative to the current directory. Each file is sent to the 
autosplitter one at a time, to be split into the directory B<lib/auto>.

In both usages of the autosplitter, only subroutines defined following the
perl special marker I<__END__> are split out into separate files. Some
routines may be placed prior to this marker to force their immediate loading
and parsing.

=head1 CAVEATS

Currently, C<AutoSplit> cannot handle multiple package specifications
within one file.

=head1 DIAGNOSTICS

C<AutoSplit> will inform the user if it is necessary to create the top-level
directory specified in the invocation. It is preferred that the script or
installation process that invokes C<AutoSplit> have created the full directory
path ahead of time. This warning may indicate that the module is being split
into an incorrect path.

C<AutoSplit> will warn the user of all subroutines whose name causes potential
file naming conflicts on machines with drastically limited (8 characters or
less) file name length. Since the subroutine name is used as the file name,
these warnings can aid in portability to such systems.

Warnings are issued and the file skipped if C<AutoSplit> cannot locate either
the I<__END__> marker or a "package Name;"-style specification.

C<AutoSplit> will also emit general diagnostics for inability to create
directories or files.

=cut

# for portability warn about names longer than $maxlen
$Maxlen  = 8;	# 8 for dos, 11 (14-".al") for SYSVR3
$Verbose = 1;	# 0=none, 1=minimal, 2=list .al files
$Keep    = 0;
$CheckForAutoloader = 1;
$CheckModTime = 1;

$IndexFile = "autosplit.ix";	# file also serves as timestamp
$maxflen = 255;
$maxflen = 14 if $Config{'d_flexfnam'} ne 'define';
$Is_VMS = ($^O eq 'VMS');


sub autosplit{
    my($file, $autodir,  $k, $ckal, $ckmt) = @_;
    # $file    - the perl source file to be split (after __END__)
    # $autodir - the ".../auto" dir below which to write split subs
    # Handle optional flags:
    $keep = $Keep unless defined $k;
    $ckal = $CheckForAutoloader unless defined $ckal;
    $ckmt = $CheckModTime unless defined $ckmt;
    autosplit_file($file, $autodir, $keep, $ckal, $ckmt);
}


# This function is used during perl building/installation
# ./miniperl -e 'use AutoSplit; autosplit_lib_modules(@ARGV)' ...

sub autosplit_lib_modules{
    my(@modules) = @_; # list of Module names

    while(defined($_ = shift @modules)){
	s#::#/#g;	# incase specified as ABC::XYZ
	s|\\|/|g;		# bug in ksh OS/2
	s#^lib/##; # incase specified as lib/*.pm
	if ($Is_VMS && /[:>\]]/) { # may need to convert VMS-style filespecs
	    my ($dir,$name) = (/(.*])(.*)/);
	    $dir =~ s/.*lib[\.\]]//;
	    $dir =~ s#[\.\]]#/#g;
	    $_ = $dir . $name;
	}
	autosplit_file("lib/$_", "lib/auto", $Keep, $CheckForAutoloader, $CheckModTime);
    }
    0;
}


# private functions

sub autosplit_file{
    my($filename, $autodir, $keep, $check_for_autoloader, $check_mod_time) = @_;
    my(@names);
    local($_);

    # where to write output files
    $autodir = "lib/auto" unless $autodir;
    if ($Is_VMS) {
	($autodir = VMS::Filespec::unixpath($autodir)) =~ s{/$}{};
	$filename = VMS::Filespec::unixify($filename); # may have dirs
    }
    unless (-d $autodir){
	mkpath($autodir,0,0755);
	# We should never need to create the auto dir here. installperl
	# (or similar) should have done it. Expecting it to exist is a valuable
	# sanity check against autosplitting into some random directory by mistake.
	print "Warning: AutoSplit had to create top-level $autodir unexpectedly.\n";
    }

    # allow just a package name to be used
    $filename .= ".pm" unless ($filename =~ m/\.pm$/);

    open(IN, "<$filename") || die "AutoSplit: Can't open $filename: $!\n";
    my($pm_mod_time) = (stat($filename))[9];
    my($autoloader_seen) = 0;
    my($in_pod) = 0;
    while (<IN>) {
	# Skip pod text.
	$in_pod = 1 if /^=/;
	$in_pod = 0 if /^=cut/;
	next if ($in_pod || /^=cut/);

	# record last package name seen
	$package = $1 if (m/^\s*package\s+([\w:]+)\s*;/);
	++$autoloader_seen if m/^\s*(use|require)\s+AutoLoader\b/;
	++$autoloader_seen if m/\bISA\s*=.*\bAutoLoader\b/;
	last if /^__END__/;
    }
    if ($check_for_autoloader && !$autoloader_seen){
	print "AutoSplit skipped $filename: no AutoLoader used\n" if ($Verbose>=2);
	return 0
    }
    $_ or die "Can't find __END__ in $filename\n";

    $package or die "Can't find 'package Name;' in $filename\n";

    my($modpname) = $package; 
    if ($^O eq 'MSWin32') {
	$modpname =~ s#::#\\#g; 
    } else {
	$modpname =~ s#::#/#g;
    }

    die "Package $package ($modpname.pm) does not match filename $filename"
	    unless ($filename =~ m/\Q$modpname.pm\E$/ or
		    ($^O eq "msdos") or ($^O eq 'MSWin32') or
	            $Is_VMS && $filename =~ m/$modpname.pm/i);

    my($al_idx_file) = "$autodir/$modpname/$IndexFile";

    if ($check_mod_time){
	my($al_ts_time) = (stat("$al_idx_file"))[9] || 1;
	if ($al_ts_time >= $pm_mod_time){
	    print "AutoSplit skipped ($al_idx_file newer that $filename)\n"
		if ($Verbose >= 2);
	    return undef;	# one undef, not a list
	}
    }

    my($from) = ($Verbose>=2) ? "$filename => " : "";
    print "AutoSplitting $package ($from$autodir/$modpname)\n"
	if $Verbose;

    unless (-d "$autodir/$modpname"){
	mkpath("$autodir/$modpname",0,0777);
    }

    # We must try to deal with some SVR3 systems with a limit of 14
    # characters for file names. Sadly we *cannot* simply truncate all
    # file names to 14 characters on these systems because we *must*
    # create filenames which exactly match the names used by AutoLoader.pm.
    # This is a problem because some systems silently truncate the file
    # names while others treat long file names as an error.

    # We do not yet deal with multiple packages within one file.
    # Ideally both of these styles should work.
    #
    #   package NAME;
    #   __END__
    #   sub AAA { ... }
    #   package NAME::option1;
    #   sub BBB { ... }
    #   package NAME::option2;
    #   sub BBB { ... }
    #
    #   package NAME;
    #   __END__
    #   sub AAA { ... }
    #   sub NAME::option1::BBB { ... }
    #   sub NAME::option2::BBB { ... }
    #
    # For now both of these produce warnings.

    open(OUT,">/dev/null") || open(OUT,">nla0:"); # avoid 'not opened' warning
    my(@subnames, %proto);
    my @cache = ();
    my $caching = 1;
    while (<IN>) {
	next if /^=\w/ .. /^=cut/;
	if (/^package ([\w:]+)\s*;/) {
	    warn "package $1; in AutoSplit section ignored. Not currently supported.";
	}
	if (/^sub\s+([\w:]+)(\s*\(.*?\))?/) {
	    print OUT "1;\n";
	    my $subname = $1;
	    $proto{$1} = $2 || '';
	    if ($subname =~ m/::/){
		warn "subs with package names not currently supported in AutoSplit section";
	    }
	    push(@subnames, $subname);
	    my($lname, $sname) = ($subname, substr($subname,0,$maxflen-3));
	    my($lpath) = "$autodir/$modpname/$lname.al";
	    my($spath) = "$autodir/$modpname/$sname.al";
	    unless(open(OUT, ">$lpath")){
		open(OUT, ">$spath") or die "Can't create $spath: $!\n";
		push(@names, $sname);
		print "  writing $spath (with truncated name)\n"
			if ($Verbose>=1);
	    }else{
		push(@names, $lname);
		print "  writing $lpath\n" if ($Verbose>=2);
	    }
	    print OUT "# NOTE: Derived from $filename.  ",
			"Changes made here will be lost.\n";
	    print OUT "package $package;\n\n";
	    print OUT @cache;
	    @cache = ();
	    $caching = 0;
	}
	if($caching) {
	    push(@cache, $_) if @cache || /\S/;
	}
	else {
	    print OUT $_;
	}
	if(/^}/) {
	    if($caching) {
		print OUT @cache;
		@cache = ();
	    }
	    print OUT "\n";
	    $caching = 1;
	}
    }
    print OUT @cache,"1;\n";
    close(OUT);
    close(IN);

    if (!$keep){  # don't keep any obsolete *.al files in the directory
	my(%names);
	@names{@names} = @names;
	opendir(OUTDIR,"$autodir/$modpname");
	foreach(sort readdir(OUTDIR)){
	    next unless /\.al$/;
	    my($subname) = m/(.*)\.al$/;
	    next if $names{substr($subname,0,$maxflen-3)};
	    my($file) = "$autodir/$modpname/$_";
	    print "  deleting $file\n" if ($Verbose>=2);
	    my($deleted,$thistime);  # catch all versions on VMS
	    do { $deleted += ($thistime = unlink $file) } while ($thistime);
	    carp "Unable to delete $file: $!" unless $deleted;
	}
	closedir(OUTDIR);
    }

    open(TS,">$al_idx_file") or
	carp "AutoSplit: unable to create timestamp file ($al_idx_file): $!";
    print TS "# Index created by AutoSplit for $filename (file acts as timestamp)\n";
    print TS "package $package;\n";
    print TS map("sub $_$proto{$_} ;\n", @subnames);
    print TS "1;\n";
    close(TS);

    check_unique($package, $Maxlen, 1, @names);

    @names;
}


sub check_unique{
    my($module, $maxlen, $warn, @names) = @_;
    my(%notuniq) = ();
    my(%shorts)  = ();
    my(@toolong) = grep(length > $maxlen, @names);

    foreach(@toolong){
	my($trunc) = substr($_,0,$maxlen);
	$notuniq{$trunc}=1 if $shorts{$trunc};
	$shorts{$trunc} = ($shorts{$trunc}) ? "$shorts{$trunc}, $_" : $_;
    }
    if (%notuniq && $warn){
	print "$module: some names are not unique when truncated to $maxlen characters:\n";
	foreach(keys %notuniq){
	    print " $shorts{$_} truncate to $_\n";
	}
    }
    %notuniq;
}

1;
__END__

# test functions so AutoSplit.pm can be applied to itself:
sub test1{ "test 1\n"; }
sub test2{ "test 2\n"; }
sub test3{ "test 3\n"; }
sub test4{ "test 4\n"; }


