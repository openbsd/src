package ExtUtils::MM_Unix;

require 5.005_03;  # Maybe further back, dunno

use strict;

use Exporter ();
use Carp;
use Config;
use File::Basename qw(basename dirname fileparse);
use File::Spec;
use DirHandle;
use strict;
use vars qw($VERSION @ISA
            $Is_Mac $Is_OS2 $Is_VMS $Is_Win32 $Is_Dos $Is_VOS
            $Verbose %pm %static $Xsubpp_Version
            %Config_Override
           );

use ExtUtils::MakeMaker qw($Verbose neatvalue);

$VERSION = '1.33';

require ExtUtils::MM_Any;
@ISA = qw(ExtUtils::MM_Any);

$Is_OS2   = $^O eq 'os2';
$Is_Mac   = $^O eq 'MacOS';
$Is_Win32 = $^O eq 'MSWin32' || $Config{osname} eq 'NetWare';
$Is_Dos   = $^O eq 'dos';
$Is_VOS   = $^O eq 'vos';
$Is_VMS   = $^O eq 'VMS';

=head1 NAME

ExtUtils::MM_Unix - methods used by ExtUtils::MakeMaker

=head1 SYNOPSIS

C<require ExtUtils::MM_Unix;>

=head1 DESCRIPTION

The methods provided by this package are designed to be used in
conjunction with ExtUtils::MakeMaker. When MakeMaker writes a
Makefile, it creates one or more objects that inherit their methods
from a package C<MM>. MM itself doesn't provide any methods, but it
ISA ExtUtils::MM_Unix class. The inheritance tree of MM lets operating
specific packages take the responsibility for all the methods provided
by MM_Unix. We are trying to reduce the number of the necessary
overrides by defining rather primitive operations within
ExtUtils::MM_Unix.

If you are going to write a platform specific MM package, please try
to limit the necessary overrides to primitive methods, and if it is not
possible to do so, let's work out how to achieve that gain.

If you are overriding any of these methods in your Makefile.PL (in the
MY class), please report that to the makemaker mailing list. We are
trying to minimize the necessary method overrides and switch to data
driven Makefile.PLs wherever possible. In the long run less methods
will be overridable via the MY class.

=head1 METHODS

The following description of methods is still under
development. Please refer to the code for not suitably documented
sections and complain loudly to the makemaker mailing list.

Not all of the methods below are overridable in a
Makefile.PL. Overridable methods are marked as (o). All methods are
overridable by a platform specific MM_*.pm file (See
L<ExtUtils::MM_VMS>) and L<ExtUtils::MM_OS2>).

=cut

# So we don't have to keep calling the methods over and over again,
# we have these globals to cache the values.  They have to be global
# else the SelfLoaded methods can't see them.
use vars qw($Curdir $Rootdir $Updir);
$Curdir  = File::Spec->curdir;
$Rootdir = File::Spec->rootdir;
$Updir   = File::Spec->updir;

sub c_o;
sub clean;
sub const_cccmd;
sub const_config;
sub const_loadlibs;
sub constants;
sub depend;
sub dir_target;
sub dist;
sub dist_basics;
sub dist_ci;
sub dist_core;
sub dist_dir;
sub dist_test;
sub dlsyms;
sub dynamic;
sub dynamic_bs;
sub dynamic_lib;
sub exescan;
sub export_list;
sub extliblist;
sub find_perl;
sub fixin;
sub force;
sub guess_name;
sub has_link_code;
sub init_dirscan;
sub init_main;
sub init_others;
sub install;
sub installbin;
sub libscan;
sub linkext;
sub lsdir;
sub macro;
sub makeaperl;
sub makefile;
sub manifypods;
sub maybe_command;
sub maybe_command_in_dirs;
sub needs_linking;
sub nicetext;
sub parse_abstract;
sub parse_version;
sub pasthru;
sub perl_archive;
sub perl_archive_after;
sub perl_script;
sub perldepend;
sub pm_to_blib;
sub ppd;
sub post_constants;
sub post_initialize;
sub postamble;
sub prefixify;
sub processPL;
sub quote_paren;
sub realclean;
sub replace_manpage_separator;
sub static;
sub static_lib;
sub staticmake;
sub subdir_x;
sub subdirs;
sub test;
sub test_via_harness;
sub test_via_script;
sub tool_autosplit;
sub tool_xsubpp;
sub tools_other;
sub top_targets;
sub writedoc;
sub xs_c;
sub xs_cpp;
sub xs_o;
sub xsubpp_version;

#use SelfLoader;

# SelfLoader not smart enough to avoid autoloading DESTROY
sub DESTROY { }

#1;

#__DATA__

=head2 SelfLoaded methods

=over 2

=item c_o (o)

Defines the suffix rules to compile different flavors of C files to
object files.

=cut

sub c_o {
# --- Translation Sections ---

    my($self) = shift;
    return '' unless $self->needs_linking();
    my(@m);
    if (my $cpp = $Config{cpprun}) {
        my $cpp_cmd = $self->const_cccmd;
        $cpp_cmd =~ s/^CCCMD\s*=\s*\$\(CC\)/$cpp/;
        push @m, '
.c.i:
	'. $cpp_cmd . ' $(CCCDLFLAGS) "-I$(PERL_INC)" $(PASTHRU_DEFINE) $(DEFINE) $*.c > $*.i
';
    }
    push @m, '
.c.s:
	$(CCCMD) -S $(CCCDLFLAGS) "-I$(PERL_INC)" $(PASTHRU_DEFINE) $(DEFINE) $*.c
';
    push @m, '
.c$(OBJ_EXT):
	$(CCCMD) $(CCCDLFLAGS) "-I$(PERL_INC)" $(PASTHRU_DEFINE) $(DEFINE) $*.c
';
    push @m, '
.C$(OBJ_EXT):
	$(CCCMD) $(CCCDLFLAGS) "-I$(PERL_INC)" $(PASTHRU_DEFINE) $(DEFINE) $*.C
' if !$Is_OS2 and !$Is_Win32 and !$Is_Dos; #Case-specific
    push @m, '
.cpp$(OBJ_EXT):
	$(CCCMD) $(CCCDLFLAGS) "-I$(PERL_INC)" $(PASTHRU_DEFINE) $(DEFINE) $*.cpp

.cxx$(OBJ_EXT):
	$(CCCMD) $(CCCDLFLAGS) "-I$(PERL_INC)" $(PASTHRU_DEFINE) $(DEFINE) $*.cxx

.cc$(OBJ_EXT):
	$(CCCMD) $(CCCDLFLAGS) "-I$(PERL_INC)" $(PASTHRU_DEFINE) $(DEFINE) $*.cc
';
    join "", @m;
}

=item cflags (o)

Does very much the same as the cflags script in the perl
distribution. It doesn't return the whole compiler command line, but
initializes all of its parts. The const_cccmd method then actually
returns the definition of the CCCMD macro which uses these parts.

=cut

#'

sub cflags {
    my($self,$libperl)=@_;
    return $self->{CFLAGS} if $self->{CFLAGS};
    return '' unless $self->needs_linking();

    my($prog, $uc, $perltype, %cflags);
    $libperl ||= $self->{LIBPERL_A} || "libperl$self->{LIB_EXT}" ;
    $libperl =~ s/\.\$\(A\)$/$self->{LIB_EXT}/;

    @cflags{qw(cc ccflags optimize shellflags)}
	= @Config{qw(cc ccflags optimize shellflags)};
    my($optdebug) = "";

    $cflags{shellflags} ||= '';

    my(%map) =  (
		D =>   '-DDEBUGGING',
		E =>   '-DEMBED',
		DE =>  '-DDEBUGGING -DEMBED',
		M =>   '-DEMBED -DMULTIPLICITY',
		DM =>  '-DDEBUGGING -DEMBED -DMULTIPLICITY',
		);

    if ($libperl =~ /libperl(\w*)\Q$self->{LIB_EXT}/){
	$uc = uc($1);
    } else {
	$uc = ""; # avoid warning
    }
    $perltype = $map{$uc} ? $map{$uc} : "";

    if ($uc =~ /^D/) {
	$optdebug = "-g";
    }


    my($name);
    ( $name = $self->{NAME} . "_cflags" ) =~ s/:/_/g ;
    if ($prog = $Config{$name}) {
	# Expand hints for this extension via the shell
	print STDOUT "Processing $name hint:\n" if $Verbose;
	my(@o)=`cc=\"$cflags{cc}\"
	  ccflags=\"$cflags{ccflags}\"
	  optimize=\"$cflags{optimize}\"
	  perltype=\"$cflags{perltype}\"
	  optdebug=\"$cflags{optdebug}\"
	  eval '$prog'
	  echo cc=\$cc
	  echo ccflags=\$ccflags
	  echo optimize=\$optimize
	  echo perltype=\$perltype
	  echo optdebug=\$optdebug
	  `;
	my($line);
	foreach $line (@o){
	    chomp $line;
	    if ($line =~ /(.*?)=\s*(.*)\s*$/){
		$cflags{$1} = $2;
		print STDOUT "	$1 = $2\n" if $Verbose;
	    } else {
		print STDOUT "Unrecognised result from hint: '$line'\n";
	    }
	}
    }

    if ($optdebug) {
	$cflags{optimize} = $optdebug;
    }

    for (qw(ccflags optimize perltype)) {
        $cflags{$_} ||= '';
	$cflags{$_} =~ s/^\s+//;
	$cflags{$_} =~ s/\s+/ /g;
	$cflags{$_} =~ s/\s+$//;
	$self->{uc $_} ||= $cflags{$_};
    }

    if ($self->{POLLUTE}) {
	$self->{CCFLAGS} .= ' -DPERL_POLLUTE ';
    }

    my $pollute = '';
    if ($Config{usemymalloc} and not $Config{bincompat5005}
	and not $Config{ccflags} =~ /-DPERL_POLLUTE_MALLOC\b/
	and $self->{PERL_MALLOC_OK}) {
	$pollute = '$(PERL_MALLOC_DEF)';
    }

    $self->{CCFLAGS}  = quote_paren($self->{CCFLAGS});
    $self->{OPTIMIZE} = quote_paren($self->{OPTIMIZE});

    return $self->{CFLAGS} = qq{
CCFLAGS = $self->{CCFLAGS}
OPTIMIZE = $self->{OPTIMIZE}
PERLTYPE = $self->{PERLTYPE}
MPOLLUTE = $pollute
};

}

=item clean (o)

Defines the clean target.

=cut

