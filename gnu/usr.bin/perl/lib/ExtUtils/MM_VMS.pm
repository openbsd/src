#   MM_VMS.pm
#   MakeMaker default methods for VMS
#   This package is inserted into @ISA of MakeMaker's MM before the
#   built-in ExtUtils::MM_Unix methods if MakeMaker.pm is run under VMS.
#
#   Author:  Charles Bailey  bailey@genetics.upenn.edu

package ExtUtils::MM_VMS;
$ExtUtils::MM_VMS::Revision=$ExtUtils::MM_VMS::Revision = '5.35 (23-Jun-1996)';
unshift @MM::ISA, 'ExtUtils::MM_VMS';

use Config;
require Exporter;
use VMS::Filespec;
use File::Basename;

Exporter::import('ExtUtils::MakeMaker', '$Verbose', '&neatvalue');

=head1 NAME

ExtUtils::MM_VMS - methods to override UN*X behaviour in ExtUtils::MakeMaker

=head1 SYNOPSIS

 use ExtUtils::MM_VMS; # Done internally by ExtUtils::MakeMaker if needed

=head1 DESCRIPTION

See ExtUtils::MM_Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=head2 Methods always loaded

=item eliminate_macros

Expands MM[KS]/Make macros in a text string, using the contents of
identically named elements of C<%$self>, and returns the result
as a file specification in Unix syntax.

=cut

sub eliminate_macros {
    my($self,$path) = @_;
    unless ($path) {
	print "eliminate_macros('') = ||\n" if $Verbose >= 3;
	return '';
    }
    my($npath) = unixify($path);
    my($head,$macro,$tail);

    # perform m##g in scalar context so it acts as an iterator
    while ($npath =~ m#(.*?)\$\((\S+?)\)(.*)#g) { 
        if ($self->{$2}) {
            ($head,$macro,$tail) = ($1,$2,$3);
            ($macro = unixify($self->{$macro})) =~ s#/$##;
            $npath = "$head$macro$tail";
        }
    }
    print "eliminate_macros($path) = |$npath|\n" if $Verbose >= 3;
    $npath;
}

=item fixpath

Catchall routine to clean up problem MM[SK]/Make macros.  Expands macros
in any directory specification, in order to avoid juxtaposing two
VMS-syntax directories when MM[SK] is run.  Also expands expressions which
are all macro, so that we can tell how long the expansion is, and avoid
overrunning DCL's command buffer when MM[KS] is running.

If optional second argument has a TRUE value, then the return string is
a VMS-syntax directory specification, otherwise it is a VMS-syntax file
specification.

=cut

sub fixpath {
    my($self,$path,$force_path) = @_;
    unless ($path) {
	print "eliminate_macros('') = ||\n" if $Verbose >= 3;
	return '';
    }
    my($fixedpath,$prefix,$name);

    if ($path =~ m#^\$\(.+\)$# || $path =~ m#[/:>\]]#) { 
        if ($force_path or $path =~ /(?:DIR\)|\])$/) {
            $fixedpath = vmspath($self->eliminate_macros($path));
        }
        else {
            $fixedpath = vmsify($self->eliminate_macros($path));
        }
    }
    elsif ((($prefix,$name) = ($path =~ m#^\$\(([^\)]+)\)(.+)#)) && $self->{$prefix}) {
        my($vmspre) = vmspath($self->{$prefix}) || ''; # is it a dir or just a name?
        $fixedpath = ($vmspre ? $vmspre : $self->{$prefix}) . $name;
        $fixedpath = vmspath($fixedpath) if $force_path;
    }
    else {
        $fixedpath = $path;
        $fixedpath = vmspath($fixedpath) if $force_path;
    }
    # Convert names without directory or type to paths
    if (!$force_path and $fixedpath !~ /[:>(.\]]/) { $fixedpath = vmspath($fixedpath); }
    print "fixpath($path) = |$fixedpath|\n" if $Verbose >= 3;
    $fixedpath;
}

=item catdir

Concatenates a list of file specifications, and returns the result as a
VMS-syntax directory specification.

=cut

sub catdir {
    my($self,@dirs) = @_;
    my($dir) = pop @dirs;
    @dirs = grep($_,@dirs);
    my($rslt);
    if (@dirs) {
      my($path) = (@dirs == 1 ? $dirs[0] : $self->catdir(@dirs));
      my($spath,$sdir) = ($path,$dir);
      $spath =~ s/.dir$//; $sdir =~ s/.dir$//; 
      $sdir = $self->eliminate_macros($sdir) unless $sdir =~ /^[\w\-]+$/;
      $rslt = vmspath($self->eliminate_macros($spath)."/$sdir");
    }
    else { $rslt = vmspath($dir); }
    print "catdir(",join(',',@_[1..$#_]),") = |$rslt|\n" if $Verbose >= 3;
    $rslt;
}

=item catfile

Concatenates a list of file specifications, and returns the result as a
VMS-syntax directory specification.

=cut

sub catfile {
    my($self,@files) = @_;
    my($file) = pop @files;
    @files = grep($_,@files);
    my($rslt);
    if (@files) {
      my($path) = (@files == 1 ? $files[0] : $self->catdir(@files));
      my($spath) = $path;
      $spath =~ s/.dir$//;
      if ( $spath =~ /^[^\)\]\/:>]+\)$/ && basename($file) eq $file) { $rslt = "$spath$file"; }
      else {
          $rslt = $self->eliminate_macros($spath);
          $rslt = vmsify($rslt.($rslt ? '/' : '').unixify($file));
      }
    }
    else { $rslt = vmsify($file); }
    print "catfile(",join(',',@_[1..$#_]),") = |$rslt|\n" if $Verbose >= 3;
    $rslt;
}

=item curdir (override)

Returns a string representing of the current directory.

=cut

sub curdir {
    return '[]';
}

=item rootdir (override)

Returns a string representing of the root directory.

=cut

sub rootdir {
    return '';
}

=item updir (override)

Returns a string representing of the parent directory.

=cut

sub updir {
    return '[-]';
}

package ExtUtils::MM_VMS;

sub ExtUtils::MM_VMS::guess_name;
sub ExtUtils::MM_VMS::find_perl;
sub ExtUtils::MM_VMS::path;
sub ExtUtils::MM_VMS::maybe_command;
sub ExtUtils::MM_VMS::maybe_command_in_dirs;
sub ExtUtils::MM_VMS::perl_script;
sub ExtUtils::MM_VMS::file_name_is_absolute;
sub ExtUtils::MM_VMS::replace_manpage_separator;
sub ExtUtils::MM_VMS::init_others;
sub ExtUtils::MM_VMS::constants;
sub ExtUtils::MM_VMS::const_loadlibs;
sub ExtUtils::MM_VMS::cflags;
sub ExtUtils::MM_VMS::const_cccmd;
sub ExtUtils::MM_VMS::pm_to_blib;
sub ExtUtils::MM_VMS::tool_autosplit;
sub ExtUtils::MM_VMS::tool_xsubpp;
sub ExtUtils::MM_VMS::xsubpp_version;
sub ExtUtils::MM_VMS::tools_other;
sub ExtUtils::MM_VMS::dist;
sub ExtUtils::MM_VMS::c_o;
sub ExtUtils::MM_VMS::xs_c;
sub ExtUtils::MM_VMS::xs_o;
sub ExtUtils::MM_VMS::top_targets;
sub ExtUtils::MM_VMS::dlsyms;
sub ExtUtils::MM_VMS::dynamic_lib;
sub ExtUtils::MM_VMS::dynamic_bs;
sub ExtUtils::MM_VMS::static_lib;
sub ExtUtils::MM_VMS::manifypods;
sub ExtUtils::MM_VMS::processPL;
sub ExtUtils::MM_VMS::installbin;
sub ExtUtils::MM_VMS::subdir_x;
sub ExtUtils::MM_VMS::clean;
sub ExtUtils::MM_VMS::realclean;
sub ExtUtils::MM_VMS::dist_basics;
sub ExtUtils::MM_VMS::dist_core;
sub ExtUtils::MM_VMS::dist_dir;
sub ExtUtils::MM_VMS::dist_test;
sub ExtUtils::MM_VMS::install;
sub ExtUtils::MM_VMS::perldepend;
sub ExtUtils::MM_VMS::makefile;
sub ExtUtils::MM_VMS::test;
sub ExtUtils::MM_VMS::test_via_harness;
sub ExtUtils::MM_VMS::test_via_script;
sub ExtUtils::MM_VMS::makeaperl;
sub ExtUtils::MM_VMS::ext;
sub ExtUtils::MM_VMS::nicetext;

#use SelfLoader;
sub AUTOLOAD {
    my $code;
    if (defined fileno(DATA)) {
	my $fh = select DATA;
	my $o = $/;			# For future reads from the file.
	$/ = "\n__END__\n";
	$code = <DATA>;
	$/ = $o;
	select $fh;
	close DATA;
	eval $code;
	if ($@) {
	    $@ =~ s/ at .*\n//;
	    Carp::croak $@;
	}
    } else {
	warn "AUTOLOAD called unexpectedly for $AUTOLOAD"; 
    }
    defined(&$AUTOLOAD) or die "Myloader inconsistency error";
    goto &$AUTOLOAD;
}

1;

#__DATA__

=head2 SelfLoaded methods

Those methods which override default MM_Unix methods are marked
"(override)", while methods unique to MM_VMS are marked "(specific)".
For overridden methods, documentation is limited to an explanation
of why this method overrides the MM_Unix method; see the ExtUtils::MM_Unix
documentation for more details.

=item guess_name (override)

Try to determine name of extension being built.  We begin with the name
of the current directory.  Since VMS filenames are case-insensitive,
however, we look for a F<.pm> file whose name matches that of the current
directory (presumably the 'main' F<.pm> file for this extension), and try
to find a C<package> statement from which to obtain the Mixed::Case
package name.

=cut

sub guess_name {
    my($self) = @_;
    my($defname,$defpm);
    local *PM;

    $defname = basename(fileify($ENV{'DEFAULT'}));
    $defname =~ s![\d\-_]*\.dir.*$!!;  # Clip off .dir;1 suffix, and package version
    $defpm = $defname;
    if (open(PM,"${defpm}.pm")){
        while (<PM>) {
            if (/^\s*package\s+([^;]+)/i) {
                $defname = $1;
                last;
            }
        }
        print STDOUT "Warning (non-fatal): Couldn't find package name in ${defpm}.pm;\n\t",
                     "defaulting package name to $defname\n"
            if eof(PM);
        close PM;
    }
    else {
        print STDOUT "Warning (non-fatal): Couldn't find ${defpm}.pm;\n\t",
                     "defaulting package name to $defname\n";
    }
    $defname =~ s#[\d.\-_]+$##;
    $defname;
}

=item find_perl (override)

Use VMS file specification syntax and CLI commands to find and
invoke Perl images.

=cut

sub find_perl{
    my($self, $ver, $names, $dirs, $trace) = @_;
    my($name,$dir,$vmsfile,@sdirs,@snames,@cand);
    # Check in relative directories first, so we pick up the current
    # version of Perl if we're running MakeMaker as part of the main build.
    @sdirs = sort { my($absb) = file_name_is_absolute($a);
                    my($absb) = file_name_is_absolute($b);
                    if ($absa && $absb) { return $a cmp $b }
                    else { return $absa ? 1 : ($absb ? -1 : ($a cmp $b)); }
                  } @$dirs;
    # Check miniperl before perl, and check names likely to contain
    # version numbers before "generic" names, so we pick up an
    # executable that's less likely to be from an old installation.
    @snames = sort { my($ba) = $a =~ m!([^:>\]/]+)$!;  # basename
                     my($bb) = $b =~ m!([^:>\]/]+)$!;
                     substr($ba,0,1) cmp substr($bb,0,1)
                     or -1*(length($ba) <=> length($bb)) } @$names;
    if ($trace){
	print "Looking for perl $ver by these names:\n";
	print "\t@snames,\n";
	print "in these dirs:\n";
	print "\t@sdirs\n";
    }
    foreach $dir (@sdirs){
	next unless defined $dir; # $self->{PERL_SRC} may be undefined
	foreach $name (@snames){
	    if ($name !~ m![/:>\]]!) { push(@cand,$self->catfile($dir,$name)); }
	    else                     { push(@cand,$self->fixpath($name));      }
	}
    }
    foreach $name (@cand) {
	print "Checking $name\n" if ($trace >= 2);
	next unless $vmsfile = $self->maybe_command($name);
	$vmsfile =~ s/;[\d\-]*$//;  # Clip off version number; we can use a newer version as well
	print "Executing $vmsfile\n" if ($trace >= 2);
	if (`MCR $vmsfile -e "require $ver; print ""VER_OK\n"""` =~ /VER_OK/) {
	    print "Using PERL=MCR $vmsfile\n" if $trace;
	    return "MCR $vmsfile"
	}
    }
    print STDOUT "Unable to find a perl $ver (by these names: @$names, in these dirs: @$dirs)\n";
    0; # false and not empty
}

