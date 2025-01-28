package Test2::Util::Importer;
use strict; no strict 'refs';
use warnings; no warnings 'once';

our $VERSION = '0.000162';

my %SIG_TO_SLOT = (
    '&' => 'CODE',
    '$' => 'SCALAR',
    '%' => 'HASH',
    '@' => 'ARRAY',
    '*' => 'GLOB',
);

our %IMPORTED;

# This will be used to check if an import arg is a version number
my %NUMERIC = map +($_ => 1), 0 .. 9;

sub IMPORTER_MENU() {
    return (
        export_ok   => [qw/optimal_import/],
        export_anon => {
            import => sub {
                my $from  = shift;
                my @caller = caller(0);

                _version_check($from, \@caller, shift @_) if @_ && $NUMERIC{substr($_[0], 0, 1)};

                my $file = _mod_to_file($from);
                _load_file(\@caller, $file) unless $INC{$file};

                return if optimal_import($from, $caller[0], \@caller, @_);

                my $self = __PACKAGE__->new(
                    from   => $from,
                    caller => \@caller,
                );

                $self->do_import($caller[0], @_);
            },
        },
    );
}

###########################################################################
#
# These are class methods
# import and unimport are what you would expect.
# import_into and unimport_from are the indirect forms you can use in other
# package import() methods.
#
# These all attempt to do a fast optimal-import if possible, then fallback to
# the full-featured import that constructs an object when needed.
#

sub import {
    my $class = shift;

    my @caller = caller(0);

    _version_check($class, \@caller, shift @_) if @_ && $NUMERIC{substr($_[0], 0, 1)};

    return unless @_;

    my ($from, @args) = @_;

    my $file = _mod_to_file($from);
    _load_file(\@caller, $file) unless $INC{$file};

    return if optimal_import($from, $caller[0], \@caller, @args);

    my $self = $class->new(
        from   => $from,
        caller => \@caller,
    );

    $self->do_import($caller[0], @args);
}

sub unimport {
    my $class = shift;
    my @caller = caller(0);

    my $self = $class->new(
        from   => $caller[0],
        caller => \@caller,
    );

    $self->do_unimport(@_);
}

sub import_into {
    my $class = shift;
    my ($from, $into, @args) = @_;

    my @caller;

    if (ref($into)) {
        @caller = @$into;
        $into = $caller[0];
    }
    elsif ($into =~ m/^\d+$/) {
        @caller = caller($into + 1);
        $into = $caller[0];
    }
    else {
        @caller = caller(0);
    }

    my $file = _mod_to_file($from);
    _load_file(\@caller, $file) unless $INC{$file};

    return if optimal_import($from, $into, \@caller, @args);

    my $self = $class->new(
        from   => $from,
        caller => \@caller,
    );

    $self->do_import($into, @args);
}

sub unimport_from {
    my $class = shift;
    my ($from, @args) = @_;

    my @caller;
    if ($from =~ m/^\d+$/) {
        @caller = caller($from + 1);
        $from = $caller[0];
    }
    else {
        @caller = caller(0);
    }

    my $self = $class->new(
        from   => $from,
        caller => \@caller,
    );

    $self->do_unimport(@args);
}

###########################################################################
#
# Constructors
#

sub new {
    my $class = shift;
    my %params = @_;

    my $caller = $params{caller} || [caller()];

    die "You must specify a package to import from at $caller->[1] line $caller->[2].\n"
        unless $params{from};

    return bless {
        from   => $params{from},
        caller => $params{caller},    # Do not use our caller.
    }, $class;
}

###########################################################################
#
# Shortcuts for getting symbols without any namespace modifications
#

sub get {
    my $proto = shift;
    my @caller = caller(1);

    my $self = ref($proto) ? $proto : $proto->new(
        from   => shift(@_),
        caller => \@caller,
    );

    my %result;
    $self->do_import($caller[0], @_, sub { $result{$_[0]} = $_[1] });
    return \%result;
}

