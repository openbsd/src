package ExtUtils::Install;

$VERSION = substr q$Revision: 1.1.1.1 $, 10;
# $Id: Install.pm,v 1.1.1.1 1996/08/19 10:12:39 downsj Exp $

use Exporter;
use Carp ();
use Config ();
use vars qw(@ISA @EXPORT $VERSION);
@ISA = ('Exporter');
@EXPORT = ('install','uninstall','pm_to_blib');
$Is_VMS = $^O eq 'VMS';

my $splitchar = $^O eq 'VMS' ? '|' : $^O eq 'os2' ? ';' : ':';
my @PERL_ENV_LIB = split $splitchar, defined $ENV{'PERL5LIB'} ? $ENV{'PERL5LIB'} : $ENV{'PERLLIB'};
my $Inc_uninstall_warn_handler;

#use vars qw( @EXPORT @ISA $Is_VMS );
#use strict;

sub forceunlink {
    chmod 0666, $_[0];
    unlink $_[0] or Carp::croak("Cannot forceunlink $_[0]: $!")
}

sub install {
    my($hash,$verbose,$nonono,$inc_uninstall) = @_;
    $verbose ||= 0;
    $nonono  ||= 0;

    use Cwd qw(cwd);
    use ExtUtils::MakeMaker; # to implement a MY class
    use File::Basename qw(dirname);
    use File::Copy qw(copy);
    use File::Find qw(find);
    use File::Path qw(mkpath);
    # The following lines were needed with AutoLoader (left for the record)
    # my $my_req = $self->catfile(qw(auto ExtUtils Install my_cmp.al));
    # require $my_req;
    # $my_req = $self->catfile(qw(auto ExtUtils Install forceunlink.al));
    # require $my_req; # Hairy, but for the first
    # time use we are in a different directory when autoload happens, so
    # the relativ path to ./blib is ill.

    my(%hash) = %$hash;
    my(%pack, %write, $dir);
    local(*DIR, *P);
    for (qw/read write/) {
	$pack{$_}=$hash{$_};
	delete $hash{$_};
    }
    my($source_dir_or_file);
    foreach $source_dir_or_file (sort keys %hash) {
	#Check if there are files, and if yes, look if the corresponding
	#target directory is writable for us
	opendir DIR, $source_dir_or_file or next;
	for (readdir DIR) {
	    next if $_ eq "." || $_ eq ".." || $_ eq ".exists";
	    if (-w $hash{$source_dir_or_file} || mkpath($hash{$source_dir_or_file})) {
		last;
	    } else {
		Carp::croak("You do not have permissions to install into $hash{$source_dir_or_file}");
	    }
	}
	closedir DIR;
    }
    if (-f $pack{"read"}) {
	open P, $pack{"read"} or Carp::croak("Couldn't read $pack{'read'}");
	# Remember what you found
	while (<P>) {
	    chomp;
	    $write{$_}++;
	}
	close P;
    }
    my $cwd = cwd();
    my $umask = umask 0 unless $Is_VMS;

    # This silly reference is just here to be able to call MY->catdir
    # without a warning (Waiting for a proper path/directory module,
    # Charles!)
    my $MY = {};
    bless $MY, 'MY';
    my($source);
    MOD_INSTALL: foreach $source (sort keys %hash) {
	#copy the tree to the target directory without altering
	#timestamp and permission and remember for the .packlist
	#file. The packlist file contains the absolute paths of the
	#install locations. AFS users may call this a bug. We'll have
	#to reconsider how to add the means to satisfy AFS users also.
	chdir($source) or next;
	find(sub {
	    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
                         $atime,$mtime,$ctime,$blksize,$blocks) = stat;
	    return unless -f _;
	    return if $_ eq ".exists";
	    my $targetdir = $MY->catdir($hash{$source},$File::Find::dir);
	    my $targetfile = $MY->catfile($targetdir,$_);

	    my $diff = 0;
	    if ( -f $targetfile && -s _ == $size) {
		# We have a good chance, we can skip this one
		$diff = my_cmp($_,$targetfile);
	    } else {
		print "$_ differs\n" if $verbose>1;
		$diff++;
	    }

	    if ($diff){
		if (-f $targetfile){
		    forceunlink($targetfile) unless $nonono;
		} else {
		    mkpath($targetdir,0,0755) unless $nonono;
		    print "mkpath($targetdir,0,0755)\n" if $verbose>1;
		}
		copy($_,$targetfile) unless $nonono;
		print "Installing $targetfile\n";
		utime($atime,$mtime + $Is_VMS,$targetfile) unless $nonono>1;
		print "utime($atime,$mtime,$targetfile)\n" if $verbose>1;
		$mode = 0444 | ( $mode & 0111 ? 0111 : 0 );
		chmod $mode, $targetfile;
		print "chmod($mode, $targetfile)\n" if $verbose>1;
	    } else {
		print "Skipping $targetfile (unchanged)\n" if $verbose;
	    }
	    
	    if (! defined $inc_uninstall) { # it's called 
	    } elsif ($inc_uninstall == 0){
		inc_uninstall($_,$File::Find::dir,$verbose,1); # nonono set to 1
	    } else {
		inc_uninstall($_,$File::Find::dir,$verbose,0); # nonono set to 0
	    }
	    $write{$targetfile}++;

	}, ".");
	chdir($cwd) or Carp::croak("Couldn't chdir to $cwd: $!");
    }
    umask $umask unless $Is_VMS;
    if ($pack{'write'}) {
	$dir = dirname($pack{'write'});
	mkpath($dir,0,0755);
	print "Writing $pack{'write'}\n";
	open P, ">$pack{'write'}" or Carp::croak("Couldn't write $pack{'write'}: $!");
	for (sort keys %write) {
	    print P "$_\n";
	}
	close P;
    }
}

