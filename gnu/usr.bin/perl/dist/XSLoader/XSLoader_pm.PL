use strict;
use Config;
# We require DynaLoader to make sure that mod2fname is loaded
eval { require DynaLoader };

1 while unlink "XSLoader.pm";
open OUT, ">XSLoader.pm" or die $!;
print OUT <<'EOT';
# Generated from XSLoader.pm.PL (resolved %Config::Config value)

package XSLoader;

$VERSION = "0.17";

#use strict;

package DynaLoader;

EOT

# dlutils.c before 5.006 has this:
#
#    #ifdef DEBUGGING
#        dl_debug = SvIV( perl_get_sv("DynaLoader::dl_debug", 0x04) );
#    #endif
#
# where 0x04 is GV_ADDWARN, which causes a warning to be issued by the call
# into XS below, if DynaLoader.pm hasn't been loaded.
# It was changed to 0 in the commit(s) that added XSLoader to the core
# (9cf41c4d23a47c8b and its parent 9426adcd48655815)
# Hence to backport XSLoader to work silently with earlier DynaLoaders we need
# to ensure that the variable exists:

print OUT <<'EOT' if $] < 5.006;

# enable debug/trace messages from DynaLoader perl code
$dl_debug = $ENV{PERL_DL_DEBUG} || 0 unless defined $dl_debug;

EOT

print OUT <<'EOT';
# No prizes for guessing why we don't say 'bootstrap DynaLoader;' here.
# NOTE: All dl_*.xs (including dl_none.xs) define a dl_error() XSUB
boot_DynaLoader('DynaLoader') if defined(&boot_DynaLoader) &&
                                !defined(&dl_error);
package XSLoader;

sub load {
    package DynaLoader;

    my ($module, $modlibname) = caller();

    if (@_) {
        $module = $_[0];
    } else {
        $_[0] = $module;
    }

    # work with static linking too
    my $boots = "$module\::bootstrap";
    goto &$boots if defined &$boots;

    goto \&XSLoader::bootstrap_inherit unless $module and defined &dl_load_file;

    my @modparts = split(/::/,$module);
    my $modfname = $modparts[-1];

EOT

print OUT <<'EOT' if defined &DynaLoader::mod2fname;
    # Some systems have restrictions on files names for DLL's etc.
    # mod2fname returns appropriate file base name (typically truncated)
    # It may also edit @modparts if required.
    $modfname = &mod2fname(\@modparts) if defined &mod2fname;

EOT

print OUT <<'EOT' if $^O eq 'os2';

    # os2 static build can dynaload, but cannot dynaload Perl modules...
    die 'Dynaloaded Perl modules are not available in this build of Perl' if $OS2::is_static;

EOT

print OUT <<'EOT';
    my $modpname = join('/',@modparts);
    my $c = @modparts;
    $modlibname =~ s,[\\/][^\\/]+$,, while $c--;    # Q&D basename
    # Does this look like a relative path?
    if ($modlibname !~ m|^[\\/]|) {
        # Someone may have a #line directive that changes the file name, or
        # may be calling XSLoader::load from inside a string eval.  We cer-
        # tainly do not want to go loading some code that is not in @INC,
        # as it could be untrusted.
        #
        # We could just fall back to DynaLoader here, but then the rest of
        # this function would go untested in the perl core, since all @INC
        # paths are relative during testing.  That would be a time bomb
        # waiting to happen, since bugs could be introduced into the code.
        #
        # So look through @INC to see if $modlibname is in it.  A rela-
        # tive $modlibname is not a common occurrence, so this block is
        # not hot code.
        FOUND: {
            for (@INC) {
                if ($_ eq $modlibname) {
                    last FOUND;
                }
            }
            # Not found.  Fall back to DynaLoader.
            goto \&XSLoader::bootstrap_inherit;
        }
    }
EOT

my $dl_dlext = quotemeta($Config::Config{'dlext'});

print OUT <<"EOT";
    my \$file = "\$modlibname/auto/\$modpname/\$modfname.$dl_dlext";
EOT

print OUT <<'EOT';

#   print STDERR "XSLoader::load for $module ($file)\n" if $dl_debug;

    my $bs = $file;
    $bs =~ s/(\.\w+)?(;\d*)?$/\.bs/; # look for .bs 'beside' the library

    if (-s $bs) { # only read file if it's not empty
#       print STDERR "BS: $bs ($^O, $dlsrc)\n" if $dl_debug;
        eval { do $bs; };
        warn "$bs: $@\n" if $@;
    }

    goto \&XSLoader::bootstrap_inherit if not -f $file or -s $bs;

    my $bootname = "boot_$module";
    $bootname =~ s/\W/_/g;
    @DynaLoader::dl_require_symbols = ($bootname);

    my $boot_symbol_ref;

EOT

    if ($^O eq 'darwin') {
print OUT <<'EOT';
        if ($boot_symbol_ref = dl_find_symbol(0, $bootname)) {
            goto boot; #extension library has already been loaded, e.g. darwin
        }
EOT
    }

print OUT <<'EOT';
    # Many dynamic extension loading problems will appear to come from
    # this section of code: XYZ failed at line 123 of DynaLoader.pm.
    # Often these errors are actually occurring in the initialisation
    # C code of the extension XS file. Perl reports the error as being
    # in this perl code simply because this was the last perl code
    # it executed.

    my $libref = dl_load_file($file, 0) or do { 
        require Carp;
        Carp::croak("Can't load '$file' for module $module: " . dl_error());
    };
    push(@DynaLoader::dl_librefs,$libref);  # record loaded object

    my @unresolved = dl_undef_symbols();
    if (@unresolved) {
        require Carp;
        Carp::carp("Undefined symbols present after loading $file: @unresolved\n");
    }

    $boot_symbol_ref = dl_find_symbol($libref, $bootname) or do {
        require Carp;
        Carp::croak("Can't find '$bootname' symbol in $file\n");
    };

    push(@DynaLoader::dl_modules, $module); # record loaded module

  boot:
    my $xs = dl_install_xsub($boots, $boot_symbol_ref, $file);

    # See comment block above
    push(@DynaLoader::dl_shared_objects, $file); # record files loaded
    return &$xs(@_);
}
EOT