sub get_list {
    my $proto = shift;
    my @caller = caller(1);

    my $self = ref($proto) ? $proto : $proto->new(
        from   => shift(@_),
        caller => \@caller,
    );

    my @result;
    $self->do_import($caller[0], @_, sub { push @result => $_[1] });
    return @result;
}

sub get_one {
    my $proto = shift;
    my @caller = caller(1);

    my $self = ref($proto) ? $proto : $proto->new(
        from   => shift(@_),
        caller => \@caller,
    );

    my $result;
    $self->do_import($caller[0], @_, sub { $result = $_[1] });
    return $result;
}

###########################################################################
#
# Object methods
#

sub do_import {
    my $self = shift;

    my ($into, $versions, $exclude, $import, $set) = $self->parse_args(@_);

    # Exporter supported multiple version numbers being listed...
    _version_check($self->from, $self->get_caller, @$versions) if @$versions;

    return unless @$import;

    $self->_handle_fail($into, $import) if $self->menu($into)->{fail};
    $self->_set_symbols($into, $exclude, $import, $set);
}

sub do_unimport {
    my $self = shift;

    my $from = $self->from;
    my $imported = $IMPORTED{$from} or $self->croak("'$from' does not have any imports to remove");

    my %allowed = map { $_ => 1 } @$imported;

    my @args = @_ ? @_ : @$imported;

    my $stash = \%{"$from\::"};

    for my $name (@args) {
        $name =~ s/^&//;

        $self->croak("Sub '$name' was not imported using " . ref($self)) unless $allowed{$name};

        my $glob = delete $stash->{$name};
        local *GLOBCLONE = *$glob;

        for my $type (qw/SCALAR HASH ARRAY FORMAT IO/) {
            next unless defined(*{$glob}{$type});
            *{"$from\::$name"} = *{$glob}{$type}
        }
    }
}

sub from { $_[0]->{from} }

sub from_file {
    my $self = shift;

    $self->{from_file} ||= _mod_to_file($self->{from});

    return $self->{from_file};
}

sub load_from {
    my $self = shift;
    my $from_file = $self->from_file;
    my $this_file = __FILE__;

    return if $INC{$from_file};

    my $caller = $self->get_caller;

    _load_file($caller, $from_file);
}

sub get_caller {
    my $self = shift;
    return $self->{caller} if $self->{caller};

    my $level = 1;
    while(my @caller = caller($level++)) {
        return \@caller if @caller && !$caller[0]->isa(__PACKAGE__);
        last unless @caller;
    }

    # Fallback
    return [caller(0)];
}

sub croak {
    my $self = shift;
    my ($msg) = @_;
    my $caller = $self->get_caller;
    my $file = $caller->[1] || 'unknown file';
    my $line = $caller->[2] || 'unknown line';
    die "$msg at $file line $line.\n";
}

sub carp {
    my $self = shift;
    my ($msg) = @_;
    my $caller = $self->get_caller;
    my $file = $caller->[1] || 'unknown file';
    my $line = $caller->[2] || 'unknown line';
    warn "$msg at $file line $line.\n";
}

sub menu {
    my $self = shift;
    my ($into) = @_;

    $self->croak("menu() requires the name of the destination package")
        unless $into;

    my $for = $self->{menu_for};
    delete $self->{menu} if $for && $for ne $into;
    return $self->{menu} || $self->reload_menu($into);
}