sub my_cmp {
    my($one,$two) = @_;
    local(*F,*T);
    my $diff = 0;
    open T, $two or return 1;
    open F, $one or Carp::croak("Couldn't open $one: $!");
    my($fr, $tr, $fbuf, $tbuf, $size);
    $size = 1024;
    # print "Reading $one\n";
    while ( $fr = read(F,$fbuf,$size)) {
	unless (
		$tr = read(T,$tbuf,$size) and 
		$tbuf eq $fbuf
	       ){
	    # print "diff ";
	    $diff++;
	    last;
	}
	# print "$fr/$tr ";
    }
    # print "\n";
    close F;
    close T;
    $diff;
}

sub uninstall {
    my($fil,$verbose,$nonono) = @_;
    die "no packlist file found: $fil" unless -f $fil;
    # my $my_req = $self->catfile(qw(auto ExtUtils Install forceunlink.al));
    # require $my_req; # Hairy, but for the first
    local *P;
    open P, $fil or Carp::croak("uninstall: Could not read packlist file $fil: $!");
    while (<P>) {
	chomp;
	print "unlink $_\n" if $verbose;
	forceunlink($_) unless $nonono;
    }
    print "unlink $fil\n" if $verbose;
    forceunlink($fil) unless $nonono;
}

sub inc_uninstall {
    my($file,$libdir,$verbose,$nonono) = @_;
    my($dir);
    my $MY = {};
    bless $MY, 'MY';
    my %seen_dir = ();
    foreach $dir (@INC, @PERL_ENV_LIB, @Config::Config{qw/archlibexp privlibexp sitearchexp sitelibexp/}) {
	next if $dir eq ".";
	next if $seen_dir{$dir}++;
	my($targetfile) = $MY->catfile($dir,$libdir,$file);
	next unless -f $targetfile;

	# The reason why we compare file's contents is, that we cannot
	# know, which is the file we just installed (AFS). So we leave
	# an identical file in place
	my $diff = 0;
	if ( -f $targetfile && -s _ == -s $file) {
	    # We have a good chance, we can skip this one
	    $diff = my_cmp($file,$targetfile);
	} else {
	    print "#$file and $targetfile differ\n" if $verbose>1;
	    $diff++;
	}

	next unless $diff;
	if ($nonono) {
	    if ($verbose) {
		$Inc_uninstall_warn_handler ||= new ExtUtils::Install::Warn;
		$libdir =~ s|^\./|| ; # That's just cosmetics, no need to port. It looks prettier.
		$Inc_uninstall_warn_handler->add("$libdir/$file",$targetfile);
	    }
	    # if not verbose, we just say nothing
	} else {
	    print "Unlinking $targetfile (shadowing?)\n";
	    forceunlink($targetfile);
	}
    }
}