=item path (override)

Translate logical name DCL$PATH as a searchlist, rather than trying
to C<split> string value of C<$ENV{'PATH'}>.

=cut

sub path {
    my(@dirs,$dir,$i);
    while ($dir = $ENV{'DCL$PATH;' . $i++}) { push(@dirs,$dir); }
    @dirs;
}

=item maybe_command (override)

Follows VMS naming conventions for executable files.
If the name passed in doesn't exactly match an executable file,
appends F<.Exe> to check for executable image, and F<.Com> to check
for DCL procedure.  If this fails, checks F<Sys$Share:> for an
executable file having the name specified.  Finally, appends F<.Exe>
and checks again.

=cut

sub maybe_command {
    my($self,$file) = @_;
    return $file if -x $file && ! -d _;
    return "$file.exe" if -x "$file.exe";
    return "$file.com" if -x "$file.com";
    if ($file !~ m![/:>\]]!) {
	my($shrfile) = 'Sys$Share:' . $file;
	return $file if -x $shrfile && ! -d _;
	return "$file.exe" if -x "$shrfile.exe";
    }
    return 0;
}

=item maybe_command_in_dirs (override)

Uses DCL argument quoting on test command line.

=cut

sub maybe_command_in_dirs {	# $ver is optional argument if looking for perl
    my($self, $names, $dirs, $trace, $ver) = @_;
    my($name, $dir);
    foreach $dir (@$dirs){
	next unless defined $dir; # $self->{PERL_SRC} may be undefined
	foreach $name (@$names){
	    my($abs,$tryabs);
	    if ($self->file_name_is_absolute($name)) {
		$abs = $name;
	    } else {
		$abs = $self->catfile($dir, $name);
	    }
	    print "Checking $abs for $name\n" if ($trace >= 2);
	    next unless $tryabs = $self->maybe_command($abs);
	    print "Substituting $tryabs instead of $abs\n" 
		if ($trace >= 2 and $tryabs ne $abs);
	    $abs = $tryabs;
	    if (defined $ver) {
		print "Executing $abs\n" if ($trace >= 2);
		if (`$abs -e 'require $ver; print "VER_OK\n" ' 2>&1` =~ /VER_OK/) {
		    print "Using PERL=$abs\n" if $trace;
		    return $abs;
		}
	    } else { # Do not look for perl
		return $abs;
	    }
	}
    }
}

=item perl_script (override)

If name passed in doesn't specify a readable file, appends F<.pl> and
tries again, since it's customary to have file types on all files
under VMS.

=cut

sub perl_script {
    my($self,$file) = @_;
    return $file if -r $file && ! -d _;
    return "$file.pl" if -r "$file.pl" && ! -d _;
    return '';
}

=item file_name_is_absolute (override)

Checks for VMS directory spec as well as Unix separators.

=cut

sub file_name_is_absolute {
    my($self,$file);
    $file =~ m!^/! or $file =~ m![:<\[][^.\-]!;
}

=item replace_manpage_separator

Use as separator a character which is legal in a VMS-syntax file name.

=cut

sub replace_manpage_separator {
    my($self,$man) = @_;
    $man = unixify($man);
    $man =~ s#/+#__#g;
    $man;
}

=item init_others (override)

Provide VMS-specific forms of various utility commands, then hand
off to the default MM_Unix method.

=cut

sub init_others {
    my($self) = @_;

    $self->{NOOP} = "\t@ Continue";
    $self->{FIRST_MAKEFILE} ||= 'Descrip.MMS';
    $self->{MAKE_APERL_FILE} ||= 'Makeaperl.MMS';
    $self->{MAKEFILE} ||= $self->{FIRST_MAKEFILE};
    $self->{NOECHO} ||= '@ ';
    $self->{RM_F} = '$(PERL) -e "foreach (@ARGV) { 1 while ( -d $_ ? rmdir $_ : unlink $_)}"';
    $self->{RM_RF} = '$(PERL) "-I$(PERL_LIB)" -e "use File::Path; @dirs = map(VMS::Filespec::unixify($_),@ARGV); rmtree(\@dirs,0,0)"';
    $self->{TOUCH} = '$(PERL) -e "$t=time; foreach (@ARGV) { -e $_ ? utime($t,$t,@ARGV) : (open(F,qq(>$_)),close F)}"';
    $self->{CHMOD} = '$(PERL) -e "chmod @ARGV"';  # expect Unix syntax from MakeMaker
    $self->{CP} = 'Copy/NoConfirm';
    $self->{MV} = 'Rename/NoConfirm';
    $self->{UMASK_NULL} = "\t!";  
    &ExtUtils::MM_Unix::init_others;
}

=item constants (override)

Fixes up numerous file and directory macros to insure VMS syntax
regardless of input syntax.  Also adds a few VMS-specific macros
and makes lists of files comma-separated.

=cut

sub constants {
    my($self) = @_;
    my(@m,$def,$macro);

    if ($self->{DEFINE} ne '') {
	my(@defs) = split(/\s+/,$self->{DEFINE});
	foreach $def (@defs) {
	    next unless $def;
	    $def =~ s/^-D//;
	    $def = "\"$def\"" if $def =~ /=/;
	}
	$self->{DEFINE} = join ',',@defs;
    }

    if ($self->{OBJECT} =~ /\s/) {
	$self->{OBJECT} =~ s/(\\)?\n+\s+/ /g;
	$self->{OBJECT} = map($self->fixpath($_),split(/,?\s+/,$self->{OBJECT}));
    }
    $self->{LDFROM} = join(' ',map($self->fixpath($_),split(/,?\s+/,$self->{LDFROM})));

    if ($self->{'INC'} && $self->{INC} !~ m!/Include=!i) {
	my(@val) = ( '/Include=(' );
	my(@includes) = split(/\s+/,$self->{INC});
	my($plural);
	foreach (@includes) {
	    s/^-I//;
	    push @val,', ' if $plural++;
	    push @val,$self->fixpath($_,1);
	}
	$self->{INC} = join('',@val,')');
    }

    # Fix up directory specs
    $self->{ROOTEXT} = $self->{ROOTEXT} ? $self->fixpath($self->{ROOTEXT},1)
                                        : '[]';
    foreach $macro ( qw [
            INST_BIN INST_SCRIPT INST_LIB INST_ARCHLIB INST_EXE INSTALLPRIVLIB
            INSTALLARCHLIB INSTALLSCRIPT INSTALLBIN PERL_LIB PERL_ARCHLIB
            PERL_INC PERL_SRC FULLEXT INST_MAN1DIR INSTALLMAN1DIR
            INST_MAN3DIR INSTALLMAN3DIR INSTALLSITELIB INSTALLSITEARCH
            SITELIBEXP SITEARCHEXP ] ) {
	next unless defined $self->{$macro};
	$self->{$macro} = $self->fixpath($self->{$macro},1);
    }
    $self->{PERL_VMS} = $self->catdir($self->{PERL_SRC},q(VMS))
	if ($self->{PERL_SRC});
                        


    # Fix up file specs
    foreach $macro ( qw[LIBPERL_A FIRST_MAKEFILE MAKE_APERL_FILE MYEXTLIB] ) {
	next unless defined $self->{$macro};
	$self->{$macro} = $self->fixpath($self->{$macro});
    }

    foreach $macro (qw/
	      AR_STATIC_ARGS NAME DISTNAME NAME_SYM VERSION VERSION_SYM XS_VERSION
	      INST_BIN INST_EXE INST_LIB INST_ARCHLIB INST_SCRIPT PREFIX
	      INSTALLDIRS INSTALLPRIVLIB  INSTALLARCHLIB INSTALLSITELIB
	      INSTALLSITEARCH INSTALLBIN INSTALLSCRIPT PERL_LIB
	      PERL_ARCHLIB SITELIBEXP SITEARCHEXP LIBPERL_A MYEXTLIB
	      FIRST_MAKEFILE MAKE_APERL_FILE PERLMAINCC PERL_SRC PERL_VMS
	      PERL_INC PERL FULLPERL
	      / ) {
	next unless defined $self->{$macro};
	push @m, "$macro = $self->{$macro}\n";
    }


    push @m, q[
VERSION_MACRO = VERSION
DEFINE_VERSION = "$(VERSION_MACRO)=""$(VERSION)"""
XS_VERSION_MACRO = XS_VERSION
XS_DEFINE_VERSION = "$(XS_VERSION_MACRO)=""$(XS_VERSION)"""

MAKEMAKER = ],$self->catfile($self->{PERL_LIB},'ExtUtils','MakeMaker.pm'),qq[
MM_VERSION = $ExtUtils::MakeMaker::VERSION
MM_REVISION = $ExtUtils::MakeMaker::Revision
MM_VMS_REVISION = $ExtUtils::MM_VMS::Revision

# FULLEXT = Pathname for extension directory (eg DBD/Oracle).
# BASEEXT = Basename part of FULLEXT. May be just equal FULLEXT.
# PARENT_NAME = NAME without BASEEXT and no trailing :: (eg Foo::Bar)
# DLBASE  = Basename part of dynamic library. May be just equal BASEEXT.
];

    for $tmp (qw/
	      FULLEXT BASEEXT PARENT_NAME DLBASE VERSION_FROM INC DEFINE OBJECT
	      LDFROM LINKTYPE
	      /	) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }

    for $tmp (qw/ XS MAN1PODS MAN3PODS PM /) {
	next unless defined $self->{$tmp};
	my(%tmp,$key);
	for $key (keys %{$self->{$tmp}}) {
	    $tmp{$self->fixpath($key)} = $self->fixpath($self->{$tmp}{$key});
	}
	$self->{$tmp} = \%tmp;
    }

    for $tmp (qw/ C O_FILES H /) {
	next unless defined $self->{$tmp};
	my(@tmp,$val);
	for $val (@{$self->{$tmp}}) {
	    push(@tmp,$self->fixpath($val));
	}
	$self->{$tmp} = \@tmp;
    }

    push @m,'

# Handy lists of source code files:
XS_FILES = ',join(', ', sort keys %{$self->{XS}}),'
C_FILES  = ',join(', ', @{$self->{C}}),'
O_FILES  = ',join(', ', @{$self->{O_FILES}} ),'
H_FILES  = ',join(', ', @{$self->{H}}),'
MAN1PODS = ',join(', ', sort keys %{$self->{MAN1PODS}}),'
MAN3PODS = ',join(', ', sort keys %{$self->{MAN3PODS}}),'

';

    for $tmp (qw/
	      INST_MAN1DIR INSTALLMAN1DIR MAN1EXT INST_MAN3DIR INSTALLMAN3DIR MAN3EXT
	      /) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }

