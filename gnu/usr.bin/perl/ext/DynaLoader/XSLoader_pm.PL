use Config;

sub to_string {
    my ($value) = @_;
    $value =~ s/\\/\\\\/g;
    $value =~ s/'/\\'/g;
    return "'$value'";
}

unlink "XSLoader.pm" if -f "XSLoader.pm";
open OUT, ">XSLoader.pm" or die $!;
print OUT <<'EOT';
# Generated from XSLoader.pm.PL (resolved %Config::Config value)

package XSLoader;

$VERSION = "0.02";

# enable debug/trace messages from DynaLoader perl code
# $dl_debug = $ENV{PERL_DL_DEBUG} || 0 unless defined $dl_debug;

EOT

print OUT '  my $dl_dlext = ', to_string($Config::Config{'dlext'}), ";\n" ;

print OUT <<'EOT';

package DynaLoader;

# No prizes for guessing why we don't say 'bootstrap DynaLoader;' here.
# NOTE: All dl_*.xs (including dl_none.xs) define a dl_error() XSUB
boot_DynaLoader('DynaLoader') if defined(&boot_DynaLoader) &&
                                !defined(&dl_error);
package XSLoader;

sub load {
    package DynaLoader;

    die q{XSLoader::load('Your::Module', $Your::Module::VERSION)} unless @_;

    my($module) = $_[0];

    # work with static linking too
    my $b = "$module\::bootstrap";
    goto &$b if defined &$b;

    goto retry unless $module and defined &dl_load_file;

    my @modparts = split(/::/,$module);
    my $modfname = $modparts[-1];

EOT

print OUT <<'EOT' if defined &DynaLoader::mod2fname;
    # Some systems have restrictions on files names for DLL's etc.
    # mod2fname returns appropriate file base name (typically truncated)
    # It may also edit @modparts if required.
    $modfname = &mod2fname(\@modparts) if defined &mod2fname;

EOT

print OUT <<'EOT';
    my $modpname = join('/',@modparts);
    my $modlibname = (caller())[1];
    my $c = @modparts;
    $modlibname =~ s,[\\/][^\\/]+$,, while $c--;	# Q&D basename
    my $file = "$modlibname/auto/$modpname/$modfname.$dl_dlext";

#   print STDERR "XSLoader::load for $module ($file)\n" if $dl_debug;

    my $bs = $file;
    $bs =~ s/(\.\w+)?(;\d*)?$/\.bs/; # look for .bs 'beside' the library

    goto retry if not -f $file or -s $bs;

    my $bootname = "boot_$module";
    $bootname =~ s/\W/_/g;
    @dl_require_symbols = ($bootname);

    my $boot_symbol_ref;

    if ($^O eq 'darwin') {
        if ($boot_symbol_ref = dl_find_symbol(0, $bootname)) {
            goto boot; #extension library has already been loaded, e.g. darwin
        }
    }

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
    push(@dl_librefs,$libref);  # record loaded object

    my @unresolved = dl_undef_symbols();
    if (@unresolved) {
	require Carp;
	Carp::carp("Undefined symbols present after loading $file: @unresolved\n");
    }

    $boot_symbol_ref = dl_find_symbol($libref, $bootname) or do {
	require Carp;
	Carp::croak("Can't find '$bootname' symbol in $file\n");
    };

    push(@dl_modules, $module); # record loaded module

  boot:
    my $xs = dl_install_xsub("${module}::bootstrap", $boot_symbol_ref, $file);

    # See comment block above
    return &$xs(@_);

  retry:
    require DynaLoader;
    goto &DynaLoader::bootstrap_inherit;
}

1;

__END__

=head1 NAME

XSLoader - Dynamically load C libraries into Perl code

=head1 SYNOPSIS

    package YourPackage;
    use XSLoader;

    XSLoader::load 'YourPackage', $YourPackage::VERSION;

=head1 DESCRIPTION

This module defines a standard I<simplified> interface to the dynamic
linking mechanisms available on many platforms.  Its primary purpose is
to implement cheap automatic dynamic loading of Perl modules.

For more complicated interface see L<DynaLoader>.  Many (most)
features of DynaLoader are not implemented in XSLoader, like for
example the dl_load_flags is not honored by XSLoader.

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
C<DynaLoader> from @ISA, change C<bootstrap> by C<XSLoader::load>.  Do not
forget to quote the name of your package on the C<XSLoader::load> line,
and add comma (C<,>) before the arguments ($VERSION above).

Of course, if @ISA contained only C<DynaLoader>, there is no need to have the
@ISA assignment at all; moreover, if instead of C<our> one uses
backward-compatible

    use vars qw($VERSION @ISA);

one can remove this reference to @ISA together with the @ISA assignment

If no $VERSION was specified on the C<bootstrap> line, the last line becomes

    XSLoader::load 'YourPackage';

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

The parentheses about XSLoader::load() arguments are needed since we replaced
C<use XSLoader> by C<require>, so the compiler does not know that a function
XSLoader::load() is present.

This boilerplate uses the low-overhead C<XSLoader> if present; if used with
an antic Perl which has no C<XSLoader>, it falls back to using C<DynaLoader>.

=head1 Order of initialization: early load()

I<Skip this section if the XSUB functions are supposed to be called from other
modules only; read it only if you call your XSUBs from the code in your module,
or have a C<BOOT:> section in your XS file (see L<perlxs/"The BOOT: Keyword">).
What is described here is equally applicable to L<DynaLoader|DynaLoader>
interface.>

A sufficiently complicated module using XS would have both Perl code (defined
in F<YourPackage.pm>) and XS code (defined in F<YourPackage.xs>).  If this
Perl code makes calls into this XS code, and/or this XS code makes calls to
the Perl code, one should be careful with the order of initialization.

The call to XSLoader::load() (or bootstrap()) has three side effects:

=over

=item *

if $VERSION was specified, a sanity check is done to insure that the versions
of the F<.pm> and the (compiled) F<.xs> parts are compatible;

=item *

The XSUBs are made accessible from Perl;

=item *

If the C<BOOT:> section was present in F<.xs> file, the code there is called.

=back

Consequently, if the code in F<.pm> file makes calls to these XSUBs, it is
convenient to have XSUBs installed before the Perl code is defined; for
example, this makes prototypes for XSUBs visible to this Perl code.
Alternatively, if the C<BOOT:> section makes calls to Perl functions (or
uses Perl variables) defined in F<.pm> file, they must be defined prior to
the call to XSLoader::load() (or bootstrap()).

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
section altogether.  Replace it with a function onBOOT(), and call it like
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

=head1 LIMITATIONS

To reduce the overhead as much as possible, only one possible location
is checked to find the extension DLL (this location is where C<make install>
would put the DLL).  If not found, the search for the DLL is transparently
delegated to C<DynaLoader>, which looks for the DLL along the @INC list.

In particular, this is applicable to the structure of @INC used for testing
not-yet-installed extensions.  This means that the overhead of running
uninstalled extension may be much more than running the same extension after
C<make install>.

=head1 AUTHOR

Ilya Zakharevich: extraction from DynaLoader.

=cut
EOT

close OUT or die $!;