# Can't test with DynaLoader->can('bootstrap_inherit') when building in the
# core, as XSLoader gets built before DynaLoader.

if ($] >= 5.006) {
    print OUT <<'EOT';

sub bootstrap_inherit {
    require DynaLoader;
    goto \&DynaLoader::bootstrap_inherit;
}

EOT
} else {
    print OUT <<'EOT';

sub bootstrap_inherit {
    # Versions of DynaLoader prior to 5.6.0 don't have bootstrap_inherit.
    package DynaLoader;

    my $module = $_[0];
    local *DynaLoader::isa = *{"$module\::ISA"};
    local @DynaLoader::isa = (@DynaLoader::isa, 'DynaLoader');
    # Cannot goto due to delocalization.  Will report errors on a wrong line?
    require DynaLoader;
    DynaLoader::bootstrap(@_);
}

EOT
}

print OUT <<'EOT';
1;


__END__

=head1 NAME

XSLoader - Dynamically load C libraries into Perl code

=head1 VERSION

Version 0.17

=head1 SYNOPSIS

    package YourPackage;
    require XSLoader;

    XSLoader::load();

=head1 DESCRIPTION

This module defines a standard I<simplified> interface to the dynamic
linking mechanisms available on many platforms.  Its primary purpose is
to implement cheap automatic dynamic loading of Perl modules.

For a more complicated interface, see L<DynaLoader>.  Many (most)
features of C<DynaLoader> are not implemented in C<XSLoader>, like for
example the C<dl_load_flags>, not honored by C<XSLoader>.

=head2 Migration from C<DynaLoader>

A typical module using L<DynaLoader|DynaLoader> starts like this:

    package YourPackage;
    require DynaLoader;

    our @ISA = qw( OnePackage OtherPackage DynaLoader );
    our $VERSION = '0.01';
    bootstrap YourPackage $VERSION;

Change this to

    package YourPackage;
    use XSLoader;

    our @ISA = qw( OnePackage OtherPackage );
    our $VERSION = '0.01';
    XSLoader::load 'YourPackage', $VERSION;

In other words: replace C<require DynaLoader> by C<use XSLoader>, remove
C<DynaLoader> from C<@ISA>, change C<bootstrap> by C<XSLoader::load>.  Do not
forget to quote the name of your package on the C<XSLoader::load> line,
and add comma (C<,>) before the arguments (C<$VERSION> above).

Of course, if C<@ISA> contained only C<DynaLoader>, there is no need to have
the C<@ISA> assignment at all; moreover, if instead of C<our> one uses the
more backward-compatible

    use vars qw($VERSION @ISA);

one can remove this reference to C<@ISA> together with the C<@ISA> assignment.

If no C<$VERSION> was specified on the C<bootstrap> line, the last line becomes

    XSLoader::load 'YourPackage';

If the call to C<load> is from C<YourPackage>, then that can be further
simplified to

    XSLoader::load();

as C<load> will use C<caller> to determine the package.

=head2 Backward compatible boilerplate

If you want to have your cake and eat it too, you need a more complicated
boilerplate.

    package YourPackage;
    use vars qw($VERSION @ISA);

    @ISA = qw( OnePackage OtherPackage );
    $VERSION = '0.01';
    eval {
       require XSLoader;
       XSLoader::load('YourPackage', $VERSION);
       1;
    } or do {
       require DynaLoader;
       push @ISA, 'DynaLoader';
       bootstrap YourPackage $VERSION;
    };