push @m,"
.SUFFIXES : \$(OBJ_EXT) .c .cpp .cxx .xs

# Here is the Config.pm that we are using/depend on
CONFIGDEP = \$(PERL_ARCHLIB)Config.pm, \$(PERL_INC)config.h \$(VERSION_FROM)

# Where to put things:
INST_LIBDIR = ",($self->{'INST_LIBDIR'} = $self->catdir($self->{INST_LIB},$self->{ROOTEXT})),"
INST_ARCHLIBDIR = ",($self->{'INST_ARCHLIBDIR'} = $self->catdir($self->{INST_ARCHLIB},$self->{ROOTEXT})),"

INST_AUTODIR = ",($self->{'INST_AUTODIR'} = $self->catdir($self->{INST_LIB},'auto',$self->{FULLEXT})),'
INST_ARCHAUTODIR = ',($self->{'INST_ARCHAUTODIR'} = $self->catdir($self->{INST_ARCHLIB},'auto',$self->{FULLEXT})),'
';

    if ($self->has_link_code()) {
	push @m,'
INST_STATIC = $(INST_ARCHAUTODIR)$(BASEEXT)$(LIB_EXT)
INST_DYNAMIC = $(INST_ARCHAUTODIR)$(BASEEXT).$(DLEXT)
INST_BOOT = $(INST_ARCHAUTODIR)$(BASEEXT).bs
';
    } else {
	push @m,'
INST_STATIC =
INST_DYNAMIC =
INST_BOOT =
EXPORT_LIST = $(BASEEXT).opt
PERL_ARCHIVE = ',($ENV{'PERLSHR'} ? $ENV{'PERLSHR'} : 'Sys$Share:PerlShr.Exe'),'
';
    }

    $self->{TO_INST_PM} = [ sort keys %{$self->{PM}} ];
    $self->{PM_TO_BLIB} = [ %{$self->{PM}} ];
    push @m,'
TO_INST_PM = ',join(', ',@{$self->{TO_INST_PM}}),'

PM_TO_BLIB = ',join(', ',@{$self->{PM_TO_BLIB}}),'
';

    join('',@m);
}

=item const_loadlibs (override)

Basically a stub which passes through library specfications provided
by the caller.  Will be updated or removed when VMS support is added
to ExtUtils::Liblist.

=cut

sub const_loadlibs{
    my($self) = @_;
    my (@m);
    push @m, "
# $self->{NAME} might depend on some other libraries.
# (These comments may need revising:)
#
# Dependent libraries can be linked in one of three ways:
#
#  1.  (For static extensions) by the ld command when the perl binary
#      is linked with the extension library. See EXTRALIBS below.
#
#  2.  (For dynamic extensions) by the ld command when the shared
#      object is built/linked. See LDLOADLIBS below.
#
#  3.  (For dynamic extensions) by the DynaLoader when the shared
#      object is loaded. See BSLOADLIBS below.
#
# EXTRALIBS =	List of libraries that need to be linked with when
#		linking a perl binary which includes this extension
#		Only those libraries that actually exist are included.
#		These are written to a file and used when linking perl.
#
# LDLOADLIBS =	List of those libraries which can or must be linked into
#		the shared library when created using ld. These may be
#		static or dynamic libraries.
#		LD_RUN_PATH is a colon separated list of the directories
#		in LDLOADLIBS. It is passed as an environment variable to
#		the process that links the shared library.
#
# BSLOADLIBS =	List of those libraries that are needed but can be
#		linked in dynamically at run time on this platform.
#		SunOS/Solaris does not need this because ld records
#		the information (from LDLOADLIBS) into the object file.
#		This list is used to create a .bs (bootstrap) file.
#
EXTRALIBS  = ",map($self->fixpath($_) . ' ',$self->{'EXTRALIBS'}),"
BSLOADLIBS = ",map($self->fixpath($_) . ' ',$self->{'BSLOADLIBS'}),"
LDLOADLIBS = ",map($self->fixpath($_) . ' ',$self->{'LDLOADLIBS'}),"\n";

    join('',@m);
}

=item cflags (override)

Bypass shell script and produce qualifiers for CC directly (but warn
user if a shell script for this extension exists).  Fold multiple
/Defines into one, and do the same with /Includes, since some C
compilers pay attention to only one instance of these qualifiers
on the command line.

=cut

sub cflags {
    my($self,$libperl) = @_;
    my($quals) = $Config{'ccflags'};
    my($name,$sys,@m);
    my($optimize) = '/Optimize';

    ( $name = $self->{NAME} . "_cflags" ) =~ s/:/_/g ;
    print STDOUT "Unix shell script ".$Config{"$self->{'BASEEXT'}_cflags"}.
         " required to modify CC command for $self->{'BASEEXT'}\n"
    if ($Config{$name});

    # Deal with $self->{DEFINE} here since some C compilers pay attention
    # to only one /Define clause on command line, so we have to
    # conflate the ones from $Config{'cc'} and $self->{DEFINE}
    if ($quals =~ m:(.*)/define=\(?([^\(\/\)\s]+)\)?(.*)?:i) {
	$quals = "$1/Define=($2," . ($self->{DEFINE} ? "$self->{DEFINE}," : '') .
	         "\$(DEFINE_VERSION),\$(XS_DEFINE_VERSION))$3";
    }
    else {
	$quals .= '/Define=(' . ($self->{DEFINE} ? "$self->{DEFINE}," : '') .
	          '$(DEFINE_VERSION),$(XS_DEFINE_VERSION))';
    }

    $libperl or $libperl = $self->{LIBPERL_A} || "libperl.olb";
    if ($libperl =~ /libperl(\w+)\./i) {
        my($type) = uc $1;
        my(%map) = ( 'D'  => 'DEBUGGING', 'E' => 'EMBED', 'M' => 'MULTIPLICITY',
                     'DE' => 'DEBUGGING,EMBED', 'DM' => 'DEBUGGING,MULTIPLICITY',
                     'EM' => 'EMBED,MULTIPLICITY', 'DEM' => 'DEBUGGING,EMBED,MULTIPLICITY' );
        $quals =~ s:/define=\(([^\)]+)\):/Define=($1,$map{$type}):i
    }

    # Likewise with $self->{INC} and /Include
    my($incstr) = '/Include=($(PERL_INC)';
    if ($self->{'INC'}) {
	my(@includes) = split(/\s+/,$self->{INC});
	foreach (@includes) {
	    s/^-I//;
	    $incstr .= ', '.$self->fixpath($_,1);
	}
    }
    if ($quals =~ m:(.*)/include=\(?([^\(\/\)\s]+)\)?(.*):i) {
	$quals = "$1$incstr,$2)$3";
    }
    else { $quals .= "$incstr)"; }

    $optimize = '/Debug/NoOptimize'
	if ($self->{OPTIMIZE} =~ /-g/ or $self->{OPTIMIZE} =~ m!/Debug!i);

    return $self->{CFLAGS} = qq{
CCFLAGS = $quals
OPTIMIZE = $optimize
PERLTYPE =
SPLIT =
LARGE =
};
}

=item const_cccmd (override)

Adds directives to point C preprocessor to the right place when
handling #include <sys/foo.h> directives.  Also constructs CC
command line a bit differently than MM_Unix method.

=cut

sub const_cccmd {
    my($self,$libperl) = @_;
    my(@m);

    return $self->{CONST_CCCMD} if $self->{CONST_CCCMD};
    return '' unless $self->needs_linking();
    if ($Config{'vms_cc_type'} eq 'gcc') {
        push @m,'
.FIRST
	',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" Then Define/NoLog SYS GNU_CC_Include:[VMS]';
    }
    elsif ($Config{'vms_cc_type'} eq 'vaxc') {
        push @m,'
.FIRST
	',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" .and. F$TrnLnm("VAXC$Include").eqs."" Then Define/NoLog SYS Sys$Library
	',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" .and. F$TrnLnm("VAXC$Include").nes."" Then Define/NoLog SYS VAXC$Include';
    }
    else {
        push @m,'
.FIRST
	',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" .and. F$TrnLnm("DECC$System_Include").eqs."" Then Define/NoLog SYS ',
		($Config{'arch'} eq 'VMS_AXP' ? 'Sys$Library' : 'DECC$Library_Include'),'
	',$self->{NOECHO},'If F$TrnLnm("Sys").eqs."" .and. F$TrnLnm("DECC$System_Include").nes."" Then Define/NoLog SYS DECC$System_Include';
    }

    push(@m, "\n\nCCCMD = $Config{'cc'} \$(CCFLAGS)\$(OPTIMIZE)\n");

    $self->{CONST_CCCMD} = join('',@m);
}

=item pm_to_blib (override)

DCL I<still> accepts a maximum of 255 characters on a command
line, so we write the (potentially) long list of file names
to a temp file, then persuade Perl to read it instead of the
command line to find args.

=cut

sub pm_to_blib {
    my($self) = @_;
    my($line,$from,$to,@m);
    my($autodir) = $self->catdir('$(INST_LIB)','auto');
    my(@files) = @{$self->{PM_TO_BLIB}};

    push @m, q{
# As always, keep under DCL's 255-char limit
pm_to_blib : $(TO_INST_PM)
	},$self->{NOECHO},q{$(PERL) -e "print '},shift(@files),q{ },shift(@files),q{'" >.MM_tmp
};

    $line = '';  # avoid uninitialized var warning
    while ($from = shift(@files),$to = shift(@files)) {
	$line .= " $from $to";
	if (length($line) > 128) {
	    push(@m,"\t$self->{NOECHO}\$(PERL) -e \"print '$line'\" >>.MM_tmp\n");
	    $line = '';
	}
    }
    push(@m,"\t$self->{NOECHO}\$(PERL) -e \"print '$line'\" >>.MM_tmp\n") if $line;

    push(@m,q[	$(PERL) "-I$(PERL_LIB)" "-MExtUtils::Install" -e "pm_to_blib({split(' ',<STDIN>)},'].$autodir.q[')" <.MM_tmp]);
    push(@m,qq[
	$self->{NOECHO}Delete/NoLog/NoConfirm .MM_tmp;
	$self->{NOECHO}\$(TOUCH) pm_to_blib.ts
]);

    join('',@m);
}

