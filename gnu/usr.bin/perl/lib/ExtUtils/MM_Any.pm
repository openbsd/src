package ExtUtils::MM_Any;

use strict;
use vars qw($VERSION @ISA);
$VERSION = 0.07;
@ISA = qw(File::Spec);

use Config;
use File::Spec;


=head1 NAME

ExtUtils::MM_Any - Platform-agnostic MM methods

=head1 SYNOPSIS

  FOR INTERNAL USE ONLY!

  package ExtUtils::MM_SomeOS;

  # Temporarily, you have to subclass both.  Put MM_Any first.
  require ExtUtils::MM_Any;
  require ExtUtils::MM_Unix;
  @ISA = qw(ExtUtils::MM_Any ExtUtils::Unix);

=head1 DESCRIPTION

B<FOR INTERNAL USE ONLY!>

ExtUtils::MM_Any is a superclass for the ExtUtils::MM_* set of
modules.  It contains methods which are either inherently
cross-platform or are written in a cross-platform manner.

Subclass off of ExtUtils::MM_Any I<and> ExtUtils::MM_Unix.  This is a
temporary solution.

B<THIS MAY BE TEMPORARY!>

=head1 Inherently Cross-Platform Methods

These are methods which are by their nature cross-platform and should
always be cross-platform.

=over 4

=item installvars

    my @installvars = $mm->installvars;

A list of all the INSTALL* variables without the INSTALL prefix.  Useful
for iteration or building related variable sets.

=cut

sub installvars {
    return qw(PRIVLIB SITELIB  VENDORLIB
              ARCHLIB SITEARCH VENDORARCH
              BIN     SITEBIN  VENDORBIN
              SCRIPT
              MAN1DIR SITEMAN1DIR VENDORMAN1DIR
              MAN3DIR SITEMAN3DIR VENDORMAN3DIR
             );
}

=item os_flavor_is

    $mm->os_flavor_is($this_flavor);
    $mm->os_flavor_is(@one_of_these_flavors);

Checks to see if the current operating system is one of the given flavors.

This is useful for code like:

    if( $mm->os_flavor_is('Unix') ) {
        $out = `foo 2>&1`;
    }
    else {
        $out = `foo`;
    }

=cut

sub os_flavor_is {
    my $self = shift;
    my %flavors = map { ($_ => 1) } $self->os_flavor;
    return (grep { $flavors{$_} } @_) ? 1 : 0;
}

=back

=head2 File::Spec wrappers

ExtUtils::MM_Any is a subclass of File::Spec.  The methods noted here
override File::Spec.

=over 4

=item catfile

File::Spec <= 0.83 has a bug where the file part of catfile is not
canonicalized.  This override fixes that bug.

=cut

sub catfile {
    my $self = shift;
    return $self->canonpath($self->SUPER::catfile(@_));
}

=back

=head1 Thought To Be Cross-Platform Methods

These are methods which are thought to be cross-platform by virtue of
having been written in a way to avoid incompatibilities.  They may
require partial overrides.

=over 4

=item B<split_command>

    my @cmds = $MM->split_command($cmd, @args);

Most OS have a maximum command length they can execute at once.  Large
modules can easily generate commands well past that limit.  Its
necessary to split long commands up into a series of shorter commands.

split_command() will return a series of @cmds each processing part of
the args.  Collectively they will process all the arguments.  Each
individual line in @cmds will not be longer than the
$self->max_exec_len being careful to take into account macro expansion.

$cmd should include any switches and repeated initial arguments.

If no @args are given, no @cmds will be returned.

Pairs of arguments will always be preserved in a single command, this
is a heuristic for things like pm_to_blib and pod2man which work on
pairs of arguments.  This makes things like this safe:

    $self->split_command($cmd, %pod2man);


=cut

sub split_command {
    my($self, $cmd, @args) = @_;

    my @cmds = ();
    return(@cmds) unless @args;

    # If the command was given as a here-doc, there's probably a trailing
    # newline.
    chomp $cmd;

    # set aside 20% for macro expansion.
    my $len_left = int($self->max_exec_len * 0.80);
    $len_left -= length $self->_expand_macros($cmd);

    do {
        my $arg_str = '';
        my @next_args;
        while( @next_args = splice(@args, 0, 2) ) {
            # Two at a time to preserve pairs.
            my $next_arg_str = "\t  ". join ' ', @next_args, "\n";

            if( !length $arg_str ) {
                $arg_str .= $next_arg_str
            }
            elsif( length($arg_str) + length($next_arg_str) > $len_left ) {
                unshift @args, @next_args;
                last;
            }
            else {
                $arg_str .= $next_arg_str;
            }
        }
        chop $arg_str;

        push @cmds, $self->escape_newlines("$cmd\n$arg_str");
    } while @args;

    return @cmds;
}