sub pm_to_blib {
    my($fromto,$autodir) = @_;

    use File::Basename qw(dirname);
    use File::Copy qw(copy);
    use File::Path qw(mkpath);
    use AutoSplit;
    # my $my_req = $self->catfile(qw(auto ExtUtils Install forceunlink.al));
    # require $my_req; # Hairy, but for the first

    my $umask = umask 0022 unless $Is_VMS;
    mkpath($autodir,0,0755);
    foreach (keys %$fromto) {
	next if -f $fromto->{$_} && -M $fromto->{$_} < -M $_;
	unless (my_cmp($_,$fromto->{$_})){
	    print "Skip $fromto->{$_} (unchanged)\n";
	    next;
	}
	if (-f $fromto->{$_}){
	    forceunlink($fromto->{$_});
	} else {
	    mkpath(dirname($fromto->{$_}),0,0755);
	}
	copy($_,$fromto->{$_});
	chmod(0444 | ( (stat)[2] & 0111 ? 0111 : 0 ),$fromto->{$_});
	print "cp $_ $fromto->{$_}\n";
	next unless /\.pm$/;
	autosplit($fromto->{$_},$autodir);
    }
    umask $umask unless $Is_VMS;
}

package ExtUtils::Install::Warn;

sub new { bless {}, shift }

sub add {
    my($self,$file,$targetfile) = @_;
    push @{$self->{$file}}, $targetfile;
}

sub DESTROY {
    my $self = shift;
    my($file,$i,$plural);
    foreach $file (sort keys %$self) {
	$plural = @{$self->{$file}} > 1 ? "s" : "";
	print "## Differing version$plural of $file found. You might like to\n";
	for (0..$#{$self->{$file}}) {
	    print "rm ", $self->{$file}[$_], "\n";
	    $i++;
	}
    }
    $plural = $i>1 ? "all those files" : "this file";
    print "## Running 'make install UNINST=1' will unlink $plural for you.\n";
}

1;

__END__

=head1 NAME

ExtUtils::Install - install files from here to there

=head1 SYNOPSIS

B<use ExtUtils::Install;>

B<install($hashref,$verbose,$nonono);>

B<uninstall($packlistfile,$verbose,$nonono);>

B<pm_to_blib($hashref);>

=head1 DESCRIPTION

Both install() and uninstall() are specific to the way
ExtUtils::MakeMaker handles the installation and deinstallation of
perl modules. They are not designed as general purpose tools.

install() takes three arguments. A reference to a hash, a verbose
switch and a don't-really-do-it switch. The hash ref contains a
mapping of directories: each key/value pair is a combination of
directories to be copied. Key is a directory to copy from, value is a
directory to copy to. The whole tree below the "from" directory will
be copied preserving timestamps and permissions.

There are two keys with a special meaning in the hash: "read" and
"write". After the copying is done, install will write the list of
target files to the file named by $hashref->{write}. If there is
another file named by $hashref->{read}, the contents of this file will
be merged into the written file. The read and the written file may be
identical, but on AFS it is quite likely, people are installing to a
different directory than the one where the files later appear.

uninstall() takes as first argument a file containing filenames to be
unlinked. The second argument is a verbose switch, the third is a
no-don't-really-do-it-now switch.

pm_to_blib() takes a hashref as the first argument and copies all keys
of the hash to the corresponding values efficiently. Filenames with
the extension pm are autosplit. Second argument is the autosplit
directory.

=cut