The parentheses about C<XSLoader::load()> arguments are needed since we replaced
C<use XSLoader> by C<require>, so the compiler does not know that a function
C<XSLoader::load()> is present.

This boilerplate uses the low-overhead C<XSLoader> if present; if used with
an antique Perl which has no C<XSLoader>, it falls back to using C<DynaLoader>.

=head1 Order of initialization: early load()

I<Skip this section if the XSUB functions are supposed to be called from other
modules only; read it only if you call your XSUBs from the code in your module,
or have a C<BOOT:> section in your XS file (see L<perlxs/"The BOOT: Keyword">).
What is described here is equally applicable to the L<DynaLoader|DynaLoader>
interface.>

A sufficiently complicated module using XS would have both Perl code (defined
in F<YourPackage.pm>) and XS code (defined in F<YourPackage.xs>).  If this
Perl code makes calls into this XS code, and/or this XS code makes calls to
the Perl code, one should be careful with the order of initialization.

The call to C<XSLoader::load()> (or C<bootstrap()>) calls the module's
bootstrap code. For modules build by F<xsubpp> (nearly all modules) this
has three side effects:

=over

=item *

A sanity check is done to ensure that the versions of the F<.pm> and the
(compiled) F<.xs> parts are compatible. If C<$VERSION> was specified, this
is used for the check. If not specified, it defaults to
C<$XS_VERSION // $VERSION> (in the module's namespace)

=item *

the XSUBs are made accessible from Perl

=item *

if a C<BOOT:> section was present in the F<.xs> file, the code there is called.

=back

Consequently, if the code in the F<.pm> file makes calls to these XSUBs, it is
convenient to have XSUBs installed before the Perl code is defined; for
example, this makes prototypes for XSUBs visible to this Perl code.
Alternatively, if the C<BOOT:> section makes calls to Perl functions (or
uses Perl variables) defined in the F<.pm> file, they must be defined prior to
the call to C<XSLoader::load()> (or C<bootstrap()>).

The first situation being much more frequent, it makes sense to rewrite the
boilerplate as

    package YourPackage;
    use XSLoader;
    use vars qw($VERSION @ISA);

    BEGIN {
       @ISA = qw( OnePackage OtherPackage );
       $VERSION = '0.01';

       # Put Perl code used in the BOOT: section here

       XSLoader::load 'YourPackage', $VERSION;
    }

    # Put Perl code making calls into XSUBs here

=head2 The most hairy case

If the interdependence of your C<BOOT:> section and Perl code is
more complicated than this (e.g., the C<BOOT:> section makes calls to Perl
functions which make calls to XSUBs with prototypes), get rid of the C<BOOT:>
section altogether.  Replace it with a function C<onBOOT()>, and call it like
this:

    package YourPackage;
    use XSLoader;
    use vars qw($VERSION @ISA);

    BEGIN {
       @ISA = qw( OnePackage OtherPackage );
       $VERSION = '0.01';
       XSLoader::load 'YourPackage', $VERSION;
    }

    # Put Perl code used in onBOOT() function here; calls to XSUBs are
    # prototype-checked.

    onBOOT;

    # Put Perl initialization code assuming that XS is initialized here


=head1 DIAGNOSTICS

=over

=item C<Can't find '%s' symbol in %s>

B<(F)> The bootstrap symbol could not be found in the extension module.

=item C<Can't load '%s' for module %s: %s>

B<(F)> The loading or initialisation of the extension module failed.
The detailed error follows.

=item C<Undefined symbols present after loading %s: %s>

B<(W)> As the message says, some symbols stay undefined although the
extension module was correctly loaded and initialised. The list of undefined
symbols follows.

=back

=head1 LIMITATIONS

To reduce the overhead as much as possible, only one possible location
is checked to find the extension DLL (this location is where C<make install>
would put the DLL).  If not found, the search for the DLL is transparently
delegated to C<DynaLoader>, which looks for the DLL along the C<@INC> list.

In particular, this is applicable to the structure of C<@INC> used for testing
not-yet-installed extensions.  This means that running uninstalled extensions
may have much more overhead than running the same extensions after
C<make install>.


=head1 KNOWN BUGS

The new simpler way to call C<XSLoader::load()> with no arguments at all
does not work on Perl 5.8.4 and 5.8.5.


=head1 BUGS

Please report any bugs or feature requests via the perlbug(1) utility.


=head1 SEE ALSO

L<DynaLoader>


=head1 AUTHORS

Ilya Zakharevich originally extracted C<XSLoader> from C<DynaLoader>.

CPAN version is currently maintained by SE<eacute>bastien Aperghis-Tramoni
E<lt>sebastien@aperghis.netE<gt>.

Previous maintainer was Michael G Schwern <schwern@pobox.com>.


=head1 COPYRIGHT & LICENSE

Copyright (C) 1990-2011 by Larry Wall and others.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
EOT

close OUT or die $!;