sub _expand_macros {
    my($self, $cmd) = @_;

    $cmd =~ s{\$\((\w+)\)}{
        defined $self->{$1} ? $self->{$1} : "\$($1)"
    }e;
    return $cmd;
}


=item B<echo>

    my @commands = $MM->echo($text);
    my @commands = $MM->echo($text, $file);
    my @commands = $MM->echo($text, $file, $appending);

Generates a set of @commands which print the $text to a $file.

If $file is not given, output goes to STDOUT.

If $appending is true the $file will be appended to rather than
overwritten.

=cut

sub echo {
    my($self, $text, $file, $appending) = @_;
    $appending ||= 0;

    my @cmds = map { '$(NOECHO) $(ECHO) '.$self->quote_literal($_) } 
               split /\n/, $text;
    if( $file ) {
        my $redirect = $appending ? '>>' : '>';
        $cmds[0] .= " $redirect $file";
        $_ .= " >> $file" foreach @cmds[1..$#cmds];
    }

    return @cmds;
}


=item init_VERSION

    $mm->init_VERSION

Initialize macros representing versions of MakeMaker and other tools

MAKEMAKER: path to the MakeMaker module.

MM_VERSION: ExtUtils::MakeMaker Version

MM_REVISION: ExtUtils::MakeMaker version control revision (for backwards 
             compat)

VERSION: version of your module

VERSION_MACRO: which macro represents the version (usually 'VERSION')

VERSION_SYM: like version but safe for use as an RCS revision number

DEFINE_VERSION: -D line to set the module version when compiling

XS_VERSION: version in your .xs file.  Defaults to $(VERSION)

XS_VERSION_MACRO: which macro represents the XS version.

XS_DEFINE_VERSION: -D line to set the xs version when compiling.

Called by init_main.

=cut

sub init_VERSION {
    my($self) = shift;

    $self->{MAKEMAKER}  = $ExtUtils::MakeMaker::Filename;
    $self->{MM_VERSION} = $ExtUtils::MakeMaker::VERSION;
    $self->{MM_REVISION}= $ExtUtils::MakeMaker::Revision;
    $self->{VERSION_FROM} ||= '';

    if ($self->{VERSION_FROM}){
        $self->{VERSION} = $self->parse_version($self->{VERSION_FROM});
        if( $self->{VERSION} eq 'undef' ) {
            require Carp;
            Carp::carp("WARNING: Setting VERSION via file ".
                       "'$self->{VERSION_FROM}' failed\n");
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


    $self->{VERSION_MACRO}  = 'VERSION';
    ($self->{VERSION_SYM} = $self->{VERSION}) =~ s/\W/_/g;
    $self->{DEFINE_VERSION} = '-D$(VERSION_MACRO)=\"$(VERSION)\"';


    # Graham Barr and Paul Marquess had some ideas how to ensure
    # version compatibility between the *.pm file and the
    # corresponding *.xs file. The bottomline was, that we need an
    # XS_VERSION macro that defaults to VERSION:
    $self->{XS_VERSION} ||= $self->{VERSION};

    $self->{XS_VERSION_MACRO}  = 'XS_VERSION';
    $self->{XS_DEFINE_VERSION} = '-D$(XS_VERSION_MACRO)=\"$(XS_VERSION)\"';

}

=item wraplist

Takes an array of items and turns them into a well-formatted list of
arguments.  In most cases this is simply something like:

    FOO \
    BAR \
    BAZ

=cut

sub wraplist {
    my $self = shift;
    return join " \\\n\t", @_;
}

=item manifypods

Defines targets and routines to translate the pods into manpages and
put them into the INST_* directories.

=cut

sub manifypods {
    my $self          = shift;

    my $POD2MAN_macro = $self->POD2MAN_macro();
    my $manifypods_target = $self->manifypods_target();

    return <<END_OF_TARGET;

$POD2MAN_macro

$manifypods_target

END_OF_TARGET

}


=item manifypods_target

  my $manifypods_target = $self->manifypods_target;

Generates the manifypods target.  This target generates man pages from
all POD files in MAN1PODS and MAN3PODS.

=cut

sub manifypods_target {
    my($self) = shift;

    my $man1pods      = '';
    my $man3pods      = '';
    my $dependencies  = '';

    # populate manXpods & dependencies:
    foreach my $name (keys %{$self->{MAN1PODS}}, keys %{$self->{MAN3PODS}}) {
        $dependencies .= " \\\n\t$name";
    }

    foreach my $name (keys %{$self->{MAN3PODS}}) {
        $dependencies .= " \\\n\t$name"
    }

    my $manify = <<END;
manifypods : pure_all $dependencies
END

    my @man_cmds;
    foreach my $section (qw(1 3)) {
        my $pods = $self->{"MAN${section}PODS"};
        push @man_cmds, $self->split_command(<<CMD, %$pods);
	\$(NOECHO) \$(POD2MAN) --section=$section --perm_rw=\$(PERM_RW)
CMD
    }

    $manify .= "\t\$(NOECHO) \$(NOOP)\n" unless @man_cmds;
    $manify .= join '', map { "$_\n" } @man_cmds;

    return $manify;
}


=item makemakerdflt_target

  my $make_frag = $mm->makemakerdflt_target

Returns a make fragment with the makemakerdeflt_target specified.
This target is the first target in the Makefile, is the default target
and simply points off to 'all' just in case any make variant gets
confused or something gets snuck in before the real 'all' target.

=cut

sub makemakerdflt_target {
    return <<'MAKE_FRAG';
makemakerdflt: all
	$(NOECHO) $(NOOP)
MAKE_FRAG

}


=item special_targets

  my $make_frag = $mm->special_targets

Returns a make fragment containing any targets which have special
meaning to make.  For example, .SUFFIXES and .PHONY.

=cut

sub special_targets {
    my $make_frag = <<'MAKE_FRAG';
.SUFFIXES: .xs .c .C .cpp .i .s .cxx .cc $(OBJ_EXT)

.PHONY: all config static dynamic test linkext manifest

MAKE_FRAG

    $make_frag .= <<'MAKE_FRAG' if $ENV{CLEARCASE_ROOT};
.NO_CONFIG_REC: Makefile

MAKE_FRAG

    return $make_frag;
}

=item POD2MAN_macro

  my $pod2man_macro = $self->POD2MAN_macro

Returns a definition for the POD2MAN macro.  This is a program
which emulates the pod2man utility.  You can add more switches to the
command by simply appending them on the macro.

Typical usage:

    $(POD2MAN) --section=3 --perm_rw=$(PERM_RW) podfile1 man_page1 ...

=cut

sub POD2MAN_macro {
    my $self = shift;

# Need the trailing '--' so perl stops gobbling arguments and - happens
# to be an alternative end of line seperator on VMS so we quote it
    return <<'END_OF_DEF';
POD2MAN_EXE = $(PERLRUN) "-MExtUtils::Command::MM" -e pod2man "--"
POD2MAN = $(POD2MAN_EXE)
END_OF_DEF
}


=item test_via_harness

  my $command = $mm->test_via_harness($perl, $tests);

Returns a $command line which runs the given set of $tests with
Test::Harness and the given $perl.

Used on the t/*.t files.

=cut

sub test_via_harness {
    my($self, $perl, $tests) = @_;

    return qq{\t$perl "-MExtUtils::Command::MM" }.
           qq{"-e" "test_harness(\$(TEST_VERBOSE), '\$(INST_LIB)', '\$(INST_ARCHLIB)')" $tests\n};
}

=item test_via_script

  my $command = $mm->test_via_script($perl, $script);

Returns a $command line which just runs a single test without
Test::Harness.  No checks are done on the results, they're just
printed.

Used for test.pl, since they don't always follow Test::Harness
formatting.

=cut

sub test_via_script {
    my($self, $perl, $script) = @_;
    return qq{\t$perl "-I\$(INST_LIB)" "-I\$(INST_ARCHLIB)" $script\n};
}

=item libscan

  my $wanted = $self->libscan($path);

Takes a path to a file or dir and returns an empty string if we don't
want to include this file in the library.  Otherwise it returns the
the $path unchanged.

Mainly used to exclude RCS, CVS, and SCCS directories from
installation.

=cut

sub libscan {
    my($self,$path) = @_;
    my($dirs,$file) = ($self->splitpath($path))[1,2];
    return '' if grep /^(?:RCS|CVS|SCCS|\.svn)$/, 
                     $self->splitdir($dirs), $file;

    return $path;
}

=item tool_autosplit

Defines a simple perl call that runs autosplit. May be deprecated by
pm_to_blib soon.

=cut

sub tool_autosplit {
    my($self, %attribs) = @_;

    my $maxlen = $attribs{MAXLEN} ? '$$AutoSplit::Maxlen=$attribs{MAXLEN};' 
                                  : '';

    my $asplit = $self->oneliner(sprintf <<'PERL_CODE', $maxlen);
use AutoSplit; %s autosplit($$ARGV[0], $$ARGV[1], 0, 1, 1)
PERL_CODE

    return sprintf <<'MAKE_FRAG', $asplit;
# Usage: $(AUTOSPLITFILE) FileToSplit AutoDirToSplitInto
AUTOSPLITFILE = %s

MAKE_FRAG

}


=item all_target

Generate the default target 'all'.

=cut

sub all_target {
    my $self = shift;

    return <<'MAKE_EXT';
all :: pure_all
	$(NOECHO) $(NOOP)
MAKE_EXT

}


=item metafile_target

    my $target = $mm->metafile_target;

Generate the metafile target.

Writes the file META.yml, YAML encoded meta-data about the module.  The
format follows Module::Build's as closely as possible.  Additionally, we
include:

    version_from
    installdirs

=cut

sub metafile_target {
    my $self = shift;

    return <<'MAKE_FRAG' if $self->{NO_META};
metafile:
	$(NOECHO) $(NOOP)
MAKE_FRAG

    my $prereq_pm = '';
    foreach my $mod ( sort { lc $a cmp lc $b } keys %{$self->{PREREQ_PM}} ) {
        my $ver = $self->{PREREQ_PM}{$mod};
        $prereq_pm .= sprintf "    %-30s %s\n", "$mod:", $ver;
    }
    
    my $meta = <<YAML;
# http://module-build.sourceforge.net/META-spec.html
#XXXXXXX This is a prototype!!!  It will change in the future!!! XXXXX#
name:         $self->{DISTNAME}
version:      $self->{VERSION}
version_from: $self->{VERSION_FROM}
installdirs:  $self->{INSTALLDIRS}
requires:
$prereq_pm
distribution_type: module
generated_by: ExtUtils::MakeMaker version $ExtUtils::MakeMaker::VERSION
YAML

    my @write_meta = $self->echo($meta, 'META.yml');
    return sprintf <<'MAKE_FRAG', join "\n\t", @write_meta;
metafile :
	%s
MAKE_FRAG

}


=item metafile_addtomanifest_target

  my $target = $mm->metafile_addtomanifest_target

Adds the META.yml file to the MANIFEST.

=cut

sub metafile_addtomanifest_target {
    my $self = shift;

    return <<'MAKE_FRAG' if $self->{NO_META};
metafile_addtomanifest:
	$(NOECHO) $(NOOP)
MAKE_FRAG

    my $add_meta = $self->oneliner(<<'CODE', ['-MExtUtils::Manifest=maniadd']);
eval { maniadd({q{META.yml} => q{Module meta-data (added by MakeMaker)}}) } 
    or print "Could not add META.yml to MANIFEST: $${'@'}\n"
CODE

    return sprintf <<'MAKE_FRAG', $add_meta;
metafile_addtomanifest:
	$(NOECHO) %s
MAKE_FRAG

}


=back

=head2 Abstract methods

Methods which cannot be made cross-platform and each subclass will
have to do their own implementation.

=over 4

=item oneliner

  my $oneliner = $MM->oneliner($perl_code);
  my $oneliner = $MM->oneliner($perl_code, \@switches);

This will generate a perl one-liner safe for the particular platform
you're on based on the given $perl_code and @switches (a -e is
assumed) suitable for using in a make target.  It will use the proper
shell quoting and escapes.

$(PERLRUN) will be used as perl.

Any newlines in $perl_code will be escaped.  Leading and trailing
newlines will be stripped.  Makes this idiom much easier:

    my $code = $MM->oneliner(<<'CODE', [...switches...]);
some code here
another line here
CODE

Usage might be something like:

    # an echo emulation
    $oneliner = $MM->oneliner('print "Foo\n"');
    $make = '$oneliner > somefile';

All dollar signs must be doubled in the $perl_code if you expect them
to be interpreted normally, otherwise it will be considered a make
macro.  Also remember to quote make macros else it might be used as a
bareword.  For example:

    # Assign the value of the $(VERSION_FROM) make macro to $vf.
    $oneliner = $MM->oneliner('$$vf = "$(VERSION_FROM)"');

Its currently very simple and may be expanded sometime in the figure
to include more flexible code and switches.


=item B<quote_literal>

    my $safe_text = $MM->quote_literal($text);

This will quote $text so it is interpreted literally in the shell.

For example, on Unix this would escape any single-quotes in $text and
put single-quotes around the whole thing.


=item B<escape_newlines>

    my $escaped_text = $MM->escape_newlines($text);

Shell escapes newlines in $text.


=item max_exec_len

    my $max_exec_len = $MM->max_exec_len;

Calculates the maximum command size the OS can exec.  Effectively,
this is the max size of a shell command line.

=for _private
$self->{_MAX_EXEC_LEN} is set by this method, but only for testing purposes.

=item B<init_others>

    $MM->init_others();

Initializes the macro definitions used by tools_other() and places them
in the $MM object.

If there is no description, its the same as the parameter to
WriteMakefile() documented in ExtUtils::MakeMaker.

Defines at least these macros.

  Macro             Description

  NOOP              Do nothing
  NOECHO            Tell make not to display the command itself

  MAKEFILE
  FIRST_MAKEFILE
  MAKEFILE_OLD
  MAKE_APERL_FILE   File used by MAKE_APERL

  SHELL             Program used to run
                    shell commands

  ECHO              Print text adding a newline on the end
  RM_F              Remove a file 
  RM_RF             Remove a directory          
  TOUCH             Update a file's timestamp   
  TEST_F            Test for a file's existence 
  CP                Copy a file                 
  MV                Move a file                 
  CHMOD             Change permissions on a     
                    file

  UMASK_NULL        Nullify umask
  DEV_NULL          Supress all command output

=item init_DIRFILESEP

  $MM->init_DIRFILESEP;
  my $dirfilesep = $MM->{DIRFILESEP};

Initializes the DIRFILESEP macro which is the seperator between the
directory and filename in a filepath.  ie. / on Unix, \ on Win32 and
nothing on VMS.

For example:

    # instead of $(INST_ARCHAUTODIR)/extralibs.ld
    $(INST_ARCHAUTODIR)$(DIRFILESEP)extralibs.ld

Something of a hack but it prevents a lot of code duplication between
MM_* variants.

Do not use this as a seperator between directories.  Some operating
systems use different seperators between subdirectories as between
directories and filenames (for example:  VOLUME:[dir1.dir2]file on VMS).

=item init_linker

    $mm->init_linker;

Initialize macros which have to do with linking.

PERL_ARCHIVE: path to libperl.a equivalent to be linked to dynamic
extensions.

PERL_ARCHIVE_AFTER: path to a library which should be put on the
linker command line I<after> the external libraries to be linked to
dynamic extensions.  This may be needed if the linker is one-pass, and
Perl includes some overrides for C RTL functions, such as malloc().

EXPORT_LIST: name of a file that is passed to linker to define symbols
to be exported.

Some OSes do not need these in which case leave it blank.


=item init_platform

    $mm->init_platform

Initialize any macros which are for platform specific use only.

A typical one is the version number of your OS specific mocule.
(ie. MM_Unix_VERSION or MM_VMS_VERSION).

=item platform_constants

    my $make_frag = $mm->platform_constants

Returns a make fragment defining all the macros initialized in
init_platform() rather than put them in constants().

=cut

sub init_platform {
    return '';
}

sub platform_constants {
    return '';
}

=item os_flavor

    my @os_flavor = $mm->os_flavor;

@os_flavor is the style of operating system this is, usually
corresponding to the MM_*.pm file we're using.  

The first element of @os_flavor is the major family (ie. Unix,
Windows, VMS, OS/2, MacOS, etc...) and the rest are sub families.

Some examples:

    Cygwin98       ('Unix',  'Cygwin', 'Cygwin9x')
    Windows NT     ('Win32', 'WinNT')
    Win98          ('Win32', 'Win9x')
    Linux          ('Unix',  'Linux')
    MacOS Classic  ('MacOS', 'MacOS Classic')
    MacOS X        ('Unix',  'Darwin', 'MacOS', 'MacOS X')
    OS/2           ('OS/2')

This is used to write code for styles of operating system.  
See os_flavor_is() for use.


=back

=head1 AUTHOR

Michael G Schwern <schwern@pobox.com> and the denizens of
makemaker@perl.org with code from ExtUtils::MM_Unix and
ExtUtils::MM_Win32.


=cut

1;
