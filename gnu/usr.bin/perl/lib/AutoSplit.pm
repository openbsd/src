package AutoSplit;

require 5.000;
require Exporter;

use Config;
use Carp;

@ISA = qw(Exporter);
@EXPORT = qw(&autosplit &autosplit_lib_modules);
@EXPORT_OK = qw($Verbose $Keep $Maxlen $CheckForAutoloader $CheckModTime);

=head1 NAME

AutoSplit - split a package for autoloading

=head1 SYNOPSIS

 perl -e 'use AutoSplit; autosplit_modules(@ARGV)'  ...

=head1 DESCRIPTION

This function will split up your program into files that the AutoLoader
module can handle.  Normally only used to build autoloading Perl library
modules, especially extensions (like POSIX).  You should look at how
they're built out for details.

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
# ./miniperl -e 'use AutoSplit; autosplit_modules(@ARGV)' ...

sub autosplit_lib_modules{
    my(@modules) = @_; # list of Module names

    foreach(@modules){
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

    # where to write output files
    $autodir = "lib/auto" unless $autodir;
    ($autodir = VMS::Filespec::unixpath($autodir)) =~ s#/$## if $Is_VMS;
    unless (-d $autodir){
	local($", @p)="/";
	foreach(split(/\//,$autodir)){
	    push(@p, $_);
	    next if -d "@p/";
	    mkdir("@p",0755) or die "AutoSplit unable to mkdir @p: $!";
	}
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

    my($modpname) = $package; $modpname =~ s#::#/#g;
    my($al_idx_file) = "$autodir/$modpname/$IndexFile";

    die "Package $package does not match filename $filename"
	    unless ($filename =~ m/$modpname.pm$/ or
	            $Is_VMS && $filename =~ m/$modpname.pm/i);

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
	local($", @p)="/";
	foreach(split(/\//,"$autodir/$modpname")){
	    push(@p, $_);
	    next if -d "@p/";
	    mkdir("@p",0777) or die "AutoSplit unable to mkdir @p: $!";
	}
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
    while (<IN>) {
	if (/^package ([\w:]+)\s*;/) {
	    warn "package $1; in AutoSplit section ignored. Not currently supported.";
	}
	if (/^sub\s+([\w:]+)(\s*\(.*?\))?/) {
	    print OUT "1;\n";
	    my $subname = $1;
	    $proto{$1} = $2 or '';
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
	}
	print OUT $_;
    }
    print OUT "1;\n";
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