=item tool_autosplit (override)

Use VMS-style quoting on command line.

=cut

sub tool_autosplit{
    my($self, %attribs) = @_;
    my($asl) = "";
    $asl = "\$AutoSplit::Maxlen=$attribs{MAXLEN};" if $attribs{MAXLEN};
    q{
# Usage: $(AUTOSPLITFILE) FileToSplit AutoDirToSplitInto
AUTOSPLITFILE = $(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e "use AutoSplit;}.$asl.q{ AutoSplit::autosplit($ARGV[0], $ARGV[1], 0, 1, 1) ;"
};
}

=item tool_sxubpp (override)

Use VMS-style quoting on xsubpp command line.

=cut

sub tool_xsubpp {
    my($self) = @_;
    return '' unless $self->needs_linking;
    my($xsdir) = $self->catdir($self->{PERL_LIB},'ExtUtils');
    # drop back to old location if xsubpp is not in new location yet
    $xsdir = $self->catdir($self->{PERL_SRC},'ext') unless (-f $self->catfile($xsdir,'xsubpp'));
    my(@tmdeps) = '$(XSUBPPDIR)typemap';
    if( $self->{TYPEMAPS} ){
	my $typemap;
	foreach $typemap (@{$self->{TYPEMAPS}}){
		if( ! -f  $typemap ){
			warn "Typemap $typemap not found.\n";
		}
		else{
			push(@tmdeps, $self->fixpath($typemap));
		}
	}
    }
    push(@tmdeps, "typemap") if -f "typemap";
    my(@tmargs) = map("-typemap $_", @tmdeps);
    if( exists $self->{XSOPT} ){
	unshift( @tmargs, $self->{XSOPT} );
    }

    my $xsubpp_version = $self->xsubpp_version($self->catfile($xsdir,'xsubpp'));

    # What are the correct thresholds for version 1 && 2 Paul?
    if ( $xsubpp_version > 1.923 ){
	$self->{XSPROTOARG} = '' unless defined $self->{XSPROTOARG};
    } else {
	if (defined $self->{XSPROTOARG} && $self->{XSPROTOARG} =~ /\-prototypes/) {
	    print STDOUT qq{Warning: This extension wants to pass the switch "-prototypes" to xsubpp.
	Your version of xsubpp is $xsubpp_version and cannot handle this.
	Please upgrade to a more recent version of xsubpp.
};
	} else {
	    $self->{XSPROTOARG} = "";
	}
    }

    "
XSUBPPDIR = $xsdir
XSUBPP = \$(PERL) \"-I\$(PERL_ARCHLIB)\" \"-I\$(PERL_LIB)\" \$(XSUBPPDIR)xsubpp
XSPROTOARG = $self->{XSPROTOARG}
XSUBPPDEPS = @tmdeps
XSUBPPARGS = @tmargs
";
}

=item xsubpp_version (override)

Test xsubpp exit status according to VMS rules ($sts & 1 ==> good)
rather than Unix rules ($sts == 0 ==> good).

=cut

sub xsubpp_version
{
    my($self,$xsubpp) = @_;
    my ($version) ;
    return '' unless $self->needs_linking;

    # try to figure out the version number of the xsubpp on the system

    # first try the -v flag, introduced in 1.921 & 2.000a2

    my $command = "$self->{PERL} \"-I$self->{PERL_LIB}\" $xsubpp -v";
    print "Running: $command\n" if $Verbose;
    $version = `$command` ;
    warn "Running '$command' exits with status " . $? unless ($? & 1);
    chop $version ;

    return $1 if $version =~ /^xsubpp version (.*)/ ;

    # nope, then try something else

    my $counter = '000';
    my ($file) = 'temp' ;
    $counter++ while -e "$file$counter"; # don't overwrite anything
    $file .= $counter;

    local(*F);
    open(F, ">$file") or die "Cannot open file '$file': $!\n" ;
    print F <<EOM ;
MODULE = fred PACKAGE = fred

int
fred(a)
	int	a;
EOM

    close F ;

    $command = "$self->{PERL} $xsubpp $file";
    print "Running: $command\n" if $Verbose;
    my $text = `$command` ;
    warn "Running '$command' exits with status " . $? unless ($? & 1);
    unlink $file ;

    # gets 1.2 -> 1.92 and 2.000a1
    return $1 if $text =~ /automatically by xsubpp version ([\S]+)\s*/  ;

    # it is either 1.0 or 1.1
    return 1.1 if $text =~ /^Warning: ignored semicolon/ ;

    # none of the above, so 1.0
    return "1.0" ;
}

=item tools_other (override)

Adds a few MM[SK] macros, and shortens some the installatin commands,
in order to stay under DCL's 255-character limit.  Also changes
EQUALIZE_TIMESTAMP to set revision date of target file to one second
later than source file, since MMK interprets precisely equal revision
dates for a source and target file as a sign that the target needs
to be updated.

=cut

sub tools_other {
    my($self) = @_;
    qq!
# Assumes \$(MMS) invokes MMS or MMK
# (It is assumed in some cases later that the default makefile name
# (Descrip.MMS for MM[SK]) is used.)
USEMAKEFILE = /Descrip=
USEMACROS = /Macro=(
MACROEND = )
MAKEFILE = Descrip.MMS
SHELL = Posix
TOUCH = $self->{TOUCH}
CHMOD = $self->{CHMOD}
CP = $self->{CP}
MV = $self->{MV}
RM_F  = $self->{RM_F}
RM_RF = $self->{RM_RF}
UMASK_NULL = $self->{UMASK_NULL}
NOOP = $self->{NOOP}
MKPATH = Create/Directory
EQUALIZE_TIMESTAMP = \$(PERL) -we "open F,qq{>\$ARGV[1]};close F;utime(0,(stat(\$ARGV[0]))[9]+1,\$ARGV[1])"
!. ($self->{PARENT} ? '' : 
qq!WARN_IF_OLD_PACKLIST = \$(PERL) -e "if (-f \$ARGV[0]){print qq[WARNING: Old package found (\$ARGV[0]); please check for collisions\\n]}"
MOD_INSTALL = \$(PERL) "-I\$(PERL_LIB)" "-MExtUtils::Install" -e "install({split(' ',<STDIN>)},1);"
DOC_INSTALL = \$(PERL) -e "\@ARGV=split('|',<STDIN>);print '=head3 ',scalar(localtime),': C<',shift,qq[>\\n\\n=over 4\\n\\n];while(\$key=shift && \$val=shift){print qq[=item *\\n\\nC<\$key: \$val>\\n\\n];}print qq[=back\\n\\n]"
UNINSTALL = \$(PERL) "-I\$(PERL_LIB)" "-MExtUtils::Install" -e "uninstall(\$ARGV[0],1);"
!);
}

=item dist (override)

Provide VMSish defaults for some values, then hand off to
default MM_Unix method.

=cut

sub dist {
    my($self, %attribs) = @_;
    $attribs{VERSION}      ||= $self->{VERSION_SYM};
    $attribs{ZIPFLAGS}     ||= '-Vu';
    $attribs{COMPRESS}     ||= 'gzip';
    $attribs{SUFFIX}       ||= '-gz';
    $attribs{SHAR}         ||= 'vms_share';
    $attribs{DIST_DEFAULT} ||= 'zipdist';

    return ExtUtils::MM_Unix::dist($self,%attribs);
}

=item c_o (override)

Use VMS syntax on command line.  In particular, $(DEFINE) and
$(PERL_INC) have been pulled into $(CCCMD).  Also use MM[SK] macros.

=cut

sub c_o {
    my($self) = @_;
    return '' unless $self->needs_linking();
    '
.c$(OBJ_EXT) :
	$(CCCMD) $(CCCDLFLAGS) $(MMS$TARGET_NAME).c

.cpp$(OBJ_EXT) :
	$(CCCMD) $(CCCDLFLAGS) $(MMS$TARGET_NAME).cpp

.cxx$(OBJ_EXT) :
	$(CCCMD) $(CCCDLFLAGS) $(MMS$TARGET_NAME).cxx

';
}

=item xs_c (override)

Use MM[SK] macros.

=cut

sub xs_c {
    my($self) = @_;
    return '' unless $self->needs_linking();
    '
.xs.c :
	$(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $(MMS$TARGET_NAME).xs >$(MMS$TARGET)
';
}

=item xs_o (override)

Use MM[SK] macros, and VMS command line for C compiler.

=cut

sub xs_o {	# many makes are too dumb to use xs_c then c_o
    my($self) = @_;
    return '' unless $self->needs_linking();
    '
.xs$(OBJ_EXT) :
	$(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $(MMS$TARGET_NAME).xs >$(MMS$TARGET_NAME).c
	$(CCCMD) $(CCCDLFLAGS) $(MMS$TARGET_NAME).c
';
}

=item top_targets (override)

Use VMS quoting on command line for Version_check.

=cut

sub top_targets {
    my($self) = shift;
    my(@m);
    push @m, '
all :: pure_all manifypods
	$(NOOP)

pure_all :: config pm_to_blib subdirs linkext
	$(NOOP)

subdirs :: $(MYEXTLIB)
	$(NOOP)

config :: $(MAKEFILE) $(INST_LIBDIR).exists
	$(NOOP)

config :: $(INST_ARCHAUTODIR).exists
	$(NOOP)

config :: $(INST_AUTODIR).exists
	$(NOOP)
';

    push @m, q{
config :: Version_check
	$(NOOP)

} unless $self->{PARENT} or ($self->{PERL_SRC} && $self->{INSTALLDIRS} eq "perl") or $self->{NO_VC};


    push @m, $self->dir_target(qw[$(INST_AUTODIR) $(INST_LIBDIR) $(INST_ARCHAUTODIR)]);
    if (%{$self->{MAN1PODS}}) {
	push @m, q[
config :: $(INST_MAN1DIR).exists
	$(NOOP)
];
	push @m, $self->dir_target(qw[$(INST_MAN1DIR)]);
    }
    if (%{$self->{MAN3PODS}}) {
	push @m, q[
config :: $(INST_MAN3DIR).exists
	$(NOOP)
];
	push @m, $self->dir_target(qw[$(INST_MAN3DIR)]);
    }

    push @m, '
$(O_FILES) : $(H_FILES)
' if @{$self->{O_FILES} || []} && @{$self->{H} || []};

    push @m, q{
help :
	perldoc ExtUtils::MakeMaker
};

    push @m, q{
Version_check :
	},$self->{NOECHO},q{$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -
	"-MExtUtils::MakeMaker=Version_check" -e "&Version_check('$(MM_VERSION)')"
};

    join('',@m);
}

=item dlsyms (override)

Create VMS linker options files specifying universal symbols for this
extension's shareable image, and listing other shareable images or 
libraries to which it should be linked.

=cut

sub dlsyms {
    my($self,%attribs) = @_;

    return '' unless $self->needs_linking();

    my($funcs) = $attribs{DL_FUNCS} || $self->{DL_FUNCS} || {};
    my($vars)  = $attribs{DL_VARS}  || $self->{DL_VARS}  || [];
    my($srcdir)= $attribs{PERL_SRC} || $self->{PERL_SRC} || '';
    my(@m);

    unless ($self->{SKIPHASH}{'dynamic'}) {
	push(@m,'
dynamic :: rtls.opt $(INST_ARCHAUTODIR)$(BASEEXT).opt
	$(NOOP)
');
	if ($srcdir) {
	   my($popt) = $self->catfile($srcdir,'perlshr.opt');
	   my($lopt) = $self->catfile($srcdir,'crtl.opt');
	   push(@m,"# Depend on $(BASEEXT).opt to insure we copy here *after* autogenerating (wrong) rtls.opt in Mksymlists
rtls.opt : $popt $lopt \$(BASEEXT).opt
	Copy/Log $popt Sys\$Disk:[]rtls.opt
	Append/Log $lopt Sys\$Disk:[]rtls.opt
");
	}
	else {
	    push(@m,'
# rtls.opt is built in the same step as $(BASEEXT).opt
rtls.opt : $(BASEEXT).opt
	$(TOUCH) $(MMS$TARGET)
');
	}
    }

    push(@m,'
static :: $(INST_ARCHAUTODIR)$(BASEEXT).opt
	$(NOOP)
') unless $self->{SKIPHASH}{'static'};

    push(@m,'
$(INST_ARCHAUTODIR)$(BASEEXT).opt : $(BASEEXT).opt
	$(CP) $(MMS$SOURCE) $(MMS$TARGET)

$(BASEEXT).opt : Makefile.PL
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e "use ExtUtils::Mksymlists;" -
	',qq[-e "Mksymlists('NAME' => '$self->{NAME}', 'DL_FUNCS' => ],
	neatvalue($funcs),q[, 'DL_VARS' => ],neatvalue($vars),')"
	$(PERL) -e "print ""$(INST_STATIC)/Include=$(BASEEXT)\n$(INST_STATIC)/Library\n"";" >>$(MMS$TARGET)
');

    join('',@m);
}

=item dynamic_lib (override)

Use VMS Link command.

=cut

sub dynamic_lib {
    my($self, %attribs) = @_;
    return '' unless $self->needs_linking(); #might be because of a subdir

    return '' unless $self->has_link_code();

    my($otherldflags) = $attribs{OTHERLDFLAGS} || "";
    my($inst_dynamic_dep) = $attribs{INST_DYNAMIC_DEP} || "";
    my(@m);
    push @m,"

OTHERLDFLAGS = $otherldflags
INST_DYNAMIC_DEP = $inst_dynamic_dep

";
    push @m, '
$(INST_DYNAMIC) : $(INST_STATIC) $(PERL_INC)perlshr_attr.opt rtls.opt $(INST_ARCHAUTODIR).exists $(EXPORT_LIST) $(PERL_ARCHIVE) $(INST_DYNAMIC_DEP)
	',$self->{NOECHO},'$(MKPATH) $(INST_ARCHAUTODIR)
	Link $(LDFLAGS) /Shareable=$(MMS$TARGET)$(OTHERLDFLAGS) $(BASEEXT).opt/Option,rtls.opt/Option,$(PERL_INC)perlshr_attr.opt/Option
';

    push @m, $self->dir_target('$(INST_ARCHAUTODIR)');
    join('',@m);
}

=item dynamic_bs (override)

Use VMS-style quoting on Mkbootstrap command line.

=cut

sub dynamic_bs {
    my($self, %attribs) = @_;
    return '
BOOTSTRAP =
' unless $self->has_link_code();
    '
BOOTSTRAP = '."$self->{BASEEXT}.bs".'

# As MakeMaker mkbootstrap might not write a file (if none is required)
# we use touch to prevent make continually trying to remake it.
# The DynaLoader only reads a non-empty file.
$(BOOTSTRAP) : $(MAKEFILE) '."$self->{BOOTDEP}".' $(INST_ARCHAUTODIR).exists
	'.$self->{NOECHO}.'Write Sys$Output "Running mkbootstrap for $(NAME) ($(BSLOADLIBS))"
	'.$self->{NOECHO}.'$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -
	-e "use ExtUtils::Mkbootstrap; Mkbootstrap(\'$(BASEEXT)\',\'$(BSLOADLIBS)\');"
	'.$self->{NOECHO}.' $(TOUCH) $(MMS$TARGET)

$(INST_BOOT) : $(BOOTSTRAP) $(INST_ARCHAUTODIR).exists
	'.$self->{NOECHO}.'$(RM_RF) $(INST_BOOT)
	- $(CP) $(BOOTSTRAP) $(INST_BOOT)
';
}

=item static_lib (override)

Use VMS commands to manipulate object library.

=cut

sub static_lib {
    my($self) = @_;
    return '' unless $self->needs_linking();

    return '
$(INST_STATIC) :
	$(NOOP)
' unless ($self->{OBJECT} or @{$self->{C} || []} or $self->{MYEXTLIB});

    my(@m);
    push @m,'
# Rely on suffix rule for update action
$(OBJECT) : $(INST_ARCHAUTODIR).exists

$(INST_STATIC) : $(OBJECT) $(MYEXTLIB)
';
    # If this extension has it's own library (eg SDBM_File)
    # then copy that to $(INST_STATIC) and add $(OBJECT) into it.
    push(@m, '	$(CP) $(MYEXTLIB) $(MMS$TARGET)',"\n") if $self->{MYEXTLIB};

    push(@m,'
	If F$Search("$(MMS$TARGET)").eqs."" Then Library/Object/Create $(MMS$TARGET)
	Library/Object/Replace $(MMS$TARGET) $(MMS$SOURCE_LIST)
	',$self->{NOECHO},'$(PERL) -e "open F,\'>>$(INST_ARCHAUTODIR)extralibs.ld\';print F qq[$(EXTRALIBS)\n];close F;"
');
    push @m, $self->dir_target('$(INST_ARCHAUTODIR)');
    join('',@m);
}


# sub installpm_x { # called by installpm perl file
#     my($self, $dist, $inst, $splitlib) = @_;
#     if ($inst =~ m!#!) {
# 	warn "Warning: MM[SK] would have problems processing this file: $inst, SKIPPED\n";
# 	return '';
#     }
#     $inst = $self->fixpath($inst);
#     $dist = $self->fixpath($dist);
#     my($instdir) = $inst =~ /([^\)]+\))[^\)]*$/ ? $1 : dirname($inst);
#     my(@m);
# 
#     push(@m, "
# $inst : $dist \$(MAKEFILE) ${instdir}.exists \$(INST_ARCHAUTODIR).exists
# ",'	',$self->{NOECHO},'$(RM_F) $(MMS$TARGET)
# 	',$self->{NOECHO},'$(CP) ',"$dist $inst",'
# 	$(CHMOD) 644 $(MMS$TARGET)
# ');
#     push(@m, '	$(AUTOSPLITFILE) $(MMS$TARGET) ',
#               $self->catdir($splitlib,'auto')."\n\n")
#         if ($splitlib and $inst =~ /\.pm$/);
#     push(@m,$self->dir_target($instdir));
# 
#     join('',@m);
# }

=item manifypods (override)

Use VMS-style quoting on command line, and VMS logical name
to specify fallback location at build time if we can't find pod2man.

=cut


sub manifypods {
    my($self, %attribs) = @_;
    return "\nmanifypods :\n\t\$(NOOP)\n" unless %{$self->{MAN3PODS}} or %{$self->{MAN1PODS}};
    my($dist);
    my($pod2man_exe);
    if (defined $self->{PERL_SRC}) {
	$pod2man_exe = $self->catfile($self->{PERL_SRC},'pod','pod2man');
    } else {
	$pod2man_exe = $self->catfile($Config{scriptdirexp},'pod2man');
    }
    if ($pod2man_exe = $self->perl_script($pod2man_exe)) { $found_pod2man = 1; }
    else {
	# No pod2man but some MAN3PODS to be installed
	print <<END;

Warning: I could not locate your pod2man program.  As a last choice,
         I will look for the file to which the logical name POD2MAN
         points when MMK is invoked.

END
        $pod2man_exe = "pod2man";
    }
    my(@m);
    push @m,
qq[POD2MAN_EXE = $pod2man_exe\n],
q[POD2MAN = $(PERL) -we "%m=@ARGV;for (keys %m){" -
-e "system(""MCR $^X $(POD2MAN_EXE) $_ >$m{$_}"");}"
];
    push @m, "\nmanifypods : ";
    push @m, join " ", keys %{$self->{MAN1PODS}}, keys %{$self->{MAN3PODS}};
    push(@m,"\n");
    if (%{$self->{MAN1PODS}} || %{$self->{MAN3PODS}}) {
	my($pod);
	foreach $pod (sort keys %{$self->{MAN1PODS}}) {
	    push @m, qq[\t\@- If F\$Search("\$(POD2MAN_EXE)").nes."" Then \$(POD2MAN) ];
	    push @m, "$pod $self->{MAN1PODS}{$pod}\n";
	}
	foreach $pod (sort keys %{$self->{MAN3PODS}}) {
	    push @m, qq[\t\@- If F\$Search("\$(POD2MAN_EXE)").nes."" Then \$(POD2MAN) ];
	    push @m, "$pod $self->{MAN3PODS}{$pod}\n";
	}
    }
    join('', @m);
}

=item processPL (override)

Use VMS-style quoting on command line.

=cut

sub processPL {
    my($self) = @_;
    return "" unless $self->{PL_FILES};
    my(@m, $plfile);
    foreach $plfile (sort keys %{$self->{PL_FILES}}) {
	push @m, "
all :: $self->{PL_FILES}->{$plfile}
	\$(NOOP)

$self->{PL_FILES}->{$plfile} :: $plfile
",'	$(PERL) "-I$(INST_ARCHLIB)" "-I$(INST_LIB)" "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" '," $plfile
";
    }
    join "", @m;
}

=item installbin (override)

Stay under DCL's 255 character command line limit once again by
splitting potentially long list of files across multiple lines
in C<realclean> target.

=cut

sub installbin {
    my($self) = @_;
    return '' unless $self->{EXE_FILES} && ref $self->{EXE_FILES} eq "ARRAY";
    return '' unless @{$self->{EXE_FILES}};
    my(@m, $from, $to, %fromto, @to, $line);
    for $from (@{$self->{EXE_FILES}}) {
	my($path) = '$(INST_SCRIPT)' . basename($from);
	local($_) = $path;  # backward compatibility
	$to = $self->libscan($path);
	print "libscan($from) => '$to'\n" if ($Verbose >=2);
	$fromto{$from}=$to;
    }
    @to   = values %fromto;
    push @m, "
EXE_FILES = @{$self->{EXE_FILES}}

all :: @to
	\$(NOOP)

realclean ::
";
    $line = '';  #avoid unitialized var warning
    foreach $to (@to) {
	if (length($line) + length($to) > 80) {
	    push @m, "\t\$(RM_F) $line\n";
	    $line = $to;
	}
	else { $line .= " $to"; }
    }
    push @m, "\t\$(RM_F) $line\n\n" if $line;

    while (($from,$to) = each %fromto) {
	last unless defined $from;
	my $todir;
	if ($to =~ m#[/>:\]]#) { $todir = dirname($to); }
	else                   { ($todir = $to) =~ s/[^\)]+$//; }
	$todir = $self->fixpath($todir,1);
	push @m, "
$to : $from \$(MAKEFILE) ${todir}.exists
	\$(CP) $from $to

", $self->dir_target($todir);
    }
    join "", @m;
}

=item subdir_x (override)

Use VMS commands to change default directory.

=cut

sub subdir_x {
    my($self, $subdir) = @_;
    my(@m,$key);
    $subdir = $self->fixpath($subdir,1);
    push @m, '

subdirs ::
	olddef = F$Environment("Default")
	Set Default ',$subdir,'
	- $(MMS) all $(USEMACROS)$(PASTHRU)$(MACROEND)
	Set Default \'olddef\'
';
    join('',@m);
}

=item clean (override)

Split potentially long list of files across multiple commands (in
order to stay under the magic command line limit).  Also use MM[SK]
commands for handling subdirectories.

=cut

sub clean {
    my($self, %attribs) = @_;
    my(@m,$dir);
    push @m, '
# Delete temporary files but do not touch installed files. We don\'t delete
# the Descrip.MMS here so that a later make realclean still has it to use.
clean ::
';
    foreach $dir (@{$self->{DIR}}) { # clean subdirectories first
	my($vmsdir) = $self->fixpath($dir,1);
	push( @m, '	If F$Search("'.$vmsdir.'$(MAKEFILE)") Then \\',"\n\t",
	      '$(PERL) -e "chdir ',"'$vmsdir'",'; print `$(MMS) clean`;"',"\n");
    }
    push @m, '	$(RM_F) *.Map *.Dmp *.Lis *.cpp *.$(DLEXT) *$(OBJ_EXT) *$(LIB_EXT) *.Opt $(BOOTSTRAP) $(BASEEXT).bso
';

    my(@otherfiles) = values %{$self->{XS}}; # .c files from *.xs files
    push(@otherfiles, $attribs{FILES}) if $attribs{FILES};
    push(@otherfiles, qw[ blib $(MAKE_APERL_FILE) extralibs.ld perlmain.c pm_to_blib.ts ]);
    push(@otherfiles,$self->catfile('$(INST_ARCHAUTODIR)','extralibs.all'));
    my($file,$line);
    $line = '';  #avoid unitialized var warning
    foreach $file (@otherfiles) {
	$file = $self->fixpath($file);
	if (length($line) + length($file) > 80) {
	    push @m, "\t\$(RM_RF) $line\n";
	    $line = "$file";
	}
	else { $line .= " $file"; }
    }
    push @m, "\t\$(RM_RF) $line\n" if line;
    push(@m, "	$attribs{POSTOP}\n") if $attribs{POSTOP};
    join('', @m);
}

=item realclean (override)

Guess what we're working around?  Also, use MM[SK] for subdirectories.

=cut

sub realclean {
    my($self, %attribs) = @_;
    my(@m);
    push(@m,'
# Delete temporary files (via clean) and also delete installed files
realclean :: clean
');
    foreach(@{$self->{DIR}}){
	my($vmsdir) = $self->fixpath($_,1);
	push(@m, '	If F$Search("'."$vmsdir".'$(MAKEFILE)").nes."" Then \\',"\n\t",
	      '$(PERL) -e "chdir ',"'$vmsdir'",'; print `$(MMS) realclean`;"',"\n");
    }
    push @m,'	$(RM_RF) $(INST_AUTODIR) $(INST_ARCHAUTODIR)
';
    # We can't expand several of the MMS macros here, since they don't have
    # corresponding %$self keys (i.e. they're defined in Descrip.MMS as a
    # combination of macros).  In order to stay below DCL's 255 char limit,
    # we put only 2 on a line.
    my($file,$line,$fcnt);
    my(@files) = qw{ $(MAKEFILE) $(MAKEFILE)_old };
    if ($self->has_link_code) {
	push(@files,qw{ $(INST_DYNAMIC) $(INST_STATIC) $(INST_BOOT) $(OBJECT) });
    }
    push(@files, values %{$self->{PM}});
    $line = '';  #avoid unitialized var warning
    foreach $file (@files) {
	$file = $self->fixpath($file);
	if (length($line) + length($file) > 80 || ++$fcnt >= 2) {
	    push @m, "\t\$(RM_F) $line\n";
	    $line = "$file";
	    $fcnt = 0;
	}
	else { $line .= " $file"; }
    }
    push @m, "\t\$(RM_F) $line\n" if $line;
    if ($attribs{FILES} && ref $attribs{FILES} eq 'ARRAY') {
	$line = '';
	foreach $file (@{$attribs{'FILES'}}) {
	    $file = $self->fixpath($file);
	    if (length($line) + length($file) > 80) {
		push @m, "\t\$(RM_RF) $line\n";
		$line = "$file";
	    }
	    else { $line .= " $file"; }
	}
	push @m, "\t\$(RM_RF) $line\n" if $line;
    }
    push(@m, "	$attribs{POSTOP}\n")                     if $attribs{POSTOP};
    join('', @m);
}

=item dist_basics (override)

Use VMS-style quoting on command line.

=cut

sub dist_basics {
    my($self) = @_;
'
distclean :: realclean distcheck
	$(NOOP)

distcheck :
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e "use ExtUtils::Manifest \'&fullcheck\'; fullcheck()"

skipcheck :
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e "use ExtUtils::Manifest \'&fullcheck\'; skipcheck()"

manifest :
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e "use ExtUtils::Manifest \'&mkmanifest\'; mkmanifest()"
';
}

=item dist_core (override)

Syntax for invoking F<VMS_Share> differs from that for Unix F<shar>,
so C<shdist> target actions are VMS-specific.

=cut

sub dist_core {
    my($self) = @_;
q[
dist : $(DIST_DEFAULT)
	].$self->{NOECHO}.q[$(PERL) -le "print 'Warning: $m older than $vf' if -e ($vf = '$(VERSION_FROM)') && -M $vf < -M ($m = '$(MAKEFILE)'"

zipdist : $(DISTVNAME).zip
	$(NOOP)

$(DISTVNAME).zip : distdir
	$(PREOP)
	$(ZIP) "$(ZIPFLAGS)" $(MMS$TARGET) $(SRC)
	$(RM_RF) $(DISTVNAME)
	$(POSTOP)

$(DISTVNAME).tar$(SUFFIX) : distdir
	$(PREOP)
	$(TO_UNIX)
	$(TAR) "$(TARFLAGS)" $(DISTVNAME).tar $(SRC)
	$(RM_RF) $(DISTVNAME)
	$(COMPRESS) $(DISTVNAME).tar
	$(POSTOP)

shdist : distdir
	$(PREOP)
	$(SHARE) $(SRC) $(DISTVNAME).share
	$(RM_RF) $(DISTVNAME)
	$(POSTOP)
];
}

=item dist_dir (override)

Use VMS-style quoting on command line.

=cut

sub dist_dir {
    my($self) = @_;
q{
distdir :
	$(RM_RF) $(DISTVNAME)
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" -e "use ExtUtils::Manifest '/mani/';" \\
	-e "manicopy(maniread(),'$(DISTVNAME)','$(DIST_CP)');"
};
}

=item dist_test (override)

Use VMS commands to change default directory, and use VMS-style
quoting on command line.

=cut

sub dist_test {
    my($self) = @_;
q{
disttest : distdir
	startdir = F$Environment("Default")
	Set Default [.$(DISTVNAME)]
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" Makefile.PL
	$(MMS)
	$(MMS) test
	Set Default 'startdir'
};
}

# --- Test and Installation Sections ---

=item install (override)

Work around DCL's 255 character limit several times,and use
VMS-style command line quoting in a few cases.

=cut

sub install {
    my($self, %attribs) = @_;
    my(@m,@docfiles);

    if ($self->{EXE_FILES}) {
	my($line,$file) = ('','');
	foreach $file (@{$self->{EXE_FILES}}) {
	    $line .= "$file ";
	    if (length($line) > 128) {
		push(@docfiles,qq[\t\$(PERL) -e "print $line" >>.MM_tmp\n]);
		$line = '';
	    }
	}
	push(@docfiles,qq[\t\$(PERL) -e "print $line" >>.MM_tmp\n]) if $line;
    }

    push @m, q[
install :: all pure_install doc_install
	$(NOOP)

install_perl :: all pure_perl_install doc_perl_install
	$(NOOP)

install_site :: all pure_site_install doc_site_install
	$(NOOP)

install_ :: install_site
	],$self->{NOECHO},q[Write Sys$Output "INSTALLDIRS not defined, defaulting to INSTALLDIRS=site"

pure_install :: pure_$(INSTALLDIRS)_install
	$(NOOP)

doc_install :: doc_$(INSTALLDIRS)_install
	],$self->{NOECHO},q[Write Sys$Output "Appending installation info to $(INST_ARCHLIB)perllocal.pod"

pure__install : pure_site_install
	],$self->{NOECHO},q[Write Sys$Output "INSTALLDIRS not defined, defaulting to INSTALLDIRS=site"

doc__install : doc_site_install
	],$self->{NOECHO},q[Write Sys$Output "INSTALLDIRS not defined, defaulting to INSTALLDIRS=site"

# This hack brought to you by DCL's 255-character command line limit
pure_perl_install ::
	].$self->{NOECHO}.q[$(PERL) -e "print 'read ].$self->catfile('$(PERL_ARCHLIB)','auto','$(FULLEXT)','.packlist').q[ '" >.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print 'write ].$self->catfile('$(INSTALLARCHLIB)','auto','$(FULLEXT)','.packlist').q[ '" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print '$(INST_LIB) $(INSTALLPRIVLIB) '" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print '$(INST_ARCHLIB) $(INSTALLARCHLIB) '" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print '$(INST_BIN) $(INSTALLBIN) '" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print '$(INST_SCRIPT) $(INSTALLSCRIPT) '" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print '$(INST_MAN1DIR) $(INSTALLMAN1DIR) '" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print '$(INST_MAN3DIR) $(INSTALLMAN3DIR) '" >>.MM_tmp
	$(MOD_INSTALL) <.MM_tmp
	].$self->{NOECHO}.q[Delete/NoLog/NoConfirm .MM_tmp;
	].$self->{NOECHO}.q[$(WARN_IF_OLD_PACKLIST) ].$self->catfile('$(SITEARCHEXP)','auto','$(FULLEXT)','.packlist').q[

# Likewise
pure_site_install ::
	].$self->{NOECHO}.q[$(PERL) -e "print 'read ].$self->catfile('$(SITEARCHEXP)','auto','$(FULLEXT)','.packlist').q[ '" >.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print 'write ].$self->catfile('$(INSTALLSITEARCH)','auto','$(FULLEXT)','.packlist').q[ '" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print '$(INST_LIB) $(INSTALLSITELIB) '" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print '$(INST_ARCHLIB) $(INSTALLSITEARCH) '" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print '$(INST_BIN) $(INSTALLBIN) '" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print '$(INST_SCRIPT) $(INSTALLSCRIPT) '" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print '$(INST_MAN1DIR) $(INSTALLMAN1DIR) '" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print '$(INST_MAN3DIR) $(INSTALLMAN3DIR) '" >>.MM_tmp
	$(MOD_INSTALL) <.MM_tmp
	].$self->{NOECHO}.q[Delete/NoLog/NoConfirm .MM_tmp;
	].$self->{NOECHO}.q[$(WARN_IF_OLD_PACKLIST) ].$self->catfile('$(PERL_ARCHLIB)','auto','$(FULLEXT)','.packlist').q[

# Ditto
doc_perl_install ::
	].$self->{NOECHO}.q[$(PERL) -e "print 'Module $(NAME)|installed into|$(INSTALLPRIVLIB)|'" >.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print 'LINKTYPE|$(LINKTYPE)|VERSION|$(VERSION)|'" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print 'LINKTYPE|$(LINKTYPE)|VERSION|$(VERSION)|EXE_FILES|'" >>.MM_tmp
],@docfiles,q[	$(DOC_INSTALL) <.MM_tmp >>].$self->catfile('$(INSTALLARCHLIB)','perllocal.pod').q[
	].$self->{NOECHO}.q[Delete/NoLog/NoConfirm .MM_tmp;

# And again
doc_site_install ::
	].$self->{NOECHO}.q[$(PERL) -e "print 'Module $(NAME)|installed into|$(INSTALLSITELIB)|'" >.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print 'LINKTYPE|$(LINKTYPE)|VERSION|$(VERSION)|'" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print 'LINKTYPE|$(LINKTYPE)|VERSION|$(VERSION)|EXE_FILES|'" >>.MM_tmp
],@docfiles,q[	$(DOC_INSTALL) <.MM_tmp >>].$self->catfile('$(INSTALLARCHLIB)','perllocal.pod').q[
	].$self->{NOECHO}.q[Delete/NoLog/NoConfirm .MM_tmp;

];

    push @m, q[
uninstall :: uninstall_from_$(INSTALLDIRS)dirs
	$(NOOP)

uninstall_from_perldirs ::
	].$self->{NOECHO}.q[$(UNINSTALL) ].$self->catfile('$(PERL_ARCHLIB)','auto','$(FULLEXT)','.packlist').q[

uninstall_from_sitedirs ::
	].$self->{NOECHO}.q[$(UNINSTALL) ].$self->catfile('$(SITEARCHEXP)','auto','$(FULLEXT)','.packlist')."\n";

    join('',@m);
}

=item perldepend (override)

Use VMS-style syntax for files; it's cheaper to just do it directly here
than to have the MM_Unix method call C<catfile> repeatedly.  Also use
config.vms as source of original config data if the Perl distribution
is available; config.sh is an ancillary file under VMS.  Finally, if
we have to rebuild Config.pm, use MM[SK] to do it.

=cut

sub perldepend {
    my($self) = @_;
    my(@m);

    push @m, '
$(OBJECT) : $(PERL_INC)EXTERN.h, $(PERL_INC)INTERN.h, $(PERL_INC)XSUB.h, $(PERL_INC)av.h
$(OBJECT) : $(PERL_INC)cop.h, $(PERL_INC)cv.h, $(PERL_INC)embed.h, $(PERL_INC)form.h
$(OBJECT) : $(PERL_INC)gv.h, $(PERL_INC)handy.h, $(PERL_INC)hv.h, $(PERL_INC)keywords.h
$(OBJECT) : $(PERL_INC)mg.h, $(PERL_INC)op.h, $(PERL_INC)opcode.h, $(PERL_INC)patchlevel.h
$(OBJECT) : $(PERL_INC)perl.h, $(PERL_INC)perly.h, $(PERL_INC)pp.h, $(PERL_INC)proto.h
$(OBJECT) : $(PERL_INC)regcomp.h, $(PERL_INC)regexp.h, $(PERL_INC)scope.h, $(PERL_INC)sv.h
$(OBJECT) : $(PERL_INC)vmsish.h, $(PERL_INC)util.h, $(PERL_INC)config.h

' if $self->{OBJECT}; 

    if ($self->{PERL_SRC}) {
	my(@macros);
	my($mmsquals) = '$(USEMAKEFILE)[.vms]$(MAKEFILE)';
	push(@macros,'__AXP__=1') if $Config{'arch'} eq 'VMS_AXP';
	push(@macros,'DECC=1')    if $Config{'vms_cc_type'} eq 'decc';
	push(@macros,'GNUC=1')    if $Config{'vms_cc_type'} eq 'gcc';
	push(@macros,'SOCKET=1')  if $Config{'d_has_sockets'};
	push(@macros,qq["CC=$Config{'cc'}"])  if $Config{'cc'} =~ m!/!;
	$mmsquals .= '$(USEMACROS)' . join(',',@macros) . '$(MACROEND)' if @macros;
	push(@m,q[
# Check for unpropagated config.sh changes. Should never happen.
# We do NOT just update config.h because that is not sufficient.
# An out of date config.h is not fatal but complains loudly!
#$(PERL_INC)config.h : $(PERL_SRC)config.sh
$(PERL_INC)config.h : $(PERL_VMS)config.vms
	],$self->{NOECHO},q[Write Sys$Error "Warning: $(PERL_INC)config.h out of date with $(PERL_VMS)config.vms"

#$(PERL_ARCHLIB)Config.pm : $(PERL_SRC)config.sh
$(PERL_ARCHLIB)Config.pm : $(PERL_VMS)config.vms $(PERL_VMS)genconfig.pl
	],$self->{NOECHO},q[Write Sys$Error "$(PERL_ARCHLIB)Config.pm may be out of date with config.vms or genconfig.pl"
	olddef = F$Environment("Default")
	Set Default $(PERL_SRC)
	$(MMS)],$mmsquals,q[ $(MMS$TARGET)
	Set Default 'olddef'
]);
    }

    push(@m, join(" ", map($self->fixpath($_),values %{$self->{XS}}))." : \$(XSUBPPDEPS)\n")
      if %{$self->{XS}};

    join('',@m);
}

=item makefile (override)

Use VMS commands and quoting.

=cut

sub makefile {
    my($self) = @_;
    my(@m,@cmd);
    # We do not know what target was originally specified so we
    # must force a manual rerun to be sure. But as it should only
    # happen very rarely it is not a significant problem.
    push @m, q[
$(OBJECT) : $(FIRST_MAKEFILE)
] if $self->{OBJECT};

    push @m,q[
# We take a very conservative approach here, but it\'s worth it.
# We move $(MAKEFILE) to $(MAKEFILE)_old here to avoid gnu make looping.
$(MAKEFILE) : Makefile.PL $(CONFIGDEP)
	],$self->{NOECHO},q[Write Sys$Output "$(MAKEFILE) out-of-date with respect to $(MMS$SOURCE_LIST)"
	],$self->{NOECHO},q[Write Sys$Output "Cleaning current config before rebuilding $(MAKEFILE) ..."
	- $(MV) $(MAKEFILE) $(MAKEFILE)_old
	- $(MMS) $(USEMAKEFILE)$(MAKEFILE)_old clean
	$(PERL) "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" Makefile.PL ],join(' ',map(qq["$_"],@ARGV)),q[
	],$self->{NOECHO},q[Write Sys$Output "$(MAKEFILE) has been rebuilt."
	],$self->{NOECHO},q[Write Sys$Output "Please run $(MMS) to build the extension."
];

    join('',@m);
}

=item test (override)

Use VMS commands for handling subdirectories.

=cut

sub test {
    my($self, %attribs) = @_;
    my($tests) = $attribs{TESTS} || ( -d 't' ? 't/*.t' : '');
    my(@m);
    push @m,"
TEST_VERBOSE = 0
TEST_TYPE = test_\$(LINKTYPE)
TEST_FILE = test.pl
TESTDB_SW = -d

test :: \$(TEST_TYPE)
	\$(NOOP)

testdb :: testdb_\$(LINKTYPE)
	\$(NOOP)

";
    foreach(@{$self->{DIR}}){
      my($vmsdir) = $self->fixpath($_,1);
      push(@m, '	If F$Search("',$vmsdir,'$(MAKEFILE)").nes."" Then $(PERL) -e "chdir ',"'$vmsdir'",
           '; print `$(MMS) $(PASTHRU2) test`'."\n");
    }
    push(@m, "\t$self->{NOECHO}Write Sys\$Output \"No tests defined for \$(NAME) extension.\"\n")
        unless $tests or -f "test.pl" or @{$self->{DIR}};
    push(@m, "\n");

    push(@m, "test_dynamic :: pure_all\n");
    push(@m, $self->test_via_harness('$(FULLPERL)', $tests)) if $tests;
    push(@m, $self->test_via_script('$(FULLPERL)', 'test.pl')) if -f "test.pl";
    push(@m, "    \$(NOOP)\n") if (!$tests && ! -f "test.pl");
    push(@m, "\n");

    push(@m, "testdb_dynamic :: pure_all\n");
    push(@m, $self->test_via_script('$(FULLPERL) "$(TESTDB_SW)"', '$(TEST_FILE)'));
    push(@m, "\n");

    # Occasionally we may face this degenerate target:
    push @m, "test_ : test_dynamic\n\n";
 
    if ($self->needs_linking()) {
	push(@m, "test_static :: pure_all \$(MAP_TARGET)\n");
	push(@m, $self->test_via_harness('$(MAP_TARGET)', $tests)) if $tests;
	push(@m, $self->test_via_script('$(MAP_TARGET)', 'test.pl')) if -f 'test.pl';
	push(@m, "\n");
	push(@m, "testdb_static :: pure_all \$(MAP_TARGET)\n");
	push(@m, $self->test_via_script('$(MAP_TARGET) $(TESTDB_SW)', '$(TEST_FILE)'));
	push(@m, "\n");
    }
    else {
	push @m, "test_static :: test_dynamic\n\t$self->{NOECHO}\$(NOOP)\n\n";
	push @m, "testdb_static :: testdb_dynamic\n\t$self->{NOECHO}\$(NOOP)\n";
    }

    join('',@m);
}

=item test_via_harness (override)

Use VMS-style quoting on command line.

=cut

sub test_via_harness {
    my($self,$perl,$tests) = @_;
    "	$perl".' "-I$(INST_ARCHLIB)" "-I$(INST_LIB)" "-I$(PERL_LIB)" "-I$(PERL_ARCHLIB)" \\'."\n\t".
    '-e "use Test::Harness qw(&runtests $verbose); $verbose=$(TEST_VERBOSE); runtests @ARGV;" \\'."\n\t$tests\n";
}

=item test_via_script (override)

Use VMS-style quoting on command line.

=cut

sub test_via_script {
    my($self,$perl,$script) = @_;
    "	$perl".' "-I$(INST_ARCHLIB)" "-I$(INST_LIB)" "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" '.$script.'
';
}

=item makeaperl (override)

Undertake to build a new set of Perl images using VMS commands.  Since
VMS does dynamic loading, it's not necessary to statically link each
extension into the Perl image, so this isn't the normal build path.
Consequently, it hasn't really been tested, and may well be incomplete.

=cut

sub makeaperl {
    my($self, %attribs) = @_;
    my($makefilename, $searchdirs, $static, $extra, $perlinc, $target, $tmp, $libperl) = 
      @attribs{qw(MAKE DIRS STAT EXTRA INCL TARGET TMP LIBPERL)};
    my(@m);
    push @m, "
# --- MakeMaker makeaperl section ---
MAP_TARGET    = $target
";
    return join '', @m if $self->{PARENT};

    my($dir) = join ":", @{$self->{DIR}};

    unless ($self->{MAKEAPERL}) {
	push @m, q{
$(MAKE_APERL_FILE) : $(FIRST_MAKEFILE)
	},$self->{NOECHO},q{Write Sys$Output "Writing ""$(MMS$TARGET)"" for this $(MAP_TARGET)"
	},$self->{NOECHO},q{$(PERL) "-I$(INST_ARCHLIB)" "-I$(INST_LIB)" "-I$(PERL_ARCHLIB)" "-I$(PERL_LIB)" \
		Makefile.PL DIR=}, $dir, q{ \
		MAKEFILE=$(MAKE_APERL_FILE) LINKTYPE=static \
		MAKEAPERL=1 NORECURS=1

$(MAP_TARGET) :: $(MAKE_APERL_FILE)
	$(MMS)$(USEMAKEFILE)$(MAKE_APERL_FILE) static $(MMS$TARGET)
};
	push @m, map( " \\\n\t\t$_", @ARGV );
	push @m, "\n";

	return join '', @m;
    }


    my($linkcmd,@staticopts,@staticpkgs,$extralist,$target,$targdir,$libperldir);

    # The front matter of the linkcommand...
    $linkcmd = join ' ', $Config{'ld'},
	    grep($_, @Config{qw(large split ldflags ccdlflags)});
    $linkcmd =~ s/\s+/ /g;

    # Which *.olb files could we make use of...
    local(%olbs);
    $olbs{$self->{INST_ARCHAUTODIR}} = "$self->{BASEEXT}\$(LIB_EXT)";
    require File::Find;
    File::Find::find(sub {
	return unless m/\Q$self->{LIB_EXT}\E$/;
	return if m/^libperl/;

	if( exists $self->{INCLUDE_EXT} ){
		my $found = 0;
		my $incl;
		my $xx;

		($xx = $File::Find::name) =~ s,.*?/auto/,,;
		$xx =~ s,/?$_,,;
		$xx =~ s,/,::,g;

		# Throw away anything not explicitly marked for inclusion.
		# DynaLoader is implied.
		foreach $incl ((@{$self->{INCLUDE_EXT}},'DynaLoader')){
			if( $xx eq $incl ){
				$found++;
				last;
			}
		}
		return unless $found;
	}
	elsif( exists $self->{EXCLUDE_EXT} ){
		my $excl;
		my $xx;

		($xx = $File::Find::name) =~ s,.*?/auto/,,;
		$xx =~ s,/?$_,,;
		$xx =~ s,/,::,g;

		# Throw away anything explicitly marked for exclusion
		foreach $excl (@{$self->{EXCLUDE_EXT}}){
			return if( $xx eq $excl );
		}
	}

	$olbs{$ENV{DEFAULT}} = $_;
    }, grep( -d $_, @{$searchdirs || []}));

    # We trust that what has been handed in as argument will be buildable
    $static = [] unless $static;
    @olbs{@{$static}} = (1) x @{$static};
 
    $extra = [] unless $extra && ref $extra eq 'ARRAY';
    # Sort the object libraries in inverse order of
    # filespec length to try to insure that dependent extensions
    # will appear before their parents, so the linker will
    # search the parent library to resolve references.
    # (e.g. Intuit::DWIM will precede Intuit, so unresolved
    # references from [.intuit.dwim]dwim.obj can be found
    # in [.intuit]intuit.olb).
    for (sort keys %olbs) {
	next unless $olbs{$_} =~ /\Q$self->{LIB_EXT}\E$/;
	my($dir) = $self->fixpath($_,1);
	my($extralibs) = $dir . "extralibs.ld";
	my($extopt) = $dir . $olbs{$_};
	$extopt =~ s/$self->{LIB_EXT}$/.opt/;
	if (-f $extralibs ) {
	    open LIST,$extralibs or warn $!,next;
	    push @$extra, <LIST>;
	    close LIST;
	}
	if (-f $extopt) {
	    open OPT,$extopt or die $!;
	    while (<OPT>) {
		next unless /(?:UNIVERSAL|VECTOR)=boot_([\w_]+)/;
		# ExtUtils::Miniperl expects Unix paths
		(my($pkg) = "$1_$1$self->{LIB_EXT}") =~ s#_*#/#g;
		push @staticpkgs,$pkg;
	    }
	    push @staticopts, $extopt;
	}
    }

    $target = "Perl.Exe" unless $target;
    ($shrtarget,$targdir) = fileparse($target);
    $shrtarget =~ s/^([^.]*)/$1Shr/;
    $shrtarget = $targdir . $shrtarget;
    $target = "Perlshr.$Config{'dlext'}" unless $target;
    $tmp = "[]" unless $tmp;
    $tmp = $self->fixpath($tmp,1);
    if (@$extra) {
	$extralist = join(' ',@$extra);
	$extralist =~ s/[,\s\n]+/, /g;
    }
    else { $extralist = ''; }
    if ($libperl) {
	unless (-f $libperl || -f ($libperl = $self->catfile($Config{'installarchlib'},'CORE',$libperl))) {
	    print STDOUT "Warning: $libperl not found\n";
	    undef $libperl;
	}
    }
    unless ($libperl) {
	if (defined $self->{PERL_SRC}) {
	    $libperl = $self->catfile($self->{PERL_SRC},"libperl$self->{LIB_EXT}");
	} elsif (-f ($libperl = $self->catfile($Config{'installarchlib'},'CORE',"libperl$self->{LIB_EXT}")) ) {
	} else {
	    print STDOUT "Warning: $libperl not found
    If you're going to build a static perl binary, make sure perl is installed
    otherwise ignore this warning\n";
	}
    }
    $libperldir = $self->fixpath((fileparse($libperl))[1],1);

    push @m, '
# Fill in the target you want to produce if it\'s not perl
MAP_TARGET    = ',$self->fixpath($target),'
MAP_SHRTARGET = ',$self->fixpath($shrtarget),"
MAP_LINKCMD   = $linkcmd
MAP_PERLINC   = ", $perlinc ? map('"$_" ',@{$perlinc}) : '','
# We use the linker options files created with each extension, rather than
#specifying the object files directly on the command line.
MAP_STATIC    = ',@staticopts ? join(' ', @staticopts) : '','
MAP_OPTS    = ',@staticopts ? ','.join(',', map($_.'/Option', @staticopts)) : '',"
MAP_EXTRA     = $extralist
MAP_LIBPERL = ",$self->fixpath($libperl),'
';


    push @m,'
$(MAP_SHRTARGET) : $(MAP_LIBPERL) $(MAP_STATIC) ',"${libperldir}Perlshr_Attr.Opt",'
	$(MAP_LINKCMD)/Shareable=$(MMS$TARGET) $(MAP_OPTS), $(MAP_EXTRA), $(MAP_LIBPERL) ',"${libperldir}Perlshr_Attr.Opt",'
$(MAP_TARGET) : $(MAP_SHRTARGET) ',"${tmp}perlmain\$(OBJ_EXT) ${tmp}PerlShr.Opt",'
	$(MAP_LINKCMD) ',"${tmp}perlmain\$(OBJ_EXT)",', PerlShr.Opt/Option
	',$self->{NOECHO},'Write Sys$Output "To install the new ""$(MAP_TARGET)"" binary, say"
	',$self->{NOECHO},'Write Sys$Output "    $(MMS)$(USEMAKEFILE)$(MAKEFILE) inst_perl $(USEMACROS)MAP_TARGET=$(MAP_TARGET)$(ENDMACRO)"
	',$self->{NOECHO},'Write Sys$Output "To remove the intermediate files, say
	',$self->{NOECHO},'Write Sys$Output "    $(MMS)$(USEMAKEFILE)$(MAKEFILE) map_clean"
';
    push @m,'
',"${tmp}perlmain.c",' : $(MAKEFILE)
	',$self->{NOECHO},'$(PERL) $(MAP_PERLINC) -e "use ExtUtils::Miniperl; writemain(qw|',@staticpkgs,'|)" >$(MMS$TARGET)
';

    push @m, q[
# More from the 255-char line length limit
doc_inst_perl :
	].$self->{NOECHO}.q[$(PERL) -e "print 'Perl binary $(MAP_TARGET)|'" >.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print 'MAP_STATIC|$(MAP_STATIC)|'" >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -pl040 -e " " ].$self->catfile('$(INST_ARCHAUTODIR)','extralibs.all'),q[ >>.MM_tmp
	].$self->{NOECHO}.q[$(PERL) -e "print 'MAP_LIBPERL|$(MAP_LIBPERL)|'" >>.MM_tmp
	$(DOC_INSTALL) <.MM_tmp >>].$self->catfile('$(INSTALLARCHLIB)','perllocal.pod').q[
	].$self->{NOECHO}.q[Delete/NoLog/NoConfirm .MM_tmp;
];

    push @m, "
inst_perl : pure_inst_perl doc_inst_perl
	\$(NOOP)

pure_inst_perl : \$(MAP_TARGET)
	$self->{CP} \$(MAP_SHRTARGET) ",$self->fixpath($Config{'installbin'},1),"
	$self->{CP} \$(MAP_TARGET) ",$self->fixpath($Config{'installbin'},1),"

clean :: map_clean
	\$(NOOP)

map_clean :
	\$(RM_F) ${tmp}perlmain\$(OBJ_EXT) ${tmp}perlmain.c \$(MAKEFILE)
	\$(RM_F) ${tmp}PerlShr.Opt \$(MAP_TARGET)
";

    join '', @m;
}
  
=item ext (specific)

Stub routine standing in for C<ExtUtils::LibList::ext> until VMS
support is added to that package.

=cut

sub ext {
    my($self) = @_;
    '','','';
}

# --- Output postprocessing section ---

=item nicetext (override)

Insure that colons marking targets are preceded by space, in order
to distinguish the target delimiter from a colon appearing as
part of a filespec.

=cut

sub nicetext {

    my($self,$text) = @_;
    $text =~ s/([^\s:])(:+\s)/$1 $2/gs;
    $text;
}

1;

__END__