sub reload_menu {
    my $self = shift;
    my ($into) = @_;

    $self->croak("reload_menu() requires the name of the destination package")
        unless $into;

    my $from = $self->from;

    if (my $menu_sub = *{"$from\::IMPORTER_MENU"}{CODE}) {
        # Hook, other exporter modules can define this method to be compatible with
        # Importer.pm

        my %got = $from->$menu_sub($into, $self->get_caller);

        $got{export}       ||= [];
        $got{export_ok}    ||= [];
        $got{export_tags}  ||= {};
        $got{export_fail}  ||= [];
        $got{export_anon}  ||= {};
        $got{export_magic} ||= {};

        $self->croak("'$from' provides both 'generate' and 'export_gen' in its IMPORTER_MENU (They are exclusive, module must pick 1)")
            if $got{export_gen} && $got{generate};

        $got{export_gen} ||= {};

        $self->{menu} = $self->_build_menu($into => \%got, 1);
    }
    else {
        my %got;
        $got{export}        = \@{"$from\::EXPORT"};
        $got{export_ok}     = \@{"$from\::EXPORT_OK"};
        $got{export_tags}   = \%{"$from\::EXPORT_TAGS"};
        $got{export_fail}   = \@{"$from\::EXPORT_FAIL"};
        $got{export_gen}    = \%{"$from\::EXPORT_GEN"};
        $got{export_anon}   = \%{"$from\::EXPORT_ANON"};
        $got{export_magic}  = \%{"$from\::EXPORT_MAGIC"};

        $self->{menu} = $self->_build_menu($into => \%got, 0);
    }

    $self->{menu_for} = $into;

    return $self->{menu};
}

sub _build_menu {
    my $self = shift;
    my ($into, $got, $new_style) = @_;

    my $from = $self->from;

    my $export       = $got->{export}       || [];
    my $export_ok    = $got->{export_ok}    || [];
    my $export_tags  = $got->{export_tags}  || {};
    my $export_fail  = $got->{export_fail}  || [];
    my $export_anon  = $got->{export_anon}  || {};
    my $export_gen   = $got->{export_gen}   || {};
    my $export_magic = $got->{export_magic} || {};

    my $generate = $got->{generate};

    $generate ||= sub {
        my $symbol = shift;
        my ($sig, $name) = ($symbol =~ m/^(\W?)(.*)$/);
        $sig ||= '&';

        my $do = $export_gen->{"${sig}${name}"};
        $do ||= $export_gen->{$name} if !$sig || $sig eq '&';

        return undef unless $do;

        $from->$do($into, $symbol);
    } if $export_gen && keys %$export_gen;

    my $lookup  = {};
    my $exports = {};
    for my $sym (@$export, @$export_ok, keys %$export_gen, keys %$export_anon) {
        my ($sig, $name) = ($sym =~ m/^(\W?)(.*)$/);
        $sig ||= '&';

        $lookup->{"${sig}${name}"} = 1;
        $lookup->{$name} = 1 if $sig eq '&';

        next if $export_gen->{"${sig}${name}"};
        next if $sig eq '&' && $export_gen->{$name};
        next if $got->{generate} && $generate->("${sig}${name}");

        my $fqn = "$from\::$name";
        # We cannot use *{$fqn}{TYPE} here, it breaks for autoloaded subs, this
        # does not:
        $exports->{"${sig}${name}"} = $export_anon->{$sym} || (
            $sig eq '&' ? \&{$fqn} :
            $sig eq '$' ? \${$fqn} :
            $sig eq '@' ? \@{$fqn} :
            $sig eq '%' ? \%{$fqn} :
            $sig eq '*' ? \*{$fqn} :
            # Sometimes people (CGI::Carp) put invalid names (^name=) into
            # @EXPORT. We simply go to 'next' in these cases. These modules
            # have hooks to prevent anyone actually trying to import these.
            next
        );
    }

    my $f_import = $new_style || $from->can('import');
    $self->croak("'$from' does not provide any exports")
        unless $new_style
            || keys %$exports
            || $from->isa('Exporter')
            || ($INC{'Exporter.pm'} && $f_import && $f_import == \&Exporter::import);

    # Do not cleanup or normalize the list added to the DEFAULT tag, legacy....
    my $tags = {
        %$export_tags,
        'DEFAULT' => [ @$export ],
    };

    # Add 'ALL' tag unless already specified. We want to normalize it.
    $tags->{ALL} ||= [ sort grep {m/^[\&\$\@\%\*]/} keys %$lookup ];

    my $fail = @$export_fail ? {
        map {
            my ($sig, $name) = (m/^(\W?)(.*)$/);
            $sig ||= '&';
            ("${sig}${name}" => 1, $sig eq '&' ? ($name => 1) : ())
        } @$export_fail
    } : undef;

    my $menu = {
        lookup   => $lookup,
        exports  => $exports,
        tags     => $tags,
        fail     => $fail,
        generate => $generate,
        magic    => $export_magic,
    };

    return $menu;
}