sub clean {
# --- Cleanup and Distribution Sections ---

    my($self, %attribs) = @_;
    my(@m,$dir);
    push(@m, '
# Delete temporary files but do not touch installed files. We don\'t delete
# the Makefile here so a later make realclean still has a makefile to use.

clean ::
');
    # clean subdirectories first
    for $dir (@{$self->{DIR}}) {
	if ($Is_Win32  &&  Win32::IsWin95()) {
	    push @m, <<EOT;
	cd $dir
	\$(TEST_F) $self->{MAKEFILE}
	\$(MAKE) clean
	cd ..
EOT
	}
	else {
	    push @m, <<EOT;
	-cd $dir && \$(TEST_F) $self->{MAKEFILE} && \$(MAKE) clean
EOT
	}
    }

    my(@otherfiles) = values %{$self->{XS}}; # .c files from *.xs files
    if ( $^O eq 'qnx' ) {
      my @errfiles = @{$self->{C}};
      for ( @errfiles ) {
	s/.c$/.err/;
      }
      push( @otherfiles, @errfiles, 'perlmain.err' );
    }
    push(@otherfiles, $attribs{FILES}) if $attribs{FILES};
    push(@otherfiles, qw[./blib $(MAKE_APERL_FILE) 
                         $(INST_ARCHAUTODIR)/extralibs.all
			 perlmain.c tmon.out mon.out so_locations pm_to_blib
			 *$(OBJ_EXT) *$(LIB_EXT) perl.exe perl perl$(EXE_EXT)
			 $(BOOTSTRAP) $(BASEEXT).bso
			 $(BASEEXT).def lib$(BASEEXT).def
			 $(BASEEXT).exp $(BASEEXT).x
			]);
    if( $Is_VOS ) {
        push(@otherfiles, qw[*.kp]);
    }
    else {
        push(@otherfiles, qw[core core.*perl.*.? *perl.core]);
    }

    push @m, "\t-$self->{RM_RF} @otherfiles\n";
    # See realclean and ext/utils/make_ext for usage of Makefile.old
    push(@m,
	 "\t-$self->{MV} $self->{MAKEFILE} $self->{MAKEFILE}.old \$(DEV_NULL)\n");
    push(@m,
	 "\t$attribs{POSTOP}\n")   if $attribs{POSTOP};
    join("", @m);
}

=item const_cccmd (o)

Returns the full compiler call for C programs and stores the
definition in CONST_CCCMD.

=cut

sub const_cccmd {
    my($self,$libperl)=@_;
    return $self->{CONST_CCCMD} if $self->{CONST_CCCMD};
    return '' unless $self->needs_linking();
    return $self->{CONST_CCCMD} =
	q{CCCMD = $(CC) -c $(PASTHRU_INC) $(INC) \\
	$(CCFLAGS) $(OPTIMIZE) \\
	$(PERLTYPE) $(MPOLLUTE) $(DEFINE_VERSION) \\
	$(XS_DEFINE_VERSION)};
}

=item const_config (o)

Defines a couple of constants in the Makefile that are imported from
%Config.

=cut

sub const_config {
# --- Constants Sections ---

    my($self) = shift;
    my(@m,$m);
    push(@m,"\n# These definitions are from config.sh (via $INC{'Config.pm'})\n");
    push(@m,"\n# They may have been overridden via Makefile.PL or on the command line\n");
    my(%once_only);
    foreach $m (@{$self->{CONFIG}}){
	# SITE*EXP macros are defined in &constants; avoid duplicates here
	next if $once_only{$m} or $m eq 'sitelibexp' or $m eq 'sitearchexp';
	$self->{uc $m} = quote_paren($self->{uc $m});
	push @m, uc($m) , ' = ' , $self->{uc $m}, "\n";
	$once_only{$m} = 1;
    }
    join('', @m);
}

=item const_loadlibs (o)

Defines EXTRALIBS, LDLOADLIBS, BSLOADLIBS, LD_RUN_PATH. See
L<ExtUtils::Liblist> for details.

=cut

sub const_loadlibs {
    my($self) = shift;
    return "" unless $self->needs_linking;
    my @m;
    push @m, qq{
# $self->{NAME} might depend on some other libraries:
# See ExtUtils::Liblist for details
#
};
    my($tmp);
    for $tmp (qw/
	 EXTRALIBS LDLOADLIBS BSLOADLIBS LD_RUN_PATH
	 /) {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }
    return join "", @m;
}

=item constants (o)

Initializes lots of constants and .SUFFIXES and .PHONY

=cut

sub constants {
    my($self) = @_;
    my(@m,$tmp);

    for $tmp (qw/

	      AR_STATIC_ARGS NAME DISTNAME NAME_SYM VERSION
	      VERSION_SYM XS_VERSION 
	      INST_ARCHLIB INST_SCRIPT INST_BIN INST_LIB
              INSTALLDIRS
              PREFIX          SITEPREFIX      VENDORPREFIX
	      INSTALLPRIVLIB  INSTALLSITELIB  INSTALLVENDORLIB
	      INSTALLARCHLIB  INSTALLSITEARCH INSTALLVENDORARCH
              INSTALLBIN      INSTALLSITEBIN  INSTALLVENDORBIN  INSTALLSCRIPT 
              PERL_LIB        PERL_ARCHLIB 
              SITELIBEXP      SITEARCHEXP 
              LIBPERL_A MYEXTLIB
	      FIRST_MAKEFILE MAKE_APERL_FILE PERLMAINCC PERL_SRC
	      PERL_INC PERL FULLPERL PERLRUN FULLPERLRUN PERLRUNINST 
              FULLPERLRUNINST ABSPERL ABSPERLRUN ABSPERLRUNINST
              FULL_AR PERL_CORE NOOP NOECHO

	      / ) 
    {
	next unless defined $self->{$tmp};

        # pathnames can have sharp signs in them; escape them so
        # make doesn't think it is a comment-start character.
        $self->{$tmp} =~ s/#/\\#/g;
	push @m, "$tmp = $self->{$tmp}\n";
    }

    push @m, qq{
VERSION_MACRO = VERSION
DEFINE_VERSION = -D\$(VERSION_MACRO)=\\\"\$(VERSION)\\\"
XS_VERSION_MACRO = XS_VERSION
XS_DEFINE_VERSION = -D\$(XS_VERSION_MACRO)=\\\"\$(XS_VERSION)\\\"
PERL_MALLOC_DEF = -DPERL_EXTMALLOC_DEF -Dmalloc=Perl_malloc -Dfree=Perl_mfree -Drealloc=Perl_realloc -Dcalloc=Perl_calloc
};

    push @m, qq{
MAKEMAKER = $INC{'ExtUtils/MakeMaker.pm'}
MM_VERSION = $ExtUtils::MakeMaker::VERSION
};

    push @m, q{
# FULLEXT = Pathname for extension directory (eg Foo/Bar/Oracle).
# BASEEXT = Basename part of FULLEXT. May be just equal FULLEXT. (eg Oracle)
# PARENT_NAME = NAME without BASEEXT and no trailing :: (eg Foo::Bar)
# DLBASE  = Basename part of dynamic library. May be just equal BASEEXT.
};

    for $tmp (qw/
	      FULLEXT BASEEXT PARENT_NAME DLBASE VERSION_FROM INC DEFINE OBJECT
	      LDFROM LINKTYPE PM_FILTER
	      /	) 
    {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }

    push @m, "
# Handy lists of source code files:
XS_FILES= ".join(" \\\n\t", sort keys %{$self->{XS}})."
C_FILES = ".join(" \\\n\t", @{$self->{C}})."
O_FILES = ".join(" \\\n\t", @{$self->{O_FILES}})."
H_FILES = ".join(" \\\n\t", @{$self->{H}})."
MAN1PODS = ".join(" \\\n\t", sort keys %{$self->{MAN1PODS}})."
MAN3PODS = ".join(" \\\n\t", sort keys %{$self->{MAN3PODS}})."
";

    for $tmp (qw/
	      INST_MAN1DIR  MAN1EXT 
              INSTALLMAN1DIR INSTALLSITEMAN1DIR INSTALLVENDORMAN1DIR
	      INST_MAN3DIR  MAN3EXT
              INSTALLMAN3DIR INSTALLSITEMAN3DIR INSTALLVENDORMAN3DIR
	      /) 
    {
	next unless defined $self->{$tmp};
	push @m, "$tmp = $self->{$tmp}\n";
    }

    for $tmp (qw(
		PERM_RW PERM_RWX
		)
	     ) 
    {
        my $method = lc($tmp);
	# warn "self[$self] method[$method]";
        push @m, "$tmp = ", $self->$method(), "\n";
    }

    push @m, q{
.NO_CONFIG_REC: Makefile
} if $ENV{CLEARCASE_ROOT};

    # why not q{} ? -- emacs
    push @m, qq{
# work around a famous dec-osf make(1) feature(?):
makemakerdflt: all

.SUFFIXES: .xs .c .C .cpp .i .s .cxx .cc \$(OBJ_EXT)

# Nick wanted to get rid of .PRECIOUS. I don't remember why. I seem to recall, that
# some make implementations will delete the Makefile when we rebuild it. Because
# we call false(1) when we rebuild it. So make(1) is not completely wrong when it
# does so. Our milage may vary.
# .PRECIOUS: Makefile    # seems to be not necessary anymore

.PHONY: all config static dynamic test linkext manifest

# Where is the Config information that we are using/depend on
CONFIGDEP = \$(PERL_ARCHLIB)/Config.pm \$(PERL_INC)/config.h
};

    my @parentdir = split(/::/, $self->{PARENT_NAME});
    push @m, q{
# Where to put things:
INST_LIBDIR      = }. File::Spec->catdir('$(INST_LIB)',@parentdir)        .q{
INST_ARCHLIBDIR  = }. File::Spec->catdir('$(INST_ARCHLIB)',@parentdir)    .q{

INST_AUTODIR     = }. File::Spec->catdir('$(INST_LIB)','auto','$(FULLEXT)')       .q{
INST_ARCHAUTODIR = }. File::Spec->catdir('$(INST_ARCHLIB)','auto','$(FULLEXT)')   .q{
};

    if ($self->has_link_code()) {
	push @m, '
INST_STATIC  = $(INST_ARCHAUTODIR)/$(BASEEXT)$(LIB_EXT)
INST_DYNAMIC = $(INST_ARCHAUTODIR)/$(DLBASE).$(DLEXT)
INST_BOOT    = $(INST_ARCHAUTODIR)/$(BASEEXT).bs
';
    } else {
	push @m, '
INST_STATIC  =
INST_DYNAMIC =
INST_BOOT    =
';
    }

    $tmp = $self->export_list;
    push @m, "
EXPORT_LIST = $tmp
";
    $tmp = $self->perl_archive;
    push @m, "
PERL_ARCHIVE = $tmp
";
    $tmp = $self->perl_archive_after;
    push @m, "
PERL_ARCHIVE_AFTER = $tmp
";

    push @m, q{
TO_INST_PM = }.join(" \\\n\t", sort keys %{$self->{PM}}).q{

PM_TO_BLIB = }.join(" \\\n\t", %{$self->{PM}}).q{
};

    join('',@m);
}

=item depend (o)

Same as macro for the depend attribute.

=cut

sub depend {
    my($self,%attribs) = @_;
    my(@m,$key,$val);
    while (($key,$val) = each %attribs){
	last unless defined $key;
	push @m, "$key : $val\n";
    }
    join "", @m;
}

=item dir_target (o)

Takes an array of directories that need to exist and returns a
Makefile entry for a .exists file in these directories. Returns
nothing, if the entry has already been processed. We're helpless
though, if the same directory comes as $(FOO) _and_ as "bar". Both of
them get an entry, that's why we use "::".

=cut

sub dir_target {
# --- Make-Directories section (internal method) ---
# dir_target(@array) returns a Makefile entry for the file .exists in each
# named directory. Returns nothing, if the entry has already been processed.
# We're helpless though, if the same directory comes as $(FOO) _and_ as "bar".
# Both of them get an entry, that's why we use "::". I chose '$(PERL)' as the
# prerequisite, because there has to be one, something that doesn't change
# too often :)

    my($self,@dirs) = @_;
    my(@m,$dir,$targdir);
    foreach $dir (@dirs) {
	my($src) = File::Spec->catfile($self->{PERL_INC},'perl.h');
	my($targ) = File::Spec->catfile($dir,'.exists');
	# catfile may have adapted syntax of $dir to target OS, so...
	if ($Is_VMS) { # Just remove file name; dirspec is often in macro
	    ($targdir = $targ) =~ s:/?\.exists\z::;
	}
	else { # while elsewhere we expect to see the dir separator in $targ
	    $targdir = dirname($targ);
	}
	next if $self->{DIR_TARGET}{$self}{$targdir}++;
	push @m, qq{
$targ :: $src
	$self->{NOECHO}\$(MKPATH) $targdir
	$self->{NOECHO}\$(EQUALIZE_TIMESTAMP) $src $targ
};
	push(@m, qq{
	-$self->{NOECHO}\$(CHMOD) \$(PERM_RWX) $targdir
}) unless $Is_VMS;
    }
    join "", @m;
}

=item dist (o)

Defines a lot of macros for distribution support.

=cut

sub dist {
    my($self, %attribs) = @_;

    # VERSION should be sanitised before use as a file name
    $attribs{VERSION}  ||= '$(VERSION)';
    $attribs{NAME}     ||= '$(DISTNAME)';
    $attribs{TAR}      ||= 'tar';
    $attribs{TARFLAGS} ||= 'cvf';
    $attribs{ZIP}      ||= 'zip';
    $attribs{ZIPFLAGS} ||= '-r';
    $attribs{COMPRESS} ||= 'gzip --best';
    $attribs{SUFFIX}   ||= '.gz';
    $attribs{SHAR}     ||= 'shar';
    $attribs{PREOP}    ||= "$self->{NOECHO}\$(NOOP)"; # eg update MANIFEST
    $attribs{POSTOP}   ||= "$self->{NOECHO}\$(NOOP)"; # eg remove the distdir
    $attribs{TO_UNIX}  ||= "$self->{NOECHO}\$(NOOP)";

    $attribs{CI}       ||= 'ci -u';
    $attribs{RCS_LABEL}||= 'rcs -Nv$(VERSION_SYM): -q';
    $attribs{DIST_CP}  ||= 'best';
    $attribs{DIST_DEFAULT} ||= 'tardist';

    $attribs{DISTVNAME} ||= "$attribs{NAME}-$attribs{VERSION}";

    # We've already printed out VERSION and NAME variables.
    delete $attribs{VERSION};
    delete $attribs{NAME};

    my $make = '';
    while(my($var, $value) = each %attribs) {
        $make .= "$var = $value\n";
    }

    return $make;
}

=item dist_basics (o)

Defines the targets distclean, distcheck, skipcheck, manifest, veryclean.

=cut

sub dist_basics {
    my($self) = shift;

    return <<'MAKE_FRAG';
distclean :: realclean distcheck
	$(NOECHO) $(NOOP)

distcheck :
	$(PERLRUN) "-MExtUtils::Manifest=fullcheck" -e fullcheck

skipcheck :
	$(PERLRUN) "-MExtUtils::Manifest=skipcheck" -e skipcheck

manifest :
	$(PERLRUN) "-MExtUtils::Manifest=mkmanifest" -e mkmanifest

veryclean : realclean
	$(RM_F) *~ *.orig */*~ */*.orig

MAKE_FRAG

}

=item dist_ci (o)

Defines a check in target for RCS.

=cut

sub dist_ci {
    my($self) = shift;
    my @m;
    push @m, q{
ci :
	$(PERLRUN) "-MExtUtils::Manifest=maniread" \\
		-e "@all = keys %{ maniread() };" \\
		-e 'print("Executing $(CI) @all\n"); system("$(CI) @all");' \\
		-e 'print("Executing $(RCS_LABEL) ...\n"); system("$(RCS_LABEL) @all");'
};
    join "", @m;
}

=item dist_core (o)

Defines the targets dist, tardist, zipdist, uutardist, shdist

=cut

sub dist_core {
    my($self) = shift;
    my @m;
    push @m, q{
dist : $(DIST_DEFAULT)
	}.$self->{NOECHO}.q{$(PERL) -le 'print "Warning: Makefile possibly out of date with $$vf" if ' \
	    -e '-e ($$vf="$(VERSION_FROM)") and -M $$vf < -M "}.$self->{MAKEFILE}.q{";'

tardist : $(DISTVNAME).tar$(SUFFIX)

zipdist : $(DISTVNAME).zip

$(DISTVNAME).tar$(SUFFIX) : distdir
	$(PREOP)
	$(TO_UNIX)
	$(TAR) $(TARFLAGS) $(DISTVNAME).tar $(DISTVNAME)
	$(RM_RF) $(DISTVNAME)
	$(COMPRESS) $(DISTVNAME).tar
	$(POSTOP)

$(DISTVNAME).zip : distdir
	$(PREOP)
	$(ZIP) $(ZIPFLAGS) $(DISTVNAME).zip $(DISTVNAME)
	$(RM_RF) $(DISTVNAME)
	$(POSTOP)

uutardist : $(DISTVNAME).tar$(SUFFIX)
	uuencode $(DISTVNAME).tar$(SUFFIX) \\
		$(DISTVNAME).tar$(SUFFIX) > \\
		$(DISTVNAME).tar$(SUFFIX)_uu

shdist : distdir
	$(PREOP)
	$(SHAR) $(DISTVNAME) > $(DISTVNAME).shar
	$(RM_RF) $(DISTVNAME)
	$(POSTOP)
};
    join "", @m;
}

=item dist_dir

Defines the scratch directory target that will hold the distribution
before tar-ing (or shar-ing).

=cut

sub dist_dir {
    my($self) = shift;

    return <<'MAKE_FRAG';
distdir :
	$(RM_RF) $(DISTVNAME)
	$(PERLRUN) "-MExtUtils::Manifest=manicopy,maniread" \
		-e "manicopy(maniread(),'$(DISTVNAME)', '$(DIST_CP)');"

MAKE_FRAG

}

=item dist_test

Defines a target that produces the distribution in the
scratchdirectory, and runs 'perl Makefile.PL; make ;make test' in that
subdirectory.

=cut

sub dist_test {
    my($self) = shift;
    my @m;
    push @m, q{
disttest : distdir
	cd $(DISTVNAME) && $(ABSPERLRUN) Makefile.PL
	cd $(DISTVNAME) && $(MAKE) $(PASTHRU)
	cd $(DISTVNAME) && $(MAKE) test $(PASTHRU)
};
    join "", @m;
}

=item dlsyms (o)

Used by AIX and VMS to define DL_FUNCS and DL_VARS and write the *.exp
files.

=cut

sub dlsyms {
    my($self,%attribs) = @_;

    return '' unless ($^O eq 'aix' && $self->needs_linking() );

    my($funcs) = $attribs{DL_FUNCS} || $self->{DL_FUNCS} || {};
    my($vars)  = $attribs{DL_VARS} || $self->{DL_VARS} || [];
    my($funclist)  = $attribs{FUNCLIST} || $self->{FUNCLIST} || [];
    my(@m);

    push(@m,"
dynamic :: $self->{BASEEXT}.exp

") unless $self->{SKIPHASH}{'dynamic'}; # dynamic and static are subs, so...

    push(@m,"
static :: $self->{BASEEXT}.exp

") unless $self->{SKIPHASH}{'static'};  # we avoid a warning if we tick them

    push(@m,"
$self->{BASEEXT}.exp: Makefile.PL
",'	$(PERLRUN) -e \'use ExtUtils::Mksymlists; \\
	Mksymlists("NAME" => "',$self->{NAME},'", "DL_FUNCS" => ',
	neatvalue($funcs), ', "FUNCLIST" => ', neatvalue($funclist),
	', "DL_VARS" => ', neatvalue($vars), ');\'
');

    join('',@m);
}

=item dynamic (o)

Defines the dynamic target.

=cut

sub dynamic {
# --- Dynamic Loading Sections ---

    my($self) = shift;
    '
## $(INST_PM) has been moved to the all: target.
## It remains here for awhile to allow for old usage: "make dynamic"
#dynamic :: '.$self->{MAKEFILE}.' $(INST_DYNAMIC) $(INST_BOOT) $(INST_PM)
dynamic :: '.$self->{MAKEFILE}.' $(INST_DYNAMIC) $(INST_BOOT)
	'.$self->{NOECHO}.'$(NOOP)
';
}

=item dynamic_bs (o)

Defines targets for bootstrap files.

=cut

sub dynamic_bs {
    my($self, %attribs) = @_;
    return '
BOOTSTRAP =
' unless $self->has_link_code();

    return '
BOOTSTRAP = '."$self->{BASEEXT}.bs".'

# As Mkbootstrap might not write a file (if none is required)
# we use touch to prevent make continually trying to remake it.
# The DynaLoader only reads a non-empty file.
$(BOOTSTRAP): '."$self->{MAKEFILE} $self->{BOOTDEP}".' $(INST_ARCHAUTODIR)/.exists
	'.$self->{NOECHO}.'echo "Running Mkbootstrap for $(NAME) ($(BSLOADLIBS))"
	'.$self->{NOECHO}.'$(PERLRUN) \
		"-MExtUtils::Mkbootstrap" \
		-e "Mkbootstrap(\'$(BASEEXT)\',\'$(BSLOADLIBS)\');"
	'.$self->{NOECHO}.'$(TOUCH) $(BOOTSTRAP)
	$(CHMOD) $(PERM_RW) $@

$(INST_BOOT): $(BOOTSTRAP) $(INST_ARCHAUTODIR)/.exists
	'."$self->{NOECHO}$self->{RM_RF}".' $(INST_BOOT)
	-'.$self->{CP}.' $(BOOTSTRAP) $(INST_BOOT)
	$(CHMOD) $(PERM_RW) $@
';
}

=item dynamic_lib (o)

Defines how to produce the *.so (or equivalent) files.

=cut

sub dynamic_lib {
    my($self, %attribs) = @_;
    return '' unless $self->needs_linking(); #might be because of a subdir

    return '' unless $self->has_link_code;

    my($otherldflags) = $attribs{OTHERLDFLAGS} || "";
    my($inst_dynamic_dep) = $attribs{INST_DYNAMIC_DEP} || "";
    my($armaybe) = $attribs{ARMAYBE} || $self->{ARMAYBE} || ":";
    my($ldfrom) = '$(LDFROM)';
    $armaybe = 'ar' if ($^O eq 'dec_osf' and $armaybe eq ':');
    my(@m);
    my $ld_opt = $Is_OS2 ? '$(OPTIMIZE) ' : '';	# Useful on other systems too?
    push(@m,'
# This section creates the dynamically loadable $(INST_DYNAMIC)
# from $(OBJECT) and possibly $(MYEXTLIB).
ARMAYBE = '.$armaybe.'
OTHERLDFLAGS = '.$ld_opt.$otherldflags.'
INST_DYNAMIC_DEP = '.$inst_dynamic_dep.'

$(INST_DYNAMIC): $(OBJECT) $(MYEXTLIB) $(BOOTSTRAP) $(INST_ARCHAUTODIR)/.exists $(EXPORT_LIST) $(PERL_ARCHIVE) $(PERL_ARCHIVE_AFTER) $(INST_DYNAMIC_DEP)
');
    if ($armaybe ne ':'){
	$ldfrom = 'tmp$(LIB_EXT)';
	push(@m,'	$(ARMAYBE) cr '.$ldfrom.' $(OBJECT)'."\n");
	push(@m,'	$(RANLIB) '."$ldfrom\n");
    }
    $ldfrom = "-all $ldfrom -none" if ($^O eq 'dec_osf');

    # The IRIX linker doesn't use LD_RUN_PATH
    my $ldrun = $^O eq 'irix' && $self->{LD_RUN_PATH} ?         
                       qq{-rpath "$self->{LD_RUN_PATH}"} : '';

    # For example in AIX the shared objects/libraries from previous builds
    # linger quite a while in the shared dynalinker cache even when nobody
    # is using them.  This is painful if one for instance tries to restart
    # a failed build because the link command will fail unnecessarily 'cos
    # the shared object/library is 'busy'.
    push(@m,'	$(RM_F) $@
');

    my $libs = '$(LDLOADLIBS)';

    if ($^O eq 'netbsd') {
	# Use nothing on static perl platforms, and to the flags needed
	# to link against the shared libperl library on shared perl
	# platforms.  We peek at lddlflags to see if we need -Wl,-R
	# or -R to add paths to the run-time library search path.
	if ($Config{'useshrplib'}) {
	    if ($Config{'lddlflags'} =~ /-Wl,-R/) {
		$libs .= ' -L$(PERL_INC) -Wl,-R$(INSTALLARCHLIB)/CORE -lperl';
	    } elsif ($Config{'lddlflags'} =~ /-R/) {
		$libs .= ' -L$(PERL_INC) -R$(INSTALLARCHLIB)/CORE -lperl';
	    }
	}
    }

    push(@m,
'	LD_RUN_PATH="$(LD_RUN_PATH)" $(LD) '.$ldrun.' $(LDDLFLAGS) '.$ldfrom.
' $(OTHERLDFLAGS) -o $@ $(MYEXTLIB) $(PERL_ARCHIVE) '.$libs.' $(PERL_ARCHIVE_AFTER) $(EXPORT_LIST)');
    push @m, '
	$(CHMOD) $(PERM_RWX) $@
';

    push @m, $self->dir_target('$(INST_ARCHAUTODIR)');
    join('',@m);
}

=item exescan

Deprecated method. Use libscan instead.

=cut

sub exescan {
    my($self,$path) = @_;
    $path;
}

=item extliblist

Called by init_others, and calls ext ExtUtils::Liblist. See
L<ExtUtils::Liblist> for details.

=cut

sub extliblist {
    my($self,$libs) = @_;
    require ExtUtils::Liblist;
    $self->ext($libs, $Verbose);
}

=item find_perl

Finds the executables PERL and FULLPERL

=cut

sub find_perl {
    my($self, $ver, $names, $dirs, $trace) = @_;
    my($name, $dir);
    if ($trace >= 2){
	print "Looking for perl $ver by these names:
@$names
in these dirs:
@$dirs
";
    }
    foreach $name (@$names){
	foreach $dir (@$dirs){
	    next unless defined $dir; # $self->{PERL_SRC} may be undefined
	    my ($abs, $val);
	    if (File::Spec->file_name_is_absolute($name)) { # /foo/bar
		$abs = $name;
	    } elsif (File::Spec->canonpath($name) eq File::Spec->canonpath(basename($name))) { # foo
		$abs = File::Spec->catfile($dir, $name);
	    } else { # foo/bar
		$abs = File::Spec->canonpath(File::Spec->catfile($Curdir, $name));
	    }
	    print "Checking $abs\n" if ($trace >= 2);
	    next unless $self->maybe_command($abs);
	    print "Executing $abs\n" if ($trace >= 2);
	    $val = `$abs -e 'require $ver; print "VER_OK\n" ' 2>&1`;
	    if ($val =~ /VER_OK/) {
	        print "Using PERL=$abs\n" if $trace;
	        return $abs;
	    } elsif ($trace >= 2) {
		print "Result: `$val'\n";
	    }
	}
    }
    print STDOUT "Unable to find a perl $ver (by these names: @$names, in these dirs: @$dirs)\n";
    0; # false and not empty
}

=item find_tests

  my $test = $mm->find_tests;

Returns a string suitable for feeding to the shell to return all
tests in t/*.t.

=cut

sub find_tests {
    my($self) = shift;
    return 't/*.t';
}

=back

=head2 Methods to actually produce chunks of text for the Makefile

The methods here are called for each MakeMaker object in the order
specified by @ExtUtils::MakeMaker::MM_Sections.

=over 2

=item fixin

  $mm->fixin(@files);

Inserts the sharpbang or equivalent magic number to a set of @files.

=cut

sub fixin { # stolen from the pink Camel book, more or less
    my($self, @files) = @_;

    my($does_shbang) = $Config{'sharpbang'} =~ /^\s*\#\!/;
    for my $file (@files) {
	local(*FIXIN);
	local(*FIXOUT);
	open(FIXIN, $file) or croak "Can't process '$file': $!";
	local $/ = "\n";
	chomp(my $line = <FIXIN>);
	next unless $line =~ s/^\s*\#!\s*//;     # Not a shbang file.
	# Now figure out the interpreter name.
	my($cmd,$arg) = split ' ', $line, 2;
	$cmd =~ s!^.*/!!;

	# Now look (in reverse) for interpreter in absolute PATH (unless perl).
        my $interpreter;
	if ($cmd eq "perl") {
            if ($Config{startperl} =~ m,^\#!.*/perl,) {
                $interpreter = $Config{startperl};
                $interpreter =~ s,^\#!,,;
            } else {
                $interpreter = $Config{perlpath};
            }
	} else {
	    my(@absdirs) = reverse grep {File::Spec->file_name_is_absolute} File::Spec->path;
	    $interpreter = '';
	    my($dir);
	    foreach $dir (@absdirs) {
		if ($self->maybe_command($cmd)) {
		    warn "Ignoring $interpreter in $file\n" if $Verbose && $interpreter;
		    $interpreter = File::Spec->catfile($dir,$cmd);
		}
	    }
	}
	# Figure out how to invoke interpreter on this machine.

	my($shb) = "";
	if ($interpreter) {
	    print STDOUT "Changing sharpbang in $file to $interpreter" if $Verbose;
	    # this is probably value-free on DOSISH platforms
	    if ($does_shbang) {
		$shb .= "$Config{'sharpbang'}$interpreter";
		$shb .= ' ' . $arg if defined $arg;
		$shb .= "\n";
	    }
	    $shb .= qq{
eval 'exec $interpreter $arg -S \$0 \${1+"\$\@"}'
    if 0; # not running under some shell
} unless $Is_Win32; # this won't work on win32, so don't
	} else {
	    warn "Can't find $cmd in PATH, $file unchanged"
		if $Verbose;
	    next;
	}

	unless ( open(FIXOUT,">$file.new") ) {
	    warn "Can't create new $file: $!\n";
	    next;
	}
	my($dev,$ino,$mode) = stat FIXIN;
	
	# Print out the new #! line (or equivalent).
	local $\;
	undef $/;
	print FIXOUT $shb, <FIXIN>;
	close FIXIN;
	close FIXOUT;

	unless ( rename($file, "$file.bak") ) {	
	    warn "Can't rename $file to $file.bak: $!";
	    next;
	}
	unless ( rename("$file.new", $file) ) {	
	    warn "Can't rename $file.new to $file: $!";
	    unless ( rename("$file.bak", $file) ) {
	        warn "Can't rename $file.bak back to $file either: $!";
		warn "Leaving $file renamed as $file.bak\n";
	    }
	    next;
	}
	unlink "$file.bak";
    } continue {
	close(FIXIN) if fileno(FIXIN);
	system("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';;
    }
}

=item force (o)

Just writes FORCE:

=cut

sub force {
    my($self) = shift;
    '# Phony target to force checking subdirectories.
FORCE:
	'.$self->{NOECHO}.'$(NOOP)
';
}

=item guess_name

Guess the name of this package by examining the working directory's
name. MakeMaker calls this only if the developer has not supplied a
NAME attribute.

=cut

# ';

sub guess_name {
    my($self) = @_;
    use Cwd 'cwd';
    my $name = basename(cwd());
    $name =~ s|[\-_][\d\.\-]+\z||;  # this is new with MM 5.00, we
                                    # strip minus or underline
                                    # followed by a float or some such
    print "Warning: Guessing NAME [$name] from current directory name.\n";
    $name;
}

=item has_link_code

Returns true if C, XS, MYEXTLIB or similar objects exist within this
object that need a compiler. Does not descend into subdirectories as
needs_linking() does.

=cut

sub has_link_code {
    my($self) = shift;
    return $self->{HAS_LINK_CODE} if defined $self->{HAS_LINK_CODE};
    if ($self->{OBJECT} or @{$self->{C} || []} or $self->{MYEXTLIB}){
	$self->{HAS_LINK_CODE} = 1;
	return 1;
    }
    return $self->{HAS_LINK_CODE} = 0;
}


=item init_dirscan

Initializes DIR, XS, PM, C, O_FILES, H, PL_FILES, MAN*PODS, EXE_FILES.

=cut

sub init_dirscan {	# --- File and Directory Lists (.xs .pm .pod etc)
    my($self) = @_;
    my($name, %dir, %xs, %c, %h, %ignore, %pl_files, %manifypods);
    local(%pm); #the sub in find() has to see this hash

    @ignore{qw(Makefile.PL test.pl t)} = (1,1,1);

    # ignore the distdir
    $Is_VMS ? $ignore{"$self->{DISTVNAME}.dir"} = 1
            : $ignore{$self->{DISTVNAME}} = 1;

    @ignore{map lc, keys %ignore} = values %ignore if $Is_VMS;

    foreach $name ($self->lsdir($Curdir)){
	next if $name =~ /\#/;
	next if $name eq $Curdir or $name eq $Updir or $ignore{$name};
	next unless $self->libscan($name);
	if (-d $name){
	    next if -l $name; # We do not support symlinks at all
	    $dir{$name} = $name if (-f File::Spec->catfile($name,"Makefile.PL"));
	} elsif ($name =~ /\.xs\z/){
	    my($c); ($c = $name) =~ s/\.xs\z/.c/;
	    $xs{$name} = $c;
	    $c{$c} = 1;
	} elsif ($name =~ /\.c(pp|xx|c)?\z/i){  # .c .C .cpp .cxx .cc
	    $c{$name} = 1
		unless $name =~ m/perlmain\.c/; # See MAP_TARGET
	} elsif ($name =~ /\.h\z/i){
	    $h{$name} = 1;
	} elsif ($name =~ /\.PL\z/) {
	    ($pl_files{$name} = $name) =~ s/\.PL\z// ;
	} elsif (($Is_VMS || $Is_Dos) && $name =~ /[._]pl$/i) {
	    # case-insensitive filesystem, one dot per name, so foo.h.PL
	    # under Unix appears as foo.h_pl under VMS or fooh.pl on Dos
	    local($/); open(PL,$name); my $txt = <PL>; close PL;
	    if ($txt =~ /Extracting \S+ \(with variable substitutions/) {
		($pl_files{$name} = $name) =~ s/[._]pl\z//i ;
	    }
	    else { 
                $pm{$name} = File::Spec->catfile($self->{INST_LIBDIR},$name); 
            }
	} elsif ($name =~ /\.(p[ml]|pod)\z/){
	    $pm{$name} = File::Spec->catfile($self->{INST_LIBDIR},$name);
	}
    }

    # Some larger extensions often wish to install a number of *.pm/pl
    # files into the library in various locations.

    # The attribute PMLIBDIRS holds an array reference which lists
    # subdirectories which we should search for library files to
    # install. PMLIBDIRS defaults to [ 'lib', $self->{BASEEXT} ].  We
    # recursively search through the named directories (skipping any
    # which don't exist or contain Makefile.PL files).

    # For each *.pm or *.pl file found $self->libscan() is called with
    # the default installation path in $_[1]. The return value of
    # libscan defines the actual installation location.  The default
    # libscan function simply returns the path.  The file is skipped
    # if libscan returns false.

    # The default installation location passed to libscan in $_[1] is:
    #
    #  ./*.pm		=> $(INST_LIBDIR)/*.pm
    #  ./xyz/...	=> $(INST_LIBDIR)/xyz/...
    #  ./lib/...	=> $(INST_LIB)/...
    #
    # In this way the 'lib' directory is seen as the root of the actual
    # perl library whereas the others are relative to INST_LIBDIR
    # (which includes PARENT_NAME). This is a subtle distinction but one
    # that's important for nested modules.

    if ($Is_VMS) {
      # avoid logical name collisions by adding directory syntax
      $self->{PMLIBDIRS} = ['./lib', './' . $self->{BASEEXT}]
	unless $self->{PMLIBDIRS};
    }
    else {
      $self->{PMLIBDIRS} = ['lib', $self->{BASEEXT}]
       unless $self->{PMLIBDIRS};
    }

    #only existing directories that aren't in $dir are allowed

    # Avoid $_ wherever possible:
    # @{$self->{PMLIBDIRS}} = grep -d && !$dir{$_}, @{$self->{PMLIBDIRS}};
    my (@pmlibdirs) = @{$self->{PMLIBDIRS}};
    my ($pmlibdir);
    @{$self->{PMLIBDIRS}} = ();
    foreach $pmlibdir (@pmlibdirs) {
	-d $pmlibdir && !$dir{$pmlibdir} && push @{$self->{PMLIBDIRS}}, $pmlibdir;
    }

    if (@{$self->{PMLIBDIRS}}){
	print "Searching PMLIBDIRS: @{$self->{PMLIBDIRS}}\n"
	    if ($Verbose >= 2);
	require File::Find;
        File::Find::find(sub {
            if (-d $_){
                if ($_ eq "CVS" || $_ eq "RCS"){
                    $File::Find::prune = 1;
                }
                return;
            }
            return if /\#/;
            return if /~$/;    # emacs temp files

	    my $path   = $File::Find::name;
            my $prefix = $self->{INST_LIBDIR};
            my $striplibpath;

	    $prefix =  $self->{INST_LIB} 
                if ($striplibpath = $path) =~ s:^(\W*)lib\W:$1:i;

	    my($inst) = File::Spec->catfile($prefix,$striplibpath);
	    local($_) = $inst; # for backwards compatibility
	    $inst = $self->libscan($inst);
	    print "libscan($path) => '$inst'\n" if ($Verbose >= 2);
	    return unless $inst;
	    $pm{$path} = $inst;
	}, @{$self->{PMLIBDIRS}});
    }

    $self->{DIR} = [sort keys %dir] unless $self->{DIR};
    $self->{XS}  = \%xs             unless $self->{XS};
    $self->{PM}  = \%pm             unless $self->{PM};
    $self->{C}   = [sort keys %c]   unless $self->{C};
    my(@o_files) = @{$self->{C}};
    $self->{O_FILES} = [grep s/\.c(pp|xx|c)?\z/$self->{OBJ_EXT}/i, @o_files] ;
    $self->{H}   = [sort keys %h]   unless $self->{H};
    $self->{PL_FILES} = \%pl_files unless $self->{PL_FILES};

    # Set up names of manual pages to generate from pods
    my %pods;
    foreach my $man (qw(MAN1 MAN3)) {
	unless ($self->{"${man}PODS"}) {
	    $self->{"${man}PODS"} = {};
	    $pods{$man} = 1 unless $self->{"INST_${man}DIR"} =~ /^(none|\s*)$/;
	}
    }

    if ($pods{MAN1}) {
	if ( exists $self->{EXE_FILES} ) {
	    foreach $name (@{$self->{EXE_FILES}}) {
		local *FH;
		my($ispod)=0;
		if (open(FH,"<$name")) {
		    while (<FH>) {
			if (/^=(head[1-4]|item|pod)\b/) {
			    $ispod=1;
			    last;
			}
		    }
		    close FH;
		} else {
		    # If it doesn't exist yet, we assume, it has pods in it
		    $ispod = 1;
		}
		next unless $ispod;
		if ($pods{MAN1}) {
		    $self->{MAN1PODS}->{$name} =
		      File::Spec->catfile("\$(INST_MAN1DIR)", basename($name).".\$(MAN1EXT)");
		}
	    }
	}
    }
    if ($pods{MAN3}) {
	my %manifypods = (); # we collect the keys first, i.e. the files
			     # we have to convert to pod
	foreach $name (keys %{$self->{PM}}) {
	    if ($name =~ /\.pod\z/ ) {
		$manifypods{$name} = $self->{PM}{$name};
	    } elsif ($name =~ /\.p[ml]\z/ ) {
		local *FH;
		my($ispod)=0;
		if (open(FH,"<$name")) {
		    while (<FH>) {
			if (/^=(head[1-4]|item|pod)\b/) {
			    $ispod=1;
			    last;
			}
		    }
		    close FH;
		} else {
		    $ispod = 1;
		}
		if( $ispod ) {
		    $manifypods{$name} = $self->{PM}{$name};
		}
	    }
	}

	# Remove "Configure.pm" and similar, if it's not the only pod listed
	# To force inclusion, just name it "Configure.pod", or override 
        # MAN3PODS
	foreach $name (keys %manifypods) {
           if ($self->{PERL_CORE} and $name =~ /(config|setup).*\.pm/is) {
		delete $manifypods{$name};
		next;
	    }
	    my($manpagename) = $name;
	    $manpagename =~ s/\.p(od|m|l)\z//;
	    unless ($manpagename =~ s!^\W*lib\W+!!s) { # everything below lib is ok
		$manpagename = File::Spec->catfile(split(/::/,$self->{PARENT_NAME}),$manpagename);
	    }
	    if ($pods{MAN3}) {
		$manpagename = $self->replace_manpage_separator($manpagename);
		$self->{MAN3PODS}->{$name} =
		  File::Spec->catfile("\$(INST_MAN3DIR)", "$manpagename.\$(MAN3EXT)");
	    }
	}
    }
}

=item init_main

Initializes AR, AR_STATIC_ARGS, BASEEXT, CONFIG, DISTNAME, DLBASE,
EXE_EXT, FULLEXT, FULLPERL, FULLPERLRUN, FULLPERLRUNINST, INST_*,
INSTALL*, INSTALLDIRS, LD, LIB_EXT, LIBPERL_A, MAP_TARGET, NAME,
OBJ_EXT, PARENT_NAME, PERL, PERL_ARCHLIB, PERL_INC, PERL_LIB,
PERL_SRC, PERLRUN, PERLRUNINST, PREFIX, VERSION,
VERSION_FROM, VERSION_SYM, XS_VERSION.

=cut

sub init_main {
    my($self) = @_;

    # --- Initialize Module Name and Paths

    # NAME    = Foo::Bar::Oracle
    # FULLEXT = Foo/Bar/Oracle
    # BASEEXT = Oracle
    # PARENT_NAME = Foo::Bar
### Only UNIX:
###    ($self->{FULLEXT} =
###     $self->{NAME}) =~ s!::!/!g ; #eg. BSD/Foo/Socket
    $self->{FULLEXT} = File::Spec->catdir(split /::/, $self->{NAME});


    # Copied from DynaLoader:

    my(@modparts) = split(/::/,$self->{NAME});
    my($modfname) = $modparts[-1];

    # Some systems have restrictions on files names for DLL's etc.
    # mod2fname returns appropriate file base name (typically truncated)
    # It may also edit @modparts if required.
    if (defined &DynaLoader::mod2fname) {
        $modfname = &DynaLoader::mod2fname(\@modparts);
    }

    ($self->{PARENT_NAME}, $self->{BASEEXT}) = $self->{NAME} =~ m!(?:([\w:]+)::)?(\w+)\z! ;
    $self->{PARENT_NAME} ||= '';

    if (defined &DynaLoader::mod2fname) {
	# As of 5.001m, dl_os2 appends '_'
	$self->{DLBASE} = $modfname;
    } else {
	$self->{DLBASE} = '$(BASEEXT)';
    }


    # --- Initialize PERL_LIB, PERL_SRC

    # *Real* information: where did we get these two from? ...
    my $inc_config_dir = dirname($INC{'Config.pm'});
    my $inc_carp_dir   = dirname($INC{'Carp.pm'});

    unless ($self->{PERL_SRC}){
	my($dir);
	foreach $dir ($Updir,
                      File::Spec->catdir($Updir,$Updir),
                      File::Spec->catdir($Updir,$Updir,$Updir),
                      File::Spec->catdir($Updir,$Updir,$Updir,$Updir),
                      File::Spec->catdir($Updir,$Updir,$Updir,$Updir,$Updir))
        {
	    if (
		-f File::Spec->catfile($dir,"config_h.SH")
		&&
		-f File::Spec->catfile($dir,"perl.h")
		&&
		-f File::Spec->catfile($dir,"lib","Exporter.pm")
	       ) {
		$self->{PERL_SRC}=$dir ;
		last;
	    }
	}
    }

    warn "PERL_CORE is set but I can't find your PERL_SRC!\n" if
      $self->{PERL_CORE} and !$self->{PERL_SRC};

    if ($self->{PERL_SRC}){
	$self->{PERL_LIB}     ||= File::Spec->catdir("$self->{PERL_SRC}","lib");

        if (defined $Cross::platform) {
            $self->{PERL_ARCHLIB} = 
              File::Spec->catdir("$self->{PERL_SRC}","xlib",$Cross::platform);
            $self->{PERL_INC}     = 
              File::Spec->catdir("$self->{PERL_SRC}","xlib",$Cross::platform, 
                                 $Is_Win32?("CORE"):());
        }
        else {
            $self->{PERL_ARCHLIB} = $self->{PERL_LIB};
            $self->{PERL_INC}     = ($Is_Win32) ? 
              File::Spec->catdir($self->{PERL_LIB},"CORE") : $self->{PERL_SRC};
        }

	# catch a situation that has occurred a few times in the past:
	unless (
		-s File::Spec->catfile($self->{PERL_SRC},'cflags')
		or
		$Is_VMS
		&&
		-s File::Spec->catfile($self->{PERL_SRC},'perlshr_attr.opt')
		or
		$Is_Mac
		or
		$Is_Win32
	       ){
	    warn qq{
You cannot build extensions below the perl source tree after executing
a 'make clean' in the perl source tree.

To rebuild extensions distributed with the perl source you should
simply Configure (to include those extensions) and then build perl as
normal. After installing perl the source tree can be deleted. It is
not needed for building extensions by running 'perl Makefile.PL'
usually without extra arguments.

It is recommended that you unpack and build additional extensions away
from the perl source tree.
};
	}
    } else {
	# we should also consider $ENV{PERL5LIB} here
        my $old = $self->{PERL_LIB} || $self->{PERL_ARCHLIB} || $self->{PERL_INC};
	$self->{PERL_LIB}     ||= $Config{privlibexp};
	$self->{PERL_ARCHLIB} ||= $Config{archlibexp};
	$self->{PERL_INC}     = File::Spec->catdir("$self->{PERL_ARCHLIB}","CORE"); # wild guess for now
	my $perl_h;

	if (not -f ($perl_h = File::Spec->catfile($self->{PERL_INC},"perl.h"))
	    and not $old){
	    # Maybe somebody tries to build an extension with an
	    # uninstalled Perl outside of Perl build tree
	    my $found;
	    for my $dir (@INC) {
	      $found = $dir, last if -e File::Spec->catdir($dir, "Config.pm");
	    }
	    if ($found) {
	      my $inc = dirname $found;
	      if (-e File::Spec->catdir($inc, "perl.h")) {
		$self->{PERL_LIB}	   = $found;
		$self->{PERL_ARCHLIB}	   = $found;
		$self->{PERL_INC}	   = $inc;
		$self->{UNINSTALLED_PERL}  = 1;
		print STDOUT <<EOP;
... Detected uninstalled Perl.  Trying to continue.
EOP
	      }
	    }
	}
	
	unless(-f ($perl_h = File::Spec->catfile($self->{PERL_INC},"perl.h")))
        {
	    die qq{
Error: Unable to locate installed Perl libraries or Perl source code.

It is recommended that you install perl in a standard location before
building extensions. Some precompiled versions of perl do not contain
these header files, so you cannot build extensions. In such a case,
please build and install your perl from a fresh perl distribution. It
usually solves this kind of problem.

\(You get this message, because MakeMaker could not find "$perl_h"\)
};
	}
#	 print STDOUT "Using header files found in $self->{PERL_INC}\n"
#	     if $Verbose && $self->needs_linking();

    }

    # We get SITELIBEXP and SITEARCHEXP directly via
    # Get_from_Config. When we are running standard modules, these
    # won't matter, we will set INSTALLDIRS to "perl". Otherwise we
    # set it to "site". I prefer that INSTALLDIRS be set from outside
    # MakeMaker.
    $self->{INSTALLDIRS} ||= "site";


    $self->init_INST;
    $self->init_INSTALL;

    $self->{MAN1EXT} ||= $Config{man1ext};
    $self->{MAN3EXT} ||= $Config{man3ext};

    # Get some stuff out of %Config if we haven't yet done so
    print STDOUT "CONFIG must be an array ref\n"
	if ($self->{CONFIG} and ref $self->{CONFIG} ne 'ARRAY');
    $self->{CONFIG} = [] unless (ref $self->{CONFIG});
    push(@{$self->{CONFIG}}, @ExtUtils::MakeMaker::Get_from_Config);
    push(@{$self->{CONFIG}}, 'shellflags') if $Config{shellflags};
    my(%once_only,$m);
    foreach $m (@{$self->{CONFIG}}){
	next if $once_only{$m};
	print STDOUT "CONFIG key '$m' does not exist in Config.pm\n"
		unless exists $Config{$m};
	$self->{uc $m} ||= $Config{$m};
	$once_only{$m} = 1;
    }

# This is too dangerous:
#    if ($^O eq "next") {
#	$self->{AR} = "libtool";
#	$self->{AR_STATIC_ARGS} = "-o";
#    }
# But I leave it as a placeholder

    $self->{AR_STATIC_ARGS} ||= "cr";

    # These should never be needed
    $self->{LD} ||= 'ld';
    $self->{OBJ_EXT} ||= '.o';
    $self->{LIB_EXT} ||= '.a';

    $self->{MAP_TARGET} ||= "perl";

    $self->{LIBPERL_A} ||= "libperl$self->{LIB_EXT}";

    # make a simple check if we find Exporter
    warn "Warning: PERL_LIB ($self->{PERL_LIB}) seems not to be a perl library directory
        (Exporter.pm not found)"
	unless -f File::Spec->catfile("$self->{PERL_LIB}","Exporter.pm") ||
        $self->{NAME} eq "ExtUtils::MakeMaker";

    # Determine VERSION and VERSION_FROM
    ($self->{DISTNAME}=$self->{NAME}) =~ s#(::)#-#g unless $self->{DISTNAME};
    if ($self->{VERSION_FROM}){
	$self->{VERSION} = $self->parse_version($self->{VERSION_FROM});
        if( $self->{VERSION} eq 'undef' ) {
	    carp "WARNING: Setting VERSION via file ".
                 "'$self->{VERSION_FROM}' failed\n";
        }
    }

    # strip blanks
    if (defined $self->{VERSION}) {
	$self->{VERSION} =~ s/^\s+//;
	$self->{VERSION} =~ s/\s+$//;
    }
    else {
        $self->{VERSION} = '';
    }
    ($self->{VERSION_SYM} = $self->{VERSION}) =~ s/\W/_/g;

    $self->{DISTVNAME} = "$self->{DISTNAME}-$self->{VERSION}";

    # Graham Barr and Paul Marquess had some ideas how to ensure
    # version compatibility between the *.pm file and the
    # corresponding *.xs file. The bottomline was, that we need an
    # XS_VERSION macro that defaults to VERSION:
    $self->{XS_VERSION} ||= $self->{VERSION};


    # --- Initialize Perl Binary Locations
    $self->init_PERL;
}

=item init_others

Initializes EXTRALIBS, BSLOADLIBS, LDLOADLIBS, LIBS, LD_RUN_PATH,
OBJECT, BOOTDEP, PERLMAINCC, LDFROM, LINKTYPE, NOOP, FIRST_MAKEFILE,
MAKEFILE, NOECHO, RM_F, RM_RF, TEST_F, TOUCH, CP, MV, CHMOD, UMASK_NULL

=cut

sub init_others {	# --- Initialize Other Attributes
    my($self) = shift;

    # Compute EXTRALIBS, BSLOADLIBS and LDLOADLIBS from $self->{LIBS}
    # Lets look at $self->{LIBS} carefully: It may be an anon array, a string or
    # undefined. In any case we turn it into an anon array:

    # May check $Config{libs} too, thus not empty.
    $self->{LIBS}=[''] unless $self->{LIBS};

    $self->{LIBS}=[$self->{LIBS}] if ref \$self->{LIBS} eq 'SCALAR';
    $self->{LD_RUN_PATH} = "";
    my($libs);
    foreach $libs ( @{$self->{LIBS}} ){
	$libs =~ s/^\s*(.*\S)\s*$/$1/; # remove leading and trailing whitespace
	my(@libs) = $self->extliblist($libs);
	if ($libs[0] or $libs[1] or $libs[2]){
	    # LD_RUN_PATH now computed by ExtUtils::Liblist
	    ($self->{EXTRALIBS},  $self->{BSLOADLIBS}, 
             $self->{LDLOADLIBS}, $self->{LD_RUN_PATH}) = @libs;
	    last;
	}
    }

    if ( $self->{OBJECT} ) {
	$self->{OBJECT} =~ s!\.o(bj)?\b!\$(OBJ_EXT)!g;
    } else {
	# init_dirscan should have found out, if we have C files
	$self->{OBJECT} = "";
	$self->{OBJECT} = '$(BASEEXT)$(OBJ_EXT)' if @{$self->{C}||[]};
    }
    $self->{OBJECT} =~ s/\n+/ \\\n\t/g;
    $self->{BOOTDEP}  = (-f "$self->{BASEEXT}_BS") ? "$self->{BASEEXT}_BS" : "";
    $self->{PERLMAINCC} ||= '$(CC)';
    $self->{LDFROM} = '$(OBJECT)' unless $self->{LDFROM};

    # Sanity check: don't define LINKTYPE = dynamic if we're skipping
    # the 'dynamic' section of MM.  We don't have this problem with
    # 'static', since we either must use it (%Config says we can't
    # use dynamic loading) or the caller asked for it explicitly.
    if (!$self->{LINKTYPE}) {
       $self->{LINKTYPE} = $self->{SKIPHASH}{'dynamic'}
                        ? 'static'
                        : ($Config{usedl} ? 'dynamic' : 'static');
    };

    # These get overridden for VMS and maybe some other systems
    $self->{NOOP}  ||= '$(SHELL) -c true';
    $self->{FIRST_MAKEFILE} ||= "Makefile";
    $self->{MAKEFILE} ||= $self->{FIRST_MAKEFILE};
    $self->{MAKE_APERL_FILE} ||= "Makefile.aperl";
    $self->{NOECHO} = '@' unless defined $self->{NOECHO};
    $self->{RM_F}  ||= "rm -f";
    $self->{RM_RF} ||= "rm -rf";
    $self->{TOUCH} ||= "touch";
    $self->{TEST_F} ||= "test -f";
    $self->{CP} ||= "cp";
    $self->{MV} ||= "mv";
    $self->{CHMOD} ||= "chmod";
    $self->{UMASK_NULL} ||= "umask 0";
    $self->{DEV_NULL} ||= "> /dev/null 2>&1";
}

=item init_INST

    $mm->init_INST;

Called by init_main.  Sets up all INST_* variables.

=cut

sub init_INST {
    my($self) = shift;

    $self->{INST_ARCHLIB} ||= File::Spec->catdir($Curdir,"blib","arch");
    $self->{INST_BIN}     ||= File::Spec->catdir($Curdir,'blib','bin');

    # INST_LIB typically pre-set if building an extension after
    # perl has been built and installed. Setting INST_LIB allows
    # you to build directly into, say $Config{privlibexp}.
    unless ($self->{INST_LIB}){
	if ($self->{PERL_CORE}) {
            if (defined $Cross::platform) {
                $self->{INST_LIB} = $self->{INST_ARCHLIB} = 
                  File::Spec->catdir($self->{PERL_LIB},"..","xlib",
                                     $Cross::platform);
            }
            else {
                $self->{INST_LIB} = $self->{INST_ARCHLIB} = $self->{PERL_LIB};
            }
	} else {
	    $self->{INST_LIB} = File::Spec->catdir($Curdir,"blib","lib");
	}
    }

    my @parentdir = split(/::/, $self->{PARENT_NAME});
    $self->{INST_LIBDIR} = File::Spec->catdir($self->{INST_LIB},@parentdir);
    $self->{INST_ARCHLIBDIR} = File::Spec->catdir($self->{INST_ARCHLIB},
                                                  @parentdir);
    $self->{INST_AUTODIR} = File::Spec->catdir($self->{INST_LIB},'auto',
                                               $self->{FULLEXT});
    $self->{INST_ARCHAUTODIR} = File::Spec->catdir($self->{INST_ARCHLIB},
                                                   'auto',$self->{FULLEXT});

    $self->{INST_SCRIPT} ||= File::Spec->catdir($Curdir,'blib','script');

    $self->{INST_MAN1DIR} ||= File::Spec->catdir($Curdir,'blib','man1');
    $self->{INST_MAN3DIR} ||= File::Spec->catdir($Curdir,'blib','man3');

    return 1;
}

=item init_INSTALL

    $mm->init_INSTALL;

Called by init_main.  Sets up all INSTALL_* variables (except
INSTALLDIRS) and PREFIX.

=cut

sub init_INSTALL {
    my($self) = shift;

    $self->init_lib2arch;

    if( $Config{usevendorprefix} ) {
        $Config_Override{installvendorman1dir} =
          File::Spec->catdir($Config{vendorprefixexp}, 'man', 'man$(MAN1EXT)');
        $Config_Override{installvendorman3dir} =
          File::Spec->catdir($Config{vendorprefixexp}, 'man', 'man$(MAN3EXT)');
    }
    else {
        $Config_Override{installvendorman1dir} = '';
        $Config_Override{installvendorman3dir} = '';
    }

    my $iprefix = $Config{installprefixexp} || $Config{installprefix} || 
                  $Config{prefixexp}        || $Config{prefix} || '';
    my $vprefix = $Config{usevendorprefix}  ? $Config{vendorprefixexp} : '';
    my $sprefix = $Config{siteprefixexp}    || '';

    # 5.005_03 doesn't have a siteprefix.
    $sprefix = $iprefix unless $sprefix;

    # There are often no Config.pm defaults for these, but we can make
    # it up.
    unless( $Config{installsiteman1dir} ) {
        $Config_Override{installsiteman1dir} = 
          File::Spec->catdir($sprefix, 'man', 'man$(MAN1EXT)');
    }

    unless( $Config{installsiteman3dir} ) {
        $Config_Override{installsiteman3dir} = 
          File::Spec->catdir($sprefix, 'man', 'man$(MAN3EXT)');
    }

    unless( $Config{installsitebin} ) {
        $Config_Override{installsitebin} =
          File::Spec->catdir($sprefix, 'bin');
    }

    my $u_prefix  = $self->{PREFIX}       || '';
    my $u_sprefix = $self->{SITEPREFIX}   || $u_prefix;
    my $u_vprefix = $self->{VENDORPREFIX} || $u_prefix;

    $self->{PREFIX}       ||= $u_prefix  || $iprefix;
    $self->{SITEPREFIX}   ||= $u_sprefix || $sprefix;
    $self->{VENDORPREFIX} ||= $u_vprefix || $vprefix;

    my $arch    = $Config{archname};
    my $version = $Config{version};

    # default style
    my $libstyle = 'lib/perl5';
    my $manstyle = '';

    if( $self->{LIBSTYLE} ) {
        $libstyle = $self->{LIBSTYLE};
        $manstyle = $self->{LIBSTYLE} eq 'lib/perl5' ? 'lib/perl5' : '';
    }

    # Some systems, like VOS, set installman*dir to '' if they can't
    # read man pages.
    for my $num (1, 3) {
        $self->{'INSTALLMAN'.$num.'DIR'} ||= 'none'
          unless $Config{'installman'.$num.'dir'};
    }

    my %bin_layouts = 
    (
        bin         => { s => $iprefix,
                         r => $u_prefix,
                         d => 'bin' },
        vendorbin   => { s => $vprefix,
                         r => $u_vprefix,
                         d => 'bin' },
        sitebin     => { s => $sprefix,
                         r => $u_sprefix,
                         d => 'bin' },
        script      => { s => $iprefix,
                         r => $u_prefix,
                         d => 'bin' },
    );
    
    my %man_layouts =
    (
        man1dir         => { s => $iprefix,
                             r => $u_prefix,
                             d => 'man/man$(MAN1EXT)',
                             style => $manstyle, },
        siteman1dir     => { s => $sprefix,
                             r => $u_sprefix,
                             d => 'man/man$(MAN1EXT)',
                             style => $manstyle, },
        vendorman1dir   => { s => $vprefix,
                             r => $u_vprefix,
                             d => 'man/man$(MAN1EXT)',
                             style => $manstyle, },

        man3dir         => { s => $iprefix,
                             r => $u_prefix,
                             d => 'man/man$(MAN3EXT)',
                             style => $manstyle, },
        siteman3dir     => { s => $sprefix,
                             r => $u_sprefix,
                             d => 'man/man$(MAN3EXT)',
                             style => $manstyle, },
        vendorman3dir   => { s => $vprefix,
                             r => $u_vprefix,
                             d => 'man/man$(MAN3EXT)',
                             style => $manstyle, },
    );

    my %lib_layouts =
    (
        privlib     => { s => $iprefix,
                         r => $u_prefix,
                         d => '',
                         style => $libstyle, },
        vendorlib   => { s => $vprefix,
                         r => $u_vprefix,
                         d => '',
                         style => $libstyle, },
        sitelib     => { s => $sprefix,
                         r => $u_sprefix,
                         d => 'site_perl',
                         style => $libstyle, },
        
        archlib     => { s => $iprefix,
                         r => $u_prefix,
                         d => "$version/$arch",
                         style => $libstyle },
        vendorarch  => { s => $vprefix,
                         r => $u_vprefix,
                         d => "$version/$arch",
                         style => $libstyle },
        sitearch    => { s => $sprefix,
                         r => $u_sprefix,
                         d => "site_perl/$version/$arch",
                         style => $libstyle },
    );


    # Special case for LIB.
    if( $self->{LIB} ) {
        foreach my $var (keys %lib_layouts) {
            my $Installvar = uc "install$var";

            if( $var =~ /arch/ ) {
                $self->{$Installvar} ||= 
                  File::Spec->catdir($self->{LIB}, $Config{archname});
            }
            else {
                $self->{$Installvar} ||= $self->{LIB};
            }
        }
    }


    my %layouts = (%bin_layouts, %man_layouts, %lib_layouts);
    while( my($var, $layout) = each(%layouts) ) {
        my($s, $r, $d, $style) = @{$layout}{qw(s r d style)};

        print STDERR "Prefixing $var\n" if $Verbose >= 2;

        my $installvar = "install$var";
        my $Installvar = uc $installvar;
        next if $self->{$Installvar};

        if( $r ) {
            $d = "$style/$d" if $style;
            $self->prefixify($installvar, $s, $r, $d);
        }
        else {
            $self->{$Installvar} = $Config_Override{$installvar} || 
                                   $Config{$installvar};
        }

        print STDERR "  $Installvar == $self->{$Installvar}\n" 
          if $Verbose >= 2;
    }

    return 1;
}

=begin _protected

=item init_lib2arch

    $mm->init_lib2arch

=end _protected

=cut

sub init_lib2arch {
    my($self) = shift;

    # The user who requests an installation directory explicitly
    # should not have to tell us an architecture installation directory
    # as well. We look if a directory exists that is named after the
    # architecture. If not we take it as a sign that it should be the
    # same as the requested installation directory. Otherwise we take
    # the found one.
    for my $libpair ({l=>"privlib",   a=>"archlib"}, 
                     {l=>"sitelib",   a=>"sitearch"},
                     {l=>"vendorlib", a=>"vendorarch"},
                    )
    {
        my $lib = "install$libpair->{l}";
        my $Lib = uc $lib;
        my $Arch = uc "install$libpair->{a}";
        if( $self->{$Lib} && ! $self->{$Arch} ){
            my($ilib) = $Config{$lib};

            $self->prefixify($Arch,$ilib,$self->{$Lib});

            unless (-d $self->{$Arch}) {
                print STDOUT "Directory $self->{$Arch} not found\n" 
                  if $Verbose;
                $self->{$Arch} = $self->{$Lib};
            }
            print STDOUT "Defaulting $Arch to $self->{$Arch}\n" if $Verbose;
        }
    }
}


=item init_PERL

    $mm->init_PERL;

Called by init_main.  Sets up ABSPERL, PERL, FULLPERL and all the
*PERLRUN* permutations.

    PERL is allowed to be miniperl
    FULLPERL must be a complete perl
    ABSPERL is PERL converted to an absolute path

    *PERLRUN contains everything necessary to run perl, find it's
         libraries, etc...

    *PERLRUNINST is *PERLRUN + everything necessary to find the
         modules being built.

=cut

sub init_PERL {
    my($self) = shift;

    my @defpath = ();
    foreach my $component ($self->{PERL_SRC}, $self->path(), 
                           $Config{binexp}) 
    {
	push @defpath, $component if defined $component;
    }

    # Build up a set of file names (not command names).
    my $thisperl = File::Spec->canonpath($^X);
    $thisperl .= $Config{exe_ext} unless $thisperl =~ m/$Config{exe_ext}$/i;
    my @perls = ($thisperl);
    push @perls, map { "$_$Config{exe_ext}" }
                     ('perl', 'perl5', "perl$Config{version}");

    # miniperl has priority over all but the cannonical perl when in the
    # core.  Otherwise its a last resort.
    my $miniperl = "miniperl$Config{exe_ext}";
    if( $self->{PERL_CORE} ) {
        splice @perls, 1, 0, $miniperl;
    }
    else {
        push @perls, $miniperl;
    }

    $self->{PERL} ||=
        $self->find_perl(5.0, \@perls, \@defpath, $Verbose );
    # don't check if perl is executable, maybe they have decided to
    # supply switches with perl

    # Define 'FULLPERL' to be a non-miniperl (used in test: target)
    ($self->{FULLPERL} = $self->{PERL}) =~ s/miniperl/perl/i
	unless $self->{FULLPERL};

    # Little hack to get around VMS's find_perl putting "MCR" in front
    # sometimes.
    $self->{ABSPERL} = $self->{PERL};
    my $has_mcr = $self->{ABSPERL} =~ s/^MCR\s*//;
    if( File::Spec->file_name_is_absolute($self->{ABSPERL}) ) {
        $self->{ABSPERL} = '$(PERL)';
    }
    else {
        $self->{ABSPERL} = File::Spec->rel2abs($self->{ABSPERL});
        $self->{ABSPERL} = 'MCR '.$self->{ABSPERL} if $has_mcr;
    }

    # Are we building the core?
    $self->{PERL_CORE} = 0 unless exists $self->{PERL_CORE};

    # How do we run perl?
    foreach my $perl (qw(PERL FULLPERL ABSPERL)) {
        $self->{$perl.'RUN'}  = "\$($perl)";

        # Make sure perl can find itself before it's installed.
        $self->{$perl.'RUN'} .= q{ "-I$(PERL_LIB)" "-I$(PERL_ARCHLIB)"} 
          if $self->{UNINSTALLED_PERL} || $self->{PERL_CORE};

        $self->{$perl.'RUNINST'} = 
          sprintf q{$(%sRUN) "-I$(INST_ARCHLIB)" "-I$(INST_LIB)"}, $perl;
    }

    return 1;
}

=item init_PERM

  $mm->init_PERM

Called by init_main.  Initializes PERL_*

=cut

sub init_PERM {
    my($self) = shift;

    $self->{PERM_RW}  = 644;
    $self->{PERM_RWX} = 755;

    return 1;
}
    

=item install (o)

Defines the install target.

=cut

sub install {
    my($self, %attribs) = @_;
    my(@m);

    push @m, q{
install :: all pure_install doc_install

install_perl :: all pure_perl_install doc_perl_install

install_site :: all pure_site_install doc_site_install

install_vendor :: all pure_vendor_install doc_vendor_install

pure_install :: pure_$(INSTALLDIRS)_install

doc_install :: doc_$(INSTALLDIRS)_install
	}.$self->{NOECHO}.q{echo Appending installation info to $(INSTALLARCHLIB)/perllocal.pod

pure__install : pure_site_install
	@echo INSTALLDIRS not defined, defaulting to INSTALLDIRS=site

doc__install : doc_site_install
	@echo INSTALLDIRS not defined, defaulting to INSTALLDIRS=site

pure_perl_install ::
	}.$self->{NOECHO}.q{$(MOD_INSTALL) \
		read }.File::Spec->catfile('$(PERL_ARCHLIB)','auto','$(FULLEXT)','.packlist').q{ \
		write }.File::Spec->catfile('$(INSTALLARCHLIB)','auto','$(FULLEXT)','.packlist').q{ \
		$(INST_LIB) $(INSTALLPRIVLIB) \
		$(INST_ARCHLIB) $(INSTALLARCHLIB) \
		$(INST_BIN) $(INSTALLBIN) \
		$(INST_SCRIPT) $(INSTALLSCRIPT) \
		$(INST_MAN1DIR) $(INSTALLMAN1DIR) \
		$(INST_MAN3DIR) $(INSTALLMAN3DIR)
	}.$self->{NOECHO}.q{$(WARN_IF_OLD_PACKLIST) \
		}.File::Spec->catdir('$(SITEARCHEXP)','auto','$(FULLEXT)').q{


pure_site_install ::
	}.$self->{NOECHO}.q{$(MOD_INSTALL) \
		read }.File::Spec->catfile('$(SITEARCHEXP)','auto','$(FULLEXT)','.packlist').q{ \
		write }.File::Spec->catfile('$(INSTALLSITEARCH)','auto','$(FULLEXT)','.packlist').q{ \
		$(INST_LIB) $(INSTALLSITELIB) \
		$(INST_ARCHLIB) $(INSTALLSITEARCH) \
		$(INST_BIN) $(INSTALLSITEBIN) \
		$(INST_SCRIPT) $(INSTALLSCRIPT) \
		$(INST_MAN1DIR) $(INSTALLSITEMAN1DIR) \
		$(INST_MAN3DIR) $(INSTALLSITEMAN3DIR)
	}.$self->{NOECHO}.q{$(WARN_IF_OLD_PACKLIST) \
		}.File::Spec->catdir('$(PERL_ARCHLIB)','auto','$(FULLEXT)').q{

pure_vendor_install ::
	}.$self->{NOECHO}.q{$(MOD_INSTALL) \
		$(INST_LIB) $(INSTALLVENDORLIB) \
		$(INST_ARCHLIB) $(INSTALLVENDORARCH) \
		$(INST_BIN) $(INSTALLVENDORBIN) \
		$(INST_SCRIPT) $(INSTALLSCRIPT) \
		$(INST_MAN1DIR) $(INSTALLVENDORMAN1DIR) \
		$(INST_MAN3DIR) $(INSTALLVENDORMAN3DIR)

doc_perl_install ::
	-}.$self->{NOECHO}.q{$(MKPATH) $(INSTALLARCHLIB)
	-}.$self->{NOECHO}.q{$(DOC_INSTALL) \
		"Module" "$(NAME)" \
		"installed into" "$(INSTALLPRIVLIB)" \
		LINKTYPE "$(LINKTYPE)" \
		VERSION "$(VERSION)" \
		EXE_FILES "$(EXE_FILES)" \
		>> }.File::Spec->catfile('$(INSTALLARCHLIB)','perllocal.pod').q{

doc_site_install ::
	-}.$self->{NOECHO}.q{$(MKPATH) $(INSTALLARCHLIB)
	-}.$self->{NOECHO}.q{$(DOC_INSTALL) \
		"Module" "$(NAME)" \
		"installed into" "$(INSTALLSITELIB)" \
		LINKTYPE "$(LINKTYPE)" \
		VERSION "$(VERSION)" \
		EXE_FILES "$(EXE_FILES)" \
		>> }.File::Spec->catfile('$(INSTALLSITEARCH)','perllocal.pod').q{

doc_vendor_install ::

};

    push @m, q{
uninstall :: uninstall_from_$(INSTALLDIRS)dirs

uninstall_from_perldirs ::
	}.$self->{NOECHO}.
	q{$(UNINSTALL) }.File::Spec->catfile('$(PERL_ARCHLIB)','auto','$(FULLEXT)','.packlist').q{

uninstall_from_sitedirs ::
	}.$self->{NOECHO}.
	q{$(UNINSTALL) }.File::Spec->catfile('$(SITEARCHEXP)','auto','$(FULLEXT)','.packlist').q{
};

    join("",@m);
}

=item installbin (o)

Defines targets to make and to install EXE_FILES.

=cut

sub installbin {
    my($self) = shift;
    return "" unless $self->{EXE_FILES} && ref $self->{EXE_FILES} eq "ARRAY";
    return "" unless @{$self->{EXE_FILES}};
    my(@m, $from, $to, %fromto, @to);
    push @m, $self->dir_target(qw[$(INST_SCRIPT)]);
    for $from (@{$self->{EXE_FILES}}) {
	my($path)= File::Spec->catfile('$(INST_SCRIPT)', basename($from));
	local($_) = $path; # for backwards compatibility
	$to = $self->libscan($path);
	print "libscan($from) => '$to'\n" if ($Verbose >=2);
	$fromto{$from}=$to;
    }
    @to   = values %fromto;
    push(@m, qq{
EXE_FILES = @{$self->{EXE_FILES}}

} . ($Is_Win32
  ? q{FIXIN = pl2bat.bat
} : q{FIXIN = $(PERLRUN) "-MExtUtils::MY" \
    -e "MY->fixin(shift)"
}).qq{
pure_all :: @to
	$self->{NOECHO}\$(NOOP)

realclean ::
	$self->{RM_F} @to
});

    while (($from,$to) = each %fromto) {
	last unless defined $from;
	my $todir = dirname($to);
	push @m, "
$to: $from $self->{MAKEFILE} " . File::Spec->catdir($todir,'.exists') . "
	$self->{NOECHO}$self->{RM_F} $to
	$self->{CP} $from $to
	\$(FIXIN) $to
	-$self->{NOECHO}\$(CHMOD) \$(PERM_RWX) $to
";
    }
    join "", @m;
}

=item libscan (o)

Takes a path to a file that is found by init_dirscan and returns false
if we don't want to include this file in the library. Mainly used to
exclude RCS, CVS, and SCCS directories from installation.

=cut

# ';

sub libscan {
    my($self,$path) = @_;
    return '' if $path =~ m:\b(RCS|CVS|SCCS)\b: ;
    $path;
}

=item linkext (o)

Defines the linkext target which in turn defines the LINKTYPE.

=cut

sub linkext {
    my($self, %attribs) = @_;
    # LINKTYPE => static or dynamic or ''
    my($linktype) = defined $attribs{LINKTYPE} ?
      $attribs{LINKTYPE} : '$(LINKTYPE)';
    "
linkext :: $linktype
	$self->{NOECHO}\$(NOOP)
";
}

=item lsdir

Takes as arguments a directory name and a regular expression. Returns
all entries in the directory that match the regular expression.

=cut

sub lsdir {
    my($self) = shift;
    my($dir, $regex) = @_;
    my(@ls);
    my $dh = new DirHandle;
    $dh->open($dir || ".") or return ();
    @ls = $dh->read;
    $dh->close;
    @ls = grep(/$regex/, @ls) if $regex;
    @ls;
}

=item macro (o)

Simple subroutine to insert the macros defined by the macro attribute
into the Makefile.

=cut

sub macro {
    my($self,%attribs) = @_;
    my(@m,$key,$val);
    while (($key,$val) = each %attribs){
	last unless defined $key;
	push @m, "$key = $val\n";
    }
    join "", @m;
}

=item makeaperl (o)

Called by staticmake. Defines how to write the Makefile to produce a
static new perl.

By default the Makefile produced includes all the static extensions in
the perl library. (Purified versions of library files, e.g.,
DynaLoader_pure_p1_c0_032.a are automatically ignored to avoid link errors.)

=cut

sub makeaperl {
    my($self, %attribs) = @_;
    my($makefilename, $searchdirs, $static, $extra, $perlinc, $target, $tmp, $libperl) =
	@attribs{qw(MAKE DIRS STAT EXTRA INCL TARGET TMP LIBPERL)};
    my(@m);
    push @m, "
# --- MakeMaker makeaperl section ---
MAP_TARGET    = $target
FULLPERL      = $self->{FULLPERL}
";
    return join '', @m if $self->{PARENT};

    my($dir) = join ":", @{$self->{DIR}};

    unless ($self->{MAKEAPERL}) {
	push @m, q{
$(MAP_TARGET) :: static $(MAKE_APERL_FILE)
	$(MAKE) -f $(MAKE_APERL_FILE) $@

$(MAKE_APERL_FILE) : $(FIRST_MAKEFILE)
	}.$self->{NOECHO}.q{echo Writing \"$(MAKE_APERL_FILE)\" for this $(MAP_TARGET)
	}.$self->{NOECHO}.q{$(PERLRUNINST) \
		Makefile.PL DIR=}, $dir, q{ \
		MAKEFILE=$(MAKE_APERL_FILE) LINKTYPE=static \
		MAKEAPERL=1 NORECURS=1 CCCDLFLAGS=};

	foreach (@ARGV){
		if( /\s/ ){
			s/=(.*)/='$1'/;
		}
		push @m, " \\\n\t\t$_";
	}
#	push @m, map( " \\\n\t\t$_", @ARGV );
	push @m, "\n";

	return join '', @m;
    }



    my($cccmd, $linkcmd, $lperl);


    $cccmd = $self->const_cccmd($libperl);
    $cccmd =~ s/^CCCMD\s*=\s*//;
    $cccmd =~ s/\$\(INC\)/ "-I$self->{PERL_INC}" /;
    $cccmd .= " $Config{cccdlflags}"
	if ($Config{useshrplib} eq 'true');
    $cccmd =~ s/\(CC\)/\(PERLMAINCC\)/;

    # The front matter of the linkcommand...
    $linkcmd = join ' ', "\$(CC)",
	    grep($_, @Config{qw(ldflags ccdlflags)});
    $linkcmd =~ s/\s+/ /g;
    $linkcmd =~ s,(perl\.exp),\$(PERL_INC)/$1,;

    # Which *.a files could we make use of...
    local(%static);
    require File::Find;
    File::Find::find(sub {
	return unless m/\Q$self->{LIB_EXT}\E$/;
	return if m/^libperl/;
	# Skip purified versions of libraries (e.g., DynaLoader_pure_p1_c0_032.a)
	return if m/_pure_\w+_\w+_\w+\.\w+$/ and -f "$File::Find::dir/.pure";

	if( exists $self->{INCLUDE_EXT} ){
		my $found = 0;
		my $incl;
		my $xx;

		($xx = $File::Find::name) =~ s,.*?/auto/,,s;
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

		($xx = $File::Find::name) =~ s,.*?/auto/,,s;
		$xx =~ s,/?$_,,;
		$xx =~ s,/,::,g;

		# Throw away anything explicitly marked for exclusion
		foreach $excl (@{$self->{EXCLUDE_EXT}}){
			return if( $xx eq $excl );
		}
	}

	# don't include the installed version of this extension. I
	# leave this line here, although it is not necessary anymore:
	# I patched minimod.PL instead, so that Miniperl.pm won't
	# enclude duplicates

	# Once the patch to minimod.PL is in the distribution, I can
	# drop it
	return if $File::Find::name =~ m:auto/$self->{FULLEXT}/$self->{BASEEXT}$self->{LIB_EXT}\z:;
	use Cwd 'cwd';
	$static{cwd() . "/" . $_}++;
    }, grep( -d $_, @{$searchdirs || []}) );

    # We trust that what has been handed in as argument, will be buildable
    $static = [] unless $static;
    @static{@{$static}} = (1) x @{$static};

    $extra = [] unless $extra && ref $extra eq 'ARRAY';
    for (sort keys %static) {
	next unless /\Q$self->{LIB_EXT}\E\z/;
	$_ = dirname($_) . "/extralibs.ld";
	push @$extra, $_;
    }

    grep(s/^(.*)/"-I$1"/, @{$perlinc || []});

    $target ||= "perl";
    $tmp    ||= ".";

# MAP_STATIC doesn't look into subdirs yet. Once "all" is made and we
# regenerate the Makefiles, MAP_STATIC and the dependencies for
# extralibs.all are computed correctly
    push @m, "
MAP_LINKCMD   = $linkcmd
MAP_PERLINC   = @{$perlinc || []}
MAP_STATIC    = ",
join(" \\\n\t", reverse sort keys %static), "

MAP_PRELIBS   = $Config{perllibs} $Config{cryptlib}
";

    if (defined $libperl) {
	($lperl = $libperl) =~ s/\$\(A\)/$self->{LIB_EXT}/;
    }
    unless ($libperl && -f $lperl) { # Ilya's code...
	my $dir = $self->{PERL_SRC} || "$self->{PERL_ARCHLIB}/CORE";
	$dir = "$self->{PERL_ARCHLIB}/.." if $self->{UNINSTALLED_PERL};
	$libperl ||= "libperl$self->{LIB_EXT}";
	$libperl   = "$dir/$libperl";
	$lperl   ||= "libperl$self->{LIB_EXT}";
	$lperl     = "$dir/$lperl";

        if (! -f $libperl and ! -f $lperl) {
          # We did not find a static libperl. Maybe there is a shared one?
          if ($^O eq 'solaris' or $^O eq 'sunos') {
            $lperl  = $libperl = "$dir/$Config{libperl}";
            # SUNOS ld does not take the full path to a shared library
            $libperl = '' if $^O eq 'sunos';
          }
        }

	print STDOUT "Warning: $libperl not found
    If you're going to build a static perl binary, make sure perl is installed
    otherwise ignore this warning\n"
		unless (-f $lperl || defined($self->{PERL_SRC}));
    }

    # SUNOS ld does not take the full path to a shared library
    my $llibperl = $libperl ? '$(MAP_LIBPERL)' : '-lperl';

    push @m, "
MAP_LIBPERL = $libperl
LLIBPERL    = $llibperl
";

    push @m, "
\$(INST_ARCHAUTODIR)/extralibs.all: \$(INST_ARCHAUTODIR)/.exists ".join(" \\\n\t", @$extra)."
	$self->{NOECHO}$self->{RM_F} \$\@
	$self->{NOECHO}\$(TOUCH) \$\@
";

    my $catfile;
    foreach $catfile (@$extra){
	push @m, "\tcat $catfile >> \$\@\n";
    }

push @m, "
\$(MAP_TARGET) :: $tmp/perlmain\$(OBJ_EXT) \$(MAP_LIBPERL) \$(MAP_STATIC) \$(INST_ARCHAUTODIR)/extralibs.all
	\$(MAP_LINKCMD) -o \$\@ \$(OPTIMIZE) $tmp/perlmain\$(OBJ_EXT) \$(LDFROM) \$(MAP_STATIC) \$(LLIBPERL) `cat \$(INST_ARCHAUTODIR)/extralibs.all` \$(MAP_PRELIBS)
	$self->{NOECHO}echo 'To install the new \"\$(MAP_TARGET)\" binary, call'
	$self->{NOECHO}echo '    make -f $makefilename inst_perl MAP_TARGET=\$(MAP_TARGET)'
	$self->{NOECHO}echo 'To remove the intermediate files say'
	$self->{NOECHO}echo '    make -f $makefilename map_clean'

$tmp/perlmain\$(OBJ_EXT): $tmp/perlmain.c
";
    push @m, qq{\tcd $tmp && $cccmd "-I\$(PERL_INC)" perlmain.c\n};

    push @m, qq{
$tmp/perlmain.c: $makefilename}, q{
	}.$self->{NOECHO}.q{echo Writing $@
	}.$self->{NOECHO}.q{$(PERL) $(MAP_PERLINC) "-MExtUtils::Miniperl" \\
		-e "writemain(grep s#.*/auto/##s, split(q| |, q|$(MAP_STATIC)|))" > $@t && $(MV) $@t $@

};
    push @m, "\t",$self->{NOECHO}.q{$(PERL) $(INSTALLSCRIPT)/fixpmain
} if (defined (&Dos::UseLFN) && Dos::UseLFN()==0);


    push @m, q{
doc_inst_perl:
	}.$self->{NOECHO}.q{echo Appending installation info to $(INSTALLARCHLIB)/perllocal.pod
	-}.$self->{NOECHO}.q{$(MKPATH) $(INSTALLARCHLIB)
	-}.$self->{NOECHO}.q{$(DOC_INSTALL) \
		"Perl binary" "$(MAP_TARGET)" \
		MAP_STATIC "$(MAP_STATIC)" \
		MAP_EXTRA "`cat $(INST_ARCHAUTODIR)/extralibs.all`" \
		MAP_LIBPERL "$(MAP_LIBPERL)" \
		>> }.File::Spec->catfile('$(INSTALLARCHLIB)','perllocal.pod').q{

};

    push @m, q{
inst_perl: pure_inst_perl doc_inst_perl

pure_inst_perl: $(MAP_TARGET)
	}.$self->{CP}.q{ $(MAP_TARGET) }.File::Spec->catfile('$(INSTALLBIN)','$(MAP_TARGET)').q{

clean :: map_clean

map_clean :
	}.$self->{RM_F}.qq{ $tmp/perlmain\$(OBJ_EXT) $tmp/perlmain.c \$(MAP_TARGET) $makefilename \$(INST_ARCHAUTODIR)/extralibs.all
};

    join '', @m;
}

=item makefile (o)

Defines how to rewrite the Makefile.

=cut

sub makefile {
    my($self) = shift;
    my @m;
    # We do not know what target was originally specified so we
    # must force a manual rerun to be sure. But as it should only
    # happen very rarely it is not a significant problem.
    push @m, '
$(OBJECT) : $(FIRST_MAKEFILE)
' if $self->{OBJECT};

    push @m, q{
# We take a very conservative approach here, but it\'s worth it.
# We move Makefile to Makefile.old here to avoid gnu make looping.
}.$self->{MAKEFILE}.q{ : Makefile.PL $(CONFIGDEP)
	}.$self->{NOECHO}.q{echo "Makefile out-of-date with respect to $?"
	}.$self->{NOECHO}.q{echo "Cleaning current config before rebuilding Makefile..."
	-}.$self->{NOECHO}.q{$(RM_F) }."$self->{MAKEFILE}.old".q{
	-}.$self->{NOECHO}.q{$(MV) }."$self->{MAKEFILE} $self->{MAKEFILE}.old".q{
	-$(MAKE) -f }.$self->{MAKEFILE}.q{.old clean $(DEV_NULL) || $(NOOP)
	$(PERLRUN) Makefile.PL }.join(" ",map(qq["$_"],@ARGV)).q{
	}.$self->{NOECHO}.q{echo "==> Your Makefile has been rebuilt. <=="
	}.$self->{NOECHO}.q{echo "==> Please rerun the make command.  <=="
	false

};

    join "", @m;
}

=item manifypods (o)

Defines targets and routines to translate the pods into manpages and
put them into the INST_* directories.

=cut

sub manifypods {
    my($self, %attribs) = @_;
    return "\nmanifypods : pure_all\n\t$self->{NOECHO}\$(NOOP)\n" unless
	%{$self->{MAN3PODS}} or %{$self->{MAN1PODS}};
    my($dist);
    my($pod2man_exe);
    if (defined $self->{PERL_SRC}) {
	$pod2man_exe = File::Spec->catfile($self->{PERL_SRC},'pod','pod2man');
    } else {
	$pod2man_exe = File::Spec->catfile($Config{scriptdirexp},'pod2man');
    }
    unless ($pod2man_exe = $self->perl_script($pod2man_exe)) {
      # Maybe a build by uninstalled Perl?
      $pod2man_exe = File::Spec->catfile($self->{PERL_INC}, "pod", "pod2man");
    }
    unless ($pod2man_exe = $self->perl_script($pod2man_exe)) {
	# No pod2man but some MAN3PODS to be installed
	print <<END;

Warning: I could not locate your pod2man program. Please make sure,
         your pod2man program is in your PATH before you execute 'make'

END
        $pod2man_exe = "-S pod2man";
    }
    my(@m);
    push @m,
qq[POD2MAN_EXE = $pod2man_exe\n],
qq[POD2MAN = \$(PERL) -we '%m=\@ARGV;for (keys %m){' \\\n],
q[-e 'next if -e $$m{$$_} && -M $$m{$$_} < -M $$_ && -M $$m{$$_} < -M "],
 $self->{MAKEFILE}, q[";' \\
-e 'print "Manifying $$m{$$_}\n";' \\
-e 'system(q[$(PERLRUN) $(POD2MAN_EXE) ].qq[$$_>$$m{$$_}])==0 or warn "Couldn\\047t install $$m{$$_}\n";' \\
-e 'chmod(oct($(PERM_RW)), $$m{$$_}) or warn "chmod $(PERM_RW) $$m{$$_}: $$!\n";}'
];
    push @m, "\nmanifypods : pure_all ";
    push @m, join " \\\n\t", keys %{$self->{MAN1PODS}}, keys %{$self->{MAN3PODS}};

    push(@m,"\n");
    if (%{$self->{MAN1PODS}} || %{$self->{MAN3PODS}}) {
	push @m, "\t$self->{NOECHO}\$(POD2MAN) \\\n\t";
	push @m, join " \\\n\t", %{$self->{MAN1PODS}}, %{$self->{MAN3PODS}};
    }
    join('', @m);
}

=item maybe_command

Returns true, if the argument is likely to be a command.

=cut

sub maybe_command {
    my($self,$file) = @_;
    return $file if -x $file && ! -d $file;
    return;
}

=item maybe_command_in_dirs

method under development. Not yet used. Ask Ilya :-)

=cut

sub maybe_command_in_dirs {	# $ver is optional argument if looking for perl
# Ilya's suggestion. Not yet used, want to understand it first, but at least the code is here
    my($self, $names, $dirs, $trace, $ver) = @_;
    my($name, $dir);
    foreach $dir (@$dirs){
	next unless defined $dir; # $self->{PERL_SRC} may be undefined
	foreach $name (@$names){
	    my($abs,$tryabs);
	    if (File::Spec->file_name_is_absolute($name)) { # /foo/bar
		$abs = $name;
	    } elsif (File::Spec->canonpath($name) eq File::Spec->canonpath(basename($name))) { # bar
		$abs = File::Spec->catfile($dir, $name);
	    } else { # foo/bar
		$abs = File::Spec->catfile($Curdir, $name);
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

=item needs_linking (o)

Does this module need linking? Looks into subdirectory objects (see
also has_link_code())

=cut

sub needs_linking {
    my($self) = shift;
    my($child,$caller);
    $caller = (caller(0))[3];
    confess("Needs_linking called too early") if 
      $caller =~ /^ExtUtils::MakeMaker::/;
    return $self->{NEEDS_LINKING} if defined $self->{NEEDS_LINKING};
    if ($self->has_link_code or $self->{MAKEAPERL}){
	$self->{NEEDS_LINKING} = 1;
	return 1;
    }
    foreach $child (keys %{$self->{CHILDREN}}) {
	if ($self->{CHILDREN}->{$child}->needs_linking) {
	    $self->{NEEDS_LINKING} = 1;
	    return 1;
	}
    }
    return $self->{NEEDS_LINKING} = 0;
}

=item nicetext

misnamed method (will have to be changed). The MM_Unix method just
returns the argument without further processing.

On VMS used to insure that colons marking targets are preceded by
space - most Unix Makes don't need this, but it's necessary under VMS
to distinguish the target delimiter from a colon appearing as part of
a filespec.

=cut

sub nicetext {
    my($self,$text) = @_;
    $text;
}

=item parse_abstract

parse a file and return what you think is the ABSTRACT

=cut

sub parse_abstract {
    my($self,$parsefile) = @_;
    my $result;
    local *FH;
    local $/ = "\n";
    open(FH,$parsefile) or die "Could not open '$parsefile': $!";
    my $inpod = 0;
    my $package = $self->{DISTNAME};
    $package =~ s/-/::/g;
    while (<FH>) {
        $inpod = /^=(?!cut)/ ? 1 : /^=cut/ ? 0 : $inpod;
        next if !$inpod;
        chop;
        next unless /^($package\s-\s)(.*)/;
        $result = $2;
        last;
    }
    close FH;
    return $result;
}

=item parse_version

parse a file and return what you think is $VERSION in this file set to.
It will return the string "undef" if it can't figure out what $VERSION
is. $VERSION should be for all to see, so our $VERSION or plain $VERSION
are okay, but my $VERSION is not.

=cut

sub parse_version {
    my($self,$parsefile) = @_;
    my $result;
    local *FH;
    local $/ = "\n";
    open(FH,$parsefile) or die "Could not open '$parsefile': $!";
    my $inpod = 0;
    while (<FH>) {
	$inpod = /^=(?!cut)/ ? 1 : /^=cut/ ? 0 : $inpod;
	next if $inpod || /^\s*#/;
	chop;
	# next unless /\$(([\w\:\']*)\bVERSION)\b.*\=/;
	next unless /([\$*])(([\w\:\']*)\bVERSION)\b.*\=/;
	my $eval = qq{
	    package ExtUtils::MakeMaker::_version;
	    no strict;

	    local $1$2;
	    \$$2=undef; do {
		$_
	    }; \$$2
	};
        local $^W = 0;
	$result = eval($eval);
	warn "Could not eval '$eval' in $parsefile: $@" if $@;
	last;
    }
    close FH;

    $result = "undef" unless defined $result;
    return $result;
}


=item pasthru (o)

Defines the string that is passed to recursive make calls in
subdirectories.

=cut

sub pasthru {
    my($self) = shift;
    my(@m,$key);

    my(@pasthru);
    my($sep) = $Is_VMS ? ',' : '';
    $sep .= "\\\n\t";

    foreach $key (qw(LIB LIBPERL_A LINKTYPE PREFIX OPTIMIZE)) {
	push @pasthru, "$key=\"\$($key)\"";
    }

    foreach $key (qw(DEFINE INC)) {
	push @pasthru, "PASTHRU_$key=\"\$(PASTHRU_$key)\"";
    }

    push @m, "\nPASTHRU = ", join ($sep, @pasthru), "\n";
    join "", @m;
}

=item perl_script

Takes one argument, a file name, and returns the file name, if the
argument is likely to be a perl script. On MM_Unix this is true for
any ordinary, readable file.

=cut

sub perl_script {
    my($self,$file) = @_;
    return $file if -r $file && -f _;
    return;
}

=item perldepend (o)

Defines the dependency from all *.h files that come with the perl
distribution.

=cut

sub perldepend {
    my($self) = shift;
    my(@m);
    push @m, q{
# Check for unpropogated config.sh changes. Should never happen.
# We do NOT just update config.h because that is not sufficient.
# An out of date config.h is not fatal but complains loudly!
$(PERL_INC)/config.h: $(PERL_SRC)/config.sh
	-}.$self->{NOECHO}.q{echo "Warning: $(PERL_INC)/config.h out of date with $(PERL_SRC)/config.sh"; false

$(PERL_ARCHLIB)/Config.pm: $(PERL_SRC)/config.sh
	}.$self->{NOECHO}.q{echo "Warning: $(PERL_ARCHLIB)/Config.pm may be out of date with $(PERL_SRC)/config.sh"
	cd $(PERL_SRC) && $(MAKE) lib/Config.pm
} if $self->{PERL_SRC};

    return join "", @m unless $self->needs_linking;

    push @m, q{
PERL_HDRS = \
	$(PERL_INC)/EXTERN.h		\
	$(PERL_INC)/INTERN.h		\
	$(PERL_INC)/XSUB.h		\
	$(PERL_INC)/av.h		\
	$(PERL_INC)/cc_runtime.h	\
	$(PERL_INC)/config.h		\
	$(PERL_INC)/cop.h		\
	$(PERL_INC)/cv.h		\
	$(PERL_INC)/dosish.h		\
	$(PERL_INC)/embed.h		\
	$(PERL_INC)/embedvar.h		\
	$(PERL_INC)/fakethr.h		\
	$(PERL_INC)/form.h		\
	$(PERL_INC)/gv.h		\
	$(PERL_INC)/handy.h		\
	$(PERL_INC)/hv.h		\
	$(PERL_INC)/intrpvar.h		\
	$(PERL_INC)/iperlsys.h		\
	$(PERL_INC)/keywords.h		\
	$(PERL_INC)/mg.h		\
	$(PERL_INC)/nostdio.h		\
	$(PERL_INC)/op.h		\
	$(PERL_INC)/opcode.h		\
	$(PERL_INC)/opnames.h		\
	$(PERL_INC)/patchlevel.h	\
	$(PERL_INC)/perl.h		\
	$(PERL_INC)/perlapi.h		\
	$(PERL_INC)/perlio.h		\
	$(PERL_INC)/perlsdio.h		\
	$(PERL_INC)/perlsfio.h		\
	$(PERL_INC)/perlvars.h		\
	$(PERL_INC)/perly.h		\
	$(PERL_INC)/pp.h		\
	$(PERL_INC)/pp_proto.h		\
	$(PERL_INC)/proto.h		\
	$(PERL_INC)/regcomp.h		\
	$(PERL_INC)/regexp.h		\
	$(PERL_INC)/regnodes.h		\
	$(PERL_INC)/scope.h		\
	$(PERL_INC)/sv.h		\
	$(PERL_INC)/thrdvar.h		\
	$(PERL_INC)/thread.h		\
	$(PERL_INC)/unixish.h		\
	$(PERL_INC)/utf8.h		\
	$(PERL_INC)/util.h		\
	$(PERL_INC)/warnings.h

$(OBJECT) : $(PERL_HDRS)
} if $self->{OBJECT};

    push @m, join(" ", values %{$self->{XS}})." : \$(XSUBPPDEPS)\n"  if %{$self->{XS}};

    join "\n", @m;
}


=item perm_rw (o)

Returns the attribute C<PERM_RW> or the string C<644>.
Used as the string that is passed
to the C<chmod> command to set the permissions for read/writeable files.
MakeMaker chooses C<644> because it has turned out in the past that
relying on the umask provokes hard-to-track bug reports.
When the return value is used by the perl function C<chmod>, it is
interpreted as an octal value.

=cut

sub perm_rw {
    shift->{PERM_RW} || "644";
}

=item perm_rwx (o)

Returns the attribute C<PERM_RWX> or the string C<755>,
i.e. the string that is passed
to the C<chmod> command to set the permissions for executable files.
See also perl_rw.

=cut

sub perm_rwx {
    shift->{PERM_RWX} || "755";
}

=item pm_to_blib

Defines target that copies all files in the hash PM to their
destination and autosplits them. See L<ExtUtils::Install/DESCRIPTION>

=cut

sub _pm_to_blib_flush {
    my ($self, $autodir, $rr, $ra, $rl) = @_;
    $$rr .= 
q{	}.$self->{NOECHO}.q[$(PERLRUNINST) "-MExtUtils::Install" \
	-e "pm_to_blib({qw{].qq[@$ra].q[}},'].$autodir.q{','$(PM_FILTER)')"
};
    @$ra = ();
    $$rl = 0;
}

sub pm_to_blib {
    my $self = shift;
    my($autodir) = File::Spec->catdir('$(INST_LIB)','auto');
    my $r = q{
pm_to_blib: $(TO_INST_PM)
};
    my %pm_to_blib = %{$self->{PM}};
    my @a;
    my $l = 0;
    while (my ($pm, $blib) = each %pm_to_blib) {
	my $la = length $pm;
	my $lb = length $blib;
	if ($l + $la + $lb + @a / 2 > 200) { # limit line length
	    _pm_to_blib_flush($self, $autodir, \$r, \@a, \$l);
        }
        push @a, $pm, $blib;
	$l += $la + $lb;
    }
    _pm_to_blib_flush($self, $autodir, \$r, \@a, \$l);
    return $r.q{	}.$self->{NOECHO}.q{$(TOUCH) $@};
}

=item post_constants (o)

Returns an empty string per default. Dedicated to overrides from
within Makefile.PL after all constants have been defined.

=cut

sub post_constants{
    my($self) = shift;
    "";
}

=item post_initialize (o)

Returns an empty string per default. Used in Makefile.PLs to add some
chunk of text to the Makefile after the object is initialized.

=cut

sub post_initialize {
    my($self) = shift;
    "";
}

=item postamble (o)

Returns an empty string. Can be used in Makefile.PLs to write some
text to the Makefile at the end.

=cut

sub postamble {
    my($self) = shift;
    "";
}

=item ppd

Defines target that creates a PPD (Perl Package Description) file
for a binary distribution.

=cut

sub ppd {
    my($self) = @_;

    if ($self->{ABSTRACT_FROM}){
        $self->{ABSTRACT} = $self->parse_abstract($self->{ABSTRACT_FROM}) or
            carp "WARNING: Setting ABSTRACT via file ".
                 "'$self->{ABSTRACT_FROM}' failed\n";
    }

    my ($pack_ver) = join ",", (split (/\./, $self->{VERSION}), (0)x4)[0..3];

    my $abstract = $self->{ABSTRACT} || '';
    $abstract =~ s/\n/\\n/sg;
    $abstract =~ s/</&lt;/g;
    $abstract =~ s/>/&gt;/g;

    my $author = $self->{AUTHOR} || '';
    $author =~ s/</&lt;/g;
    $author =~ s/>/&gt;/g;
    $author =~ s/@/\\@/g;

    my $make_ppd = sprintf <<'PPD_OUT', $pack_ver, $abstract, $author;
# Creates a PPD (Perl Package Description) for a binary distribution.
ppd:
	@$(PERL) -e "print qq{<SOFTPKG NAME=\"$(DISTNAME)\" VERSION=\"%s\">\n\t<TITLE>$(DISTNAME)</TITLE>\n\t<ABSTRACT>%s</ABSTRACT>\n\t<AUTHOR>%s</AUTHOR>\n}" > $(DISTNAME).ppd
PPD_OUT


    $make_ppd .= '	@$(PERL) -e "print qq{\t<IMPLEMENTATION>\n';
    foreach my $prereq (sort keys %{$self->{PREREQ_PM}}) {
        my $pre_req = $prereq;
        $pre_req =~ s/::/-/g;
        my ($dep_ver) = join ",", (split (/\./, $self->{PREREQ_PM}{$prereq}), 
                                  (0) x 4) [0 .. 3];
        $make_ppd .= sprintf q{\t\t<DEPENDENCY NAME=\"%s\" VERSION=\"%s\" />\n}, $pre_req, $dep_ver;
    }
    $make_ppd .= qq[}" >> \$(DISTNAME).ppd\n];


    $make_ppd .= sprintf <<'PPD_OUT', $Config{archname};
	@$(PERL) -e "print qq{\t\t<OS NAME=\"$(OSNAME)\" />\n\t\t<ARCHITECTURE NAME=\"%s\" />\n
PPD_OUT

    chomp $make_ppd;


    if ($self->{PPM_INSTALL_SCRIPT}) {
        if ($self->{PPM_INSTALL_EXEC}) {
            $make_ppd .= sprintf q{\t\t<INSTALL EXEC=\"%s\">%s</INSTALL>\n},
                  $self->{PPM_INSTALL_EXEC}, $self->{PPM_INSTALL_SCRIPT};
        }
        else {
            $make_ppd .= sprintf q{\t\t<INSTALL>%s</INSTALL>\n}, 
                  $self->{PPM_INSTALL_SCRIPT};
        }
    }

    my ($bin_location) = $self->{BINARY_LOCATION} || '';
    $bin_location =~ s/\\/\\\\/g;

    $make_ppd .= sprintf q{\t\t<CODEBASE HREF=\"%s\" />\n}, $bin_location;
    $make_ppd .= q{\t</IMPLEMENTATION>\n};
    $make_ppd .= q{</SOFTPKG>\n};

    $make_ppd .= '}" >> $(DISTNAME).ppd';

    return $make_ppd;
}

=item prefixify

  $MM->prefixify($var, $prefix, $new_prefix, $default);

Using either $MM->{uc $var} || $Config{lc $var}, it will attempt to
replace it's $prefix with a $new_prefix.  Should the $prefix fail to
match it sill simply set it to the $new_prefix + $default.

This is for heuristics which attempt to create directory structures
that mirror those of the installed perl.

For example:

    $MM->prefixify('installman1dir', '/usr', '/home/foo', 'man/man1');

this will attempt to remove '/usr' from the front of the
$MM->{INSTALLMAN1DIR} path (initializing it to $Config{installman1dir}
if necessary) and replace it with '/home/foo'.  If this fails it will
simply use '/home/foo/man/man1'.

=cut

sub prefixify {
    my($self,$var,$sprefix,$rprefix,$default) = @_;

    my $path = $self->{uc $var} || 
               $Config_Override{lc $var} || $Config{lc $var} || '';

    print STDERR "  prefixify $var => $path\n" if $Verbose >= 2;
    print STDERR "    from $sprefix to $rprefix\n" if $Verbose >= 2;

    unless( $path =~ s{^\Q$sprefix\E(?=/|\z)}{$rprefix}s ) {

        print STDERR "    cannot prefix, using default.\n" if $Verbose >= 2;
        print STDERR "    no default!\n" if !$default && $Verbose >= 2;

        $path = File::Spec->catdir($rprefix, $default) if $default;
    }

    print "    now $path\n" if $Verbose >= 2;
    return $self->{uc $var} = $path;
}


=item processPL (o)

Defines targets to run *.PL files.

=cut

sub processPL {
    my($self) = shift;
    return "" unless $self->{PL_FILES};
    my(@m, $plfile);
    foreach $plfile (sort keys %{$self->{PL_FILES}}) {
        my $list = ref($self->{PL_FILES}->{$plfile})
		? $self->{PL_FILES}->{$plfile}
		: [$self->{PL_FILES}->{$plfile}];
	my $target;
	foreach $target (@$list) {
	push @m, "
all :: $target
	$self->{NOECHO}\$(NOOP)

$target :: $plfile
	\$(PERLRUNINST) $plfile $target
";
	}
    }
    join "", @m;
}

=item quote_paren

Backslashes parentheses C<()> in command line arguments.
Doesn't handle recursive Makefile C<$(...)> constructs,
but handles simple ones.

=cut

sub quote_paren {
    local $_ = shift;
    s/\$\((.+?)\)/\$\\\\($1\\\\)/g;	# protect $(...)
    s/(?<!\\)([()])/\\$1/g;		# quote unprotected
    s/\$\\\\\((.+?)\\\\\)/\$($1)/g;	# unprotect $(...)
    return $_;
}

=item realclean (o)

Defines the realclean target.

=cut

sub realclean {
    my($self, %attribs) = @_;
    my(@m);

    push(@m,'
# Delete temporary files (via clean) and also delete installed files
realclean purge ::  clean
');
    # realclean subdirectories first (already cleaned)
    my $sub;
    if( $Is_Win32  &&  Win32::IsWin95() ) {
        $sub = <<'REALCLEAN';
	-cd %s
	-$(PERLRUN) -e "exit unless -f shift; system q{$(MAKE) realclean}" %s
	-cd ..
REALCLEAN
    }
    else {
        $sub = <<'REALCLEAN';
	-cd %s && $(TEST_F) %s && $(MAKE) %s realclean
REALCLEAN
    }

    foreach(@{$self->{DIR}}){
	push(@m, sprintf($sub,$_,"$self->{MAKEFILE}.old","-f $self->{MAKEFILE}.old"));
	push(@m, sprintf($sub,$_,"$self->{MAKEFILE}",''));
    }
    push(@m, "	$self->{RM_RF} \$(INST_AUTODIR) \$(INST_ARCHAUTODIR)\n");
    push(@m, "	$self->{RM_RF} \$(DISTVNAME)\n");
    if( $self->has_link_code ){
        push(@m, "	$self->{RM_F} \$(INST_DYNAMIC) \$(INST_BOOT)\n");
        push(@m, "	$self->{RM_F} \$(INST_STATIC)\n");
    }
    # Issue a several little RM_F commands rather than risk creating a
    # very long command line (useful for extensions such as Encode
    # that have many files).
    if (keys %{$self->{PM}}) {
	my $line = "";
	foreach (values %{$self->{PM}}) {
	    if (length($line) + length($_) > 80) {
		push @m, "\t$self->{RM_F} $line\n";
		$line = $_;
	    }
	    else {
		$line .= " $_"; 
	    }
	}
    push @m, "\t$self->{RM_F} $line\n" if $line;
    }
    my(@otherfiles) = ($self->{MAKEFILE},
		       "$self->{MAKEFILE}.old"); # Makefiles last
    push(@otherfiles, $attribs{FILES}) if $attribs{FILES};
    push(@m, "	$self->{RM_RF} @otherfiles\n") if @otherfiles;
    push(@m, "	$attribs{POSTOP}\n")       if $attribs{POSTOP};
    join("", @m);
}

=item replace_manpage_separator

  my $man_name = $MM->replace_manpage_separator($file_path);

Takes the name of a package, which may be a nested package, in the
form 'Foo/Bar.pm' and replaces the slash with C<::> or something else
safe for a man page file name.  Returns the replacement.

=cut

sub replace_manpage_separator {
    my($self,$man) = @_;

    $man =~ s,/+,::,g;
    return $man;
}

=item static (o)

Defines the static target.

=cut

sub static {
# --- Static Loading Sections ---

    my($self) = shift;
    '
## $(INST_PM) has been moved to the all: target.
## It remains here for awhile to allow for old usage: "make static"
#static :: '.$self->{MAKEFILE}.' $(INST_STATIC) $(INST_PM)
static :: '.$self->{MAKEFILE}.' $(INST_STATIC)
	'.$self->{NOECHO}.'$(NOOP)
';
}

=item static_lib (o)

Defines how to produce the *.a (or equivalent) files.

=cut

sub static_lib {
    my($self) = @_;
# Come to think of it, if there are subdirs with linkcode, we still have no INST_STATIC
#    return '' unless $self->needs_linking(); #might be because of a subdir

    return '' unless $self->has_link_code;

    my(@m);
    push(@m, <<'END');
$(INST_STATIC): $(OBJECT) $(MYEXTLIB) $(INST_ARCHAUTODIR)/.exists
	$(RM_RF) $@
END
    # If this extension has its own library (eg SDBM_File)
    # then copy that to $(INST_STATIC) and add $(OBJECT) into it.
    push(@m, "\t$self->{CP} \$(MYEXTLIB) \$\@\n") if $self->{MYEXTLIB};

    my $ar; 
    if (exists $self->{FULL_AR} && -x $self->{FULL_AR}) {
        # Prefer the absolute pathed ar if available so that PATH
        # doesn't confuse us.  Perl itself is built with the full_ar.  
        $ar = 'FULL_AR';
    } else {
        $ar = 'AR';
    }
    push @m,
        "\t\$($ar) ".'$(AR_STATIC_ARGS) $@ $(OBJECT) && $(RANLIB) $@'."\n";
    push @m,
q{	$(CHMOD) $(PERM_RWX) $@
	}.$self->{NOECHO}.q{echo "$(EXTRALIBS)" > $(INST_ARCHAUTODIR)/extralibs.ld
};
    # Old mechanism - still available:
    push @m,
"\t$self->{NOECHO}".q{echo "$(EXTRALIBS)" >> $(PERL_SRC)/ext.libs
}	if $self->{PERL_SRC} && $self->{EXTRALIBS};
    push @m, "\n";

    push @m, $self->dir_target('$(INST_ARCHAUTODIR)');
    join('', "\n",@m);
}

=item staticmake (o)

Calls makeaperl.

=cut

sub staticmake {
    my($self, %attribs) = @_;
    my(@static);

    my(@searchdirs)=($self->{PERL_ARCHLIB}, $self->{SITEARCHEXP},  $self->{INST_ARCHLIB});

    # And as it's not yet built, we add the current extension
    # but only if it has some C code (or XS code, which implies C code)
    if (@{$self->{C}}) {
	@static = File::Spec->catfile($self->{INST_ARCHLIB},
				 "auto",
				 $self->{FULLEXT},
				 "$self->{BASEEXT}$self->{LIB_EXT}"
				);
    }

    # Either we determine now, which libraries we will produce in the
    # subdirectories or we do it at runtime of the make.

    # We could ask all subdir objects, but I cannot imagine, why it
    # would be necessary.

    # Instead we determine all libraries for the new perl at
    # runtime.
    my(@perlinc) = ($self->{INST_ARCHLIB}, $self->{INST_LIB}, $self->{PERL_ARCHLIB}, $self->{PERL_LIB});

    $self->makeaperl(MAKE	=> $self->{MAKEFILE},
		     DIRS	=> \@searchdirs,
		     STAT	=> \@static,
		     INCL	=> \@perlinc,
		     TARGET	=> $self->{MAP_TARGET},
		     TMP	=> "",
		     LIBPERL	=> $self->{LIBPERL_A}
		    );
}

=item subdir_x (o)

Helper subroutine for subdirs

=cut

sub subdir_x {
    my($self, $subdir) = @_;
    my(@m);
    if ($Is_Win32 && Win32::IsWin95()) {
	if ($Config{'make'} =~ /dmake/i) {
	    # dmake-specific
	    return <<EOT;
subdirs ::
@[
	cd $subdir
	\$(MAKE) -f \$(FIRST_MAKEFILE) all \$(PASTHRU)
	cd ..
]
EOT
        } elsif ($Config{'make'} =~ /nmake/i) {
	    # nmake-specific
	    return <<EOT;
subdirs ::
	cd $subdir
	\$(MAKE) -f \$(FIRST_MAKEFILE) all \$(PASTHRU)
	cd ..
EOT
	}
    } else {
	return <<EOT;

subdirs ::
	$self->{NOECHO}cd $subdir && \$(MAKE) -f \$(FIRST_MAKEFILE) all \$(PASTHRU)
EOT
    }
}

=item subdirs (o)

Defines targets to process subdirectories.

=cut

sub subdirs {
# --- Sub-directory Sections ---
    my($self) = shift;
    my(@m,$dir);
    # This method provides a mechanism to automatically deal with
    # subdirectories containing further Makefile.PL scripts.
    # It calls the subdir_x() method for each subdirectory.
    foreach $dir (@{$self->{DIR}}){
	push(@m, $self->subdir_x($dir));
####	print "Including $dir subdirectory\n";
    }
    if (@m){
	unshift(@m, "
# The default clean, realclean and test targets in this Makefile
# have automatically been given entries for each subdir.

");
    } else {
	push(@m, "\n# none")
    }
    join('',@m);
}

=item test (o)

Defines the test targets.

=cut

sub test {
# --- Test and Installation Sections ---

    my($self, %attribs) = @_;
    my $tests = $attribs{TESTS} || '';
    if (!$tests && -d 't') {
        $tests = $self->find_tests;
    }
    # note: 'test.pl' name is also hardcoded in init_dirscan()
    my(@m);
    push(@m,"
TEST_VERBOSE=0
TEST_TYPE=test_\$(LINKTYPE)
TEST_FILE = test.pl
TEST_FILES = $tests
TESTDB_SW = -d

testdb :: testdb_\$(LINKTYPE)

test :: \$(TEST_TYPE)
");

    if ($Is_Win32 && Win32::IsWin95()) {
        push(@m, map(qq{\t$self->{NOECHO}\$(PERLRUN) -e "exit unless -f shift; chdir '$_'; system q{\$(MAKE) test \$(PASTHRU)}" $self->{MAKEFILE}\n}, @{$self->{DIR}}));
    }
    else {
        push(@m, map("\t$self->{NOECHO}cd $_ && \$(TEST_F) $self->{MAKEFILE} && \$(MAKE) test \$(PASTHRU)\n", @{$self->{DIR}}));
    }

    push(@m, "\t$self->{NOECHO}echo 'No tests defined for \$(NAME) extension.'\n")
	unless $tests or -f "test.pl" or @{$self->{DIR}};
    push(@m, "\n");

    push(@m, "test_dynamic :: pure_all\n");
    push(@m, $self->test_via_harness('$(FULLPERLRUN)', '$(TEST_FILES)')) 
      if $tests;
    push(@m, $self->test_via_script('$(FULLPERLRUN)', '$(TEST_FILE)')) 
      if -f "test.pl";
    push(@m, "\n");

    push(@m, "testdb_dynamic :: pure_all\n");
    push(@m, $self->test_via_script('$(FULLPERLRUN) $(TESTDB_SW)', 
                                    '$(TEST_FILE)'));
    push(@m, "\n");

    # Occasionally we may face this degenerate target:
    push @m, "test_ : test_dynamic\n\n";

    if ($self->needs_linking()) {
	push(@m, "test_static :: pure_all \$(MAP_TARGET)\n");
	push(@m, $self->test_via_harness('./$(MAP_TARGET)', '$(TEST_FILES)')) if $tests;
	push(@m, $self->test_via_script('./$(MAP_TARGET)', '$(TEST_FILE)')) if -f "test.pl";
	push(@m, "\n");
	push(@m, "testdb_static :: pure_all \$(MAP_TARGET)\n");
	push(@m, $self->test_via_script('./$(MAP_TARGET) $(TESTDB_SW)', '$(TEST_FILE)'));
	push(@m, "\n");
    } else {
	push @m, "test_static :: test_dynamic\n";
	push @m, "testdb_static :: testdb_dynamic\n";
    }
    join("", @m);
}

=item test_via_harness (override)

For some reason which I forget, Unix machines like to have
PERL_DL_NONLAZY set for tests.

=cut

sub test_via_harness {
    my($self, $perl, $tests) = @_;
    return $self->SUPER::test_via_harness("PERL_DL_NONLAZY=1 $perl", $tests);
}

=item test_via_script (override)

Again, the PERL_DL_NONLAZY thing.

=cut

sub test_via_script {
    my($self, $perl, $script) = @_;
    return $self->SUPER::test_via_script("PERL_DL_NONLAZY=1 $perl", $script);
}

=item tool_autosplit (o)

Defines a simple perl call that runs autosplit. May be deprecated by
pm_to_blib soon.

=cut

sub tool_autosplit {
    my($self, %attribs) = @_;
    my($asl) = "";
    $asl = "\$\$AutoSplit::Maxlen=$attribs{MAXLEN};" if $attribs{MAXLEN};

    return sprintf <<'MAKE_FRAG', $asl;
# Usage: $(AUTOSPLITFILE) FileToSplit AutoDirToSplitInto
AUTOSPLITFILE = $(PERLRUN) -e 'use AutoSplit; %s autosplit($$ARGV[0], $$ARGV[1], 0, 1, 1) ;'

MAKE_FRAG

}

=item tools_other (o)

Defines SHELL, LD, TOUCH, CP, MV, RM_F, RM_RF, CHMOD, UMASK_NULL in
the Makefile. Also defines the perl programs MKPATH,
WARN_IF_OLD_PACKLIST, MOD_INSTALL. DOC_INSTALL, and UNINSTALL.

=cut

sub tools_other {
    my($self) = shift;
    my @m;
    my $bin_sh = $Config{sh} || '/bin/sh';
    push @m, qq{
SHELL = $bin_sh
};

    for (qw/ CHMOD CP LD MV NOOP RM_F RM_RF TEST_F TOUCH UMASK_NULL DEV_NULL/ ) {
	push @m, "$_ = $self->{$_}\n";
    }

    push @m, q{
# The following is a portable way to say mkdir -p
# To see which directories are created, change the if 0 to if 1
MKPATH = $(PERLRUN) "-MExtUtils::Command" -e mkpath

# This helps us to minimize the effect of the .exists files A yet
# better solution would be to have a stable file in the perl
# distribution with a timestamp of zero. But this solution doesn't
# need any changes to the core distribution and works with older perls
EQUALIZE_TIMESTAMP = $(PERLRUN) "-MExtUtils::Command" -e eqtime
};


    return join "", @m if $self->{PARENT};

    push @m, q{
# Here we warn users that an old packlist file was found somewhere,
# and that they should call some uninstall routine
WARN_IF_OLD_PACKLIST = $(PERL) -we 'exit unless -f $$ARGV[0];' \\
-e 'print "WARNING: I have found an old package in\n";' \\
-e 'print "\t$$ARGV[0].\n";' \\
-e 'print "Please make sure the two installations are not conflicting\n";'

UNINST=0
VERBINST=0

MOD_INSTALL = $(PERL) "-I$(INST_LIB)" "-I$(PERL_LIB)" "-MExtUtils::Install" \
-e "install({@ARGV},'$(VERBINST)',0,'$(UNINST)');"

DOC_INSTALL = $(PERL) -e '$$\="\n\n";' \
-e 'print "=head2 ", scalar(localtime), ": C<", shift, ">", " L<", $$arg=shift, "|", $$arg, ">";' \
-e 'print "=over 4";' \
-e 'while (defined($$key = shift) and defined($$val = shift)){print "=item *";print "C<$$key: $$val>";}' \
-e 'print "=back";'

UNINSTALL =   $(PERLRUN) "-MExtUtils::Install" \
-e 'uninstall($$ARGV[0],1,1); print "\nUninstall is deprecated. Please check the";' \
-e 'print " packlist above carefully.\n  There may be errors. Remove the";' \
-e 'print " appropriate files manually.\n  Sorry for the inconveniences.\n"'
};

    return join "", @m;
}

=item tool_xsubpp (o)

Determines typemaps, xsubpp version, prototype behaviour.

=cut

sub tool_xsubpp {
    my($self) = shift;
    return "" unless $self->needs_linking;
    my($xsdir)  = File::Spec->catdir($self->{PERL_LIB},"ExtUtils");
    my(@tmdeps) = File::Spec->catdir('$(XSUBPPDIR)','typemap');
    if( $self->{TYPEMAPS} ){
	my $typemap;
	foreach $typemap (@{$self->{TYPEMAPS}}){
		if( ! -f  $typemap ){
			warn "Typemap $typemap not found.\n";
		}
		else{
			push(@tmdeps,  $typemap);
		}
	}
    }
    push(@tmdeps, "typemap") if -f "typemap";
    my(@tmargs) = map("-typemap $_", @tmdeps);
    if( exists $self->{XSOPT} ){
 	unshift( @tmargs, $self->{XSOPT} );
    }


    my $xsubpp_version = $self->xsubpp_version(File::Spec->catfile($xsdir,"xsubpp"));

    # What are the correct thresholds for version 1 && 2 Paul?
    if ( $xsubpp_version > 1.923 ){
	$self->{XSPROTOARG} = "" unless defined $self->{XSPROTOARG};
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

    my $xsubpp = "xsubpp";

    return qq{
XSUBPPDIR = $xsdir
XSUBPP = \$(XSUBPPDIR)/$xsubpp
XSPROTOARG = $self->{XSPROTOARG}
XSUBPPDEPS = @tmdeps \$(XSUBPP)
XSUBPPARGS = @tmargs
XSUBPP_EXTRA_ARGS = 
};
};

sub xsubpp_version
{
    my($self,$xsubpp) = @_;
    return $Xsubpp_Version if defined $Xsubpp_Version; # global variable

    my ($version) ;

    # try to figure out the version number of the xsubpp on the system

    # first try the -v flag, introduced in 1.921 & 2.000a2

    return "" unless $self->needs_linking;

    my $command = qq{$self->{PERL} "-I$self->{PERL_LIB}" $xsubpp -v 2>&1};
    print "Running $command\n" if $Verbose >= 2;
    $version = `$command` ;
    warn "Running '$command' exits with status " . ($?>>8) if $?;
    chop $version ;

    return $Xsubpp_Version = $1 if $version =~ /^xsubpp version (.*)/ ;

    # nope, then try something else

    my $counter = '000';
    my ($file) = 'temp' ;
    $counter++ while -e "$file$counter"; # don't overwrite anything
    $file .= $counter;

    open(F, ">$file") or die "Cannot open file '$file': $!\n" ;
    print F <<EOM ;
MODULE = fred PACKAGE = fred

int
fred(a)
        int     a;
EOM

    close F ;

    $command = "$self->{PERL} $xsubpp $file 2>&1";
    print "Running $command\n" if $Verbose >= 2;
    my $text = `$command` ;
    warn "Running '$command' exits with status " . ($?>>8) if $?;
    unlink $file ;

    # gets 1.2 -> 1.92 and 2.000a1
    return $Xsubpp_Version = $1 if $text =~ /automatically by xsubpp version ([\S]+)\s*/  ;

    # it is either 1.0 or 1.1
    return $Xsubpp_Version = 1.1 if $text =~ /^Warning: ignored semicolon/ ;

    # none of the above, so 1.0
    return $Xsubpp_Version = "1.0" ;
}

=item top_targets (o)

Defines the targets all, subdirs, config, and O_FILES

=cut

sub top_targets {
# --- Target Sections ---

    my($self) = shift;
    my(@m);

    push @m, '
all :: pure_all manifypods
	'.$self->{NOECHO}.'$(NOOP)
' 
	  unless $self->{SKIPHASH}{'all'};
    
    push @m, '
pure_all :: config pm_to_blib subdirs linkext
	'.$self->{NOECHO}.'$(NOOP)

subdirs :: $(MYEXTLIB)
	'.$self->{NOECHO}.'$(NOOP)

config :: '.$self->{MAKEFILE}.' $(INST_LIBDIR)/.exists
	'.$self->{NOECHO}.'$(NOOP)

config :: $(INST_ARCHAUTODIR)/.exists
	'.$self->{NOECHO}.'$(NOOP)

config :: $(INST_AUTODIR)/.exists
	'.$self->{NOECHO}.'$(NOOP)
';

    push @m, $self->dir_target(qw[$(INST_AUTODIR) $(INST_LIBDIR) $(INST_ARCHAUTODIR)]);

    if (%{$self->{MAN1PODS}}) {
	push @m, qq[
config :: \$(INST_MAN1DIR)/.exists
	$self->{NOECHO}\$(NOOP)

];
	push @m, $self->dir_target(qw[$(INST_MAN1DIR)]);
    }
    if (%{$self->{MAN3PODS}}) {
	push @m, qq[
config :: \$(INST_MAN3DIR)/.exists
	$self->{NOECHO}\$(NOOP)

];
	push @m, $self->dir_target(qw[$(INST_MAN3DIR)]);
    }

    push @m, '
$(O_FILES): $(H_FILES)
' if @{$self->{O_FILES} || []} && @{$self->{H} || []};

    push @m, q{
help:
	perldoc ExtUtils::MakeMaker
};

    join('',@m);
}

=item writedoc

Obsolete, deprecated method. Not used since Version 5.21.

=cut

sub writedoc {
# --- perllocal.pod section ---
    my($self,$what,$name,@attribs)=@_;
    my $time = localtime;
    print "=head2 $time: $what C<$name>\n\n=over 4\n\n=item *\n\n";
    print join "\n\n=item *\n\n", map("C<$_>",@attribs);
    print "\n\n=back\n\n";
}

=item xs_c (o)

Defines the suffix rules to compile XS files to C.

=cut

sub xs_c {
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs.c:
	$(PERLRUN) $(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $(XSUBPP_EXTRA_ARGS) $*.xs > $*.xsc && $(MV) $*.xsc $*.c
';
}

=item xs_cpp (o)

Defines the suffix rules to compile XS files to C++.

=cut

sub xs_cpp {
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs.cpp:
	$(PERLRUN) $(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $*.xs > $*.xsc && $(MV) $*.xsc $*.cpp
';
}

=item xs_o (o)

Defines suffix rules to go from XS to object files directly. This is
only intended for broken make implementations.

=cut

sub xs_o {	# many makes are too dumb to use xs_c then c_o
    my($self) = shift;
    return '' unless $self->needs_linking();
    '
.xs$(OBJ_EXT):
	$(PERLRUN) $(XSUBPP) $(XSPROTOARG) $(XSUBPPARGS) $*.xs > $*.xsc && $(MV) $*.xsc $*.c
	$(CCCMD) $(CCCDLFLAGS) "-I$(PERL_INC)" $(PASTHRU_DEFINE) $(DEFINE) $*.c
';
}

=item perl_archive

This is internal method that returns path to libperl.a equivalent
to be linked to dynamic extensions. UNIX does not have one but other
OSs might have one.

=cut 

sub perl_archive
{
 return "";
}

=item perl_archive_after

This is an internal method that returns path to a library which
should be put on the linker command line I<after> the external libraries
to be linked to dynamic extensions.  This may be needed if the linker
is one-pass, and Perl includes some overrides for C RTL functions,
such as malloc().

=cut 

sub perl_archive_after
{
 return "";
}

=item export_list

This is internal method that returns name of a file that is
passed to linker to define symbols to be exported.
UNIX does not have one but OS2 and Win32 do.

=cut 

sub export_list
{
 return "";
}


1;

=back

=head1 SEE ALSO

L<ExtUtils::MakeMaker>

=cut

__END__