sub parse_args {
    my $self = shift;
    my ($into, @args) = @_;

    my $menu = $self->menu($into);

    my @out = $self->_parse_args($into, $menu, \@args);
    pop @out;
    return @out;
}

sub _parse_args {
    my $self = shift;
    my ($into, $menu, $args, $is_tag) = @_;

    my $from = $self->from;
    my $main_menu = $self->menu($into);
    $menu ||= $main_menu;

    # First we strip out versions numbers and setters, this simplifies the logic late.
    my @sets;
    my @versions;
    my @leftover;
    for my $arg (@$args) {
        no warnings 'void';

        # Code refs are custom setters
        # If the first character is an ASCII numeric then it is a version number
        push @sets     => $arg and next if ref($arg) eq 'CODE';
        push @versions => $arg xor next if $NUMERIC{substr($arg, 0, 1)};
        push @leftover => $arg;
    }

    $self->carp("Multiple setters specified, only 1 will be used") if @sets > 1;
    my $set = pop @sets;

    $args = \@leftover;
    @$args = (':DEFAULT') unless $is_tag || @$args || @versions;

    my %exclude;
    my @import;

    while(my $full_arg = shift @$args) {
        my $arg = $full_arg;
        my $lead = substr($arg, 0, 1);

        my ($spec, $exc);
        if ($lead eq '!') {
            $exc = $lead;

            if ($arg eq '!') {
                # If the current arg is just '!' then we are negating the next item.
                $arg = shift @$args;
            }
            else {
                # Strip off the '!'
                substr($arg, 0, 1, '');
            }

            # Exporter.pm legacy behavior
            # negated first item implies starting with default set:
            unshift @$args => ':DEFAULT' unless @import || keys %exclude || @versions;

            # Now we have a new lead character
            $lead = substr($arg, 0, 1);
        }
        else {
            # If the item is followed by a reference then they are asking us to
            # do something special...
            $spec = ref($args->[0]) eq 'HASH' ? shift @$args : {};
        }

        if($lead eq ':') {
            substr($arg, 0, 1, '');
            my $tag = $menu->{tags}->{$arg} or $self->croak("$from does not export the :$arg tag");

            my (undef, $cvers, $cexc, $cimp, $cset, $newmenu) = $self->_parse_args($into, $menu, $tag, $arg);

            $self->croak("Exporter specified version numbers (" . join(', ', @$cvers) . ") in the :$arg tag!")
                if @$cvers;

            $self->croak("Exporter specified a custom symbol setter in the :$arg tag!")
                if $cset;

            # Merge excludes
            %exclude = (%exclude, %$cexc);

            if ($exc) {
                $exclude{$_} = 1 for grep {!ref($_) && substr($_, 0, 1) ne '+'} map {$_->[0]} @$cimp;
            }
            elsif ($spec && keys %$spec) {
                $self->croak("Cannot use '-as' to rename multiple symbols included by: $full_arg")
                    if $spec->{'-as'} && @$cimp > 1;

                for my $set (@$cimp) {
                    my ($sym, $cspec) = @$set;

                    # Start with a blind squash, spec from tag overrides the ones inside.
                    my $nspec = {%$cspec, %$spec};

                    $nspec->{'-prefix'}  = "$spec->{'-prefix'}$cspec->{'-prefix'}"   if $spec->{'-prefix'}  && $cspec->{'-prefix'};
                    $nspec->{'-postfix'} = "$cspec->{'-postfix'}$spec->{'-postfix'}" if $spec->{'-postfix'} && $cspec->{'-postfix'};

                    push @import => [$sym, $nspec];
                }
            }
            else {
                push @import => @$cimp;
            }

            # New menu
            $menu = $newmenu;

            next;
        }

        # Process the item to figure out what symbols are being touched, if it
        # is a tag or regex than it can be multiple.
        my @list;
        if(ref($arg) eq 'Regexp') {
            @list = sort grep /$arg/, keys %{$menu->{lookup}};
        }
        elsif($lead eq '/' && $arg =~ m{^/(.*)/$}) {
            my $pattern = $1;
            @list = sort grep /$1/, keys %{$menu->{lookup}};
        }
        else {
            @list = ($arg);
        }

        # Normalize list, always have a sigil
        @list = map {m/^\W/ ? $_ : "\&$_" } @list;

        if ($exc) {
            $exclude{$_} = 1 for @list;
        }
        else {
            $self->croak("Cannot use '-as' to rename multiple symbols included by: $full_arg")
                if $spec->{'-as'} && @list > 1;

            push @import => [$_, $spec] for @list;
        }
    }

    return ($into, \@versions, \%exclude, \@import, $set, $menu);
}

sub _handle_fail {
    my $self = shift;
    my ($into, $import) = @_;

    my $from = $self->from;
    my $menu = $self->menu($into);

    # Historically Exporter would strip the '&' off of sub names passed into export_fail.
    my @fail = map {my $x = $_->[0]; $x =~ s/^&//; $x} grep $menu->{fail}->{$_->[0]}, @$import or return;

    my @real_fail = $from->can('export_fail') ? $from->export_fail(@fail) : @fail;

    if (@real_fail) {
        $self->carp(qq["$_" is not implemented by the $from module on this architecture])
            for @real_fail;

        $self->croak("Can't continue after import errors");
    }

    $self->reload_menu($menu);
    return;
}

sub _set_symbols {
    my $self = shift;
    my ($into, $exclude, $import, $custom_set) = @_;

    my $from   = $self->from;
    my $menu   = $self->menu($into);
    my $caller = $self->get_caller();

    my $set_symbol = $custom_set || eval <<"    EOT" || die $@;
# Inherit the callers warning settings. If they have warnings and we
# redefine their subs they will hear about it. If they do not have warnings
# on they will not.
BEGIN { \${^WARNING_BITS} = \$caller->[9] if defined \$caller->[9] }
#line $caller->[2] "$caller->[1]"
sub { *{"$into\\::\$_[0]"} = \$_[1] }
    EOT

    for my $set (@$import) {
        my ($symbol, $spec) = @$set;

        my ($sig, $name) = ($symbol =~ m/^(\W)(.*)$/) or die "Invalid symbol: $symbol";

        # Find the thing we are actually shoving in a new namespace
        my $ref = $menu->{exports}->{$symbol};
        $ref ||= $menu->{generate}->($symbol) if $menu->{generate};

        # Exporter.pm supported listing items in @EXPORT that are not actually
        # available for export. So if it is listed (lookup) but nothing is
        # there (!$ref) we simply skip it.
        $self->croak("$from does not export $symbol") unless $ref || $menu->{lookup}->{"${sig}${name}"};
        next unless $ref;

        my $type = ref($ref);
        $type = 'SCALAR' if $type eq 'REF';
        $self->croak("Symbol '$sig$name' requested, but reference (" . ref($ref) . ") does not match sigil ($sig)")
            if $ref && $type ne $SIG_TO_SLOT{$sig};

        # If they directly renamed it then we assume they want it under the new
        # name, otherwise excludes get kicked. It is useful to be able to
        # exclude an item in a tag/match where the group has a prefix/postfix.
        next if $exclude->{"${sig}${name}"} && !$spec->{'-as'};

        my $new_name = join '' => ($spec->{'-prefix'} || '', $spec->{'-as'} || $name, $spec->{'-postfix'} || '');

        # Set the symbol (finally!)
        $set_symbol->($new_name, $ref, sig => $sig, symbol => $symbol, into => $into, from => $from, spec => $spec);

        # The remaining things get skipped with a custom setter
        next if $custom_set;

        # Record the import so that we can 'unimport'
        push @{$IMPORTED{$into}} => $new_name if $sig eq '&';

        # Apply magic
        my $magic = $menu->{magic}->{$symbol};
        $magic  ||= $menu->{magic}->{$name} if $sig eq '&';
        $from->$magic(into => $into, orig_name => $name, new_name => $new_name, ref => $ref)
            if $magic;
    }
}

###########################################################################
#
# The rest of these are utility functions, not methods!
#

sub _version_check {
    my ($mod, $caller, @versions) = @_;

    eval <<"    EOT" or die $@;
#line $caller->[2] "$caller->[1]"
\$mod->VERSION(\$_) for \@versions;
1;
    EOT
}

sub _mod_to_file {
    my $file = shift;
    $file =~ s{::}{/}g;
    $file .= '.pm';
    return $file;
}

sub _load_file {
    my ($caller, $file) = @_;

    eval <<"    EOT" || die $@;
#line $caller->[2] "$caller->[1]"
require \$file;
    EOT
}


my %HEAVY_VARS = (
    IMPORTER_MENU => 'CODE',     # Origin package has a custom menu
    EXPORT_FAIL   => 'ARRAY',    # Origin package has a failure handler
    EXPORT_GEN    => 'HASH',     # Origin package has generators
    EXPORT_ANON   => 'HASH',     # Origin package has anonymous exports
    EXPORT_MAGIC  => 'HASH',     # Origin package has magic to apply post-export
);

sub optimal_import {
    my ($from, $into, $caller, @args) = @_;

    defined(*{"$from\::$_"}{$HEAVY_VARS{$_}}) and return 0 for keys %HEAVY_VARS;

    # Default to @EXPORT
    @args = @{"$from\::EXPORT"} unless @args;

    # Subs will be listed without sigil in %allowed, all others keep sigil
    my %allowed = map +(substr($_, 0, 1) eq '&' ? substr($_, 1) : $_ => 1),
        @{"$from\::EXPORT"}, @{"$from\::EXPORT_OK"};

    # First check if it is allowed, stripping '&' if necessary, which will also
    # let scalars in, we will deal with those shortly.
    # If not allowed return 0 (need to do a heavy import)
    # if it is allowed then see if it has a CODE slot, if so use it, otherwise
    # we have a symbol that needs heavy due to non-sub, autoload, etc.
    # This will not allow $foo to import foo() since '$from' still contains the
    # sigil making it an invalid symbol name in our globref below.
    my %final = map +(
        (!ref($_) && ($allowed{$_} || (substr($_, 0, 1, "") eq '&' && $allowed{$_})))
            ? ($_ => *{"$from\::$_"}{CODE} || return 0)
            : return 0
    ), @args;

    eval <<"    EOT" || die $@;
# If the caller has redefine warnings enabled then we want to warn them if
# their import redefines things.
BEGIN { \${^WARNING_BITS} = \$caller->[9] if defined \$caller->[9] };
#line $caller->[2] "$caller->[1]"
(*{"$into\\::\$_"} = \$final{\$_}, push \@{\$Test2::Util::Importer::IMPORTED{\$into}} => \$_) for keys %final;
1;
    EOT
}

1;

__END__

=pod

=encoding UTF-8

=head1 NAME

Test2::Util::Importer - Inline copy of L<Importer>.

=head1 DESCRIPTION

See L<Importer>.

=head1 MAINTAINERS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 AUTHORS

=over 4

=item Chad Granum E<lt>exodist@cpan.orgE<gt>

=back

=head1 COPYRIGHT

Copyright 2023 Chad Granum E<lt>exodist7@gmail.comE<gt>.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See L<http://dev.perl.org/licenses/>

=cut
