package ExtUtils::ParseXS::Node;
use strict;
use warnings;

our $VERSION = '3.57';

=head1 NAME

ExtUtils::ParseXS::Node - Classes for nodes of an ExtUtils::ParseXS AST

=head1 SYNOPSIS

XXX TBC

=head1 DESCRIPTION

XXX Sept 2024: this is Work In Progress. This API is currently private and
subject to change. Most of ParseXS doesn't use an AST, and instead
maintains just enough state to emit code as it parses. This module
represents the start of an effort to make it use an AST instead.

An C<ExtUtils::ParseXS::Node> class, and its various subclasses, hold the
state for the nodes of an Abstract Syntax Tree (AST), which represents the
parsed state of an XS file.

Each node is basically a hash of fields. Which field names are legal
varies by the node type. The hash keys and values can be accessed
directly: there are no getter/setter methods.

=cut


# ======================================================================

package ExtUtils::ParseXS::Node;

# Base class for all the other node types.
#
# The 'use fields' enables compile-time or run-time errors if code
# attempts to use a key which isn't listed here.

my $USING_FIELDS;

BEGIN {
    our @FIELDS = (
        # Currently there are no node fields common to all node types
    );

    # do 'use fields', except: fields needs Hash::Util which is XS, which
    # needs us. So only 'use fields' on systems where Hash::Util has already
    # been built.
    if (eval 'require Hash::Util; 1;') {
        require fields;
        $USING_FIELDS = 1;
        fields->import(@FIELDS);
    }
}


# new(): takes one optional arg, $args, which is a hash ref of key/value
# pairs to initialise the object with.

sub new {
    my ($class, $args) = @_;
    $args = {} unless defined $args;

    my ExtUtils::ParseXS::Node $self;
    if ($USING_FIELDS) {
        $self = fields::new($class);
        %$self = %$args;
    }
    else {
        $self = bless { %$args } => $class;
    }
    return $self;
}


# ======================================================================

package ExtUtils::ParseXS::Node::Param;

# Node subclass which holds the state of one XSUB parameter, based on the
# XSUB's signature and/or an INPUT line.

BEGIN {
    our @ISA = qw(ExtUtils::ParseXS::Node);

    our @FIELDS = (
        @ExtUtils::ParseXS::Node::FIELDS,

        # values derived from the XSUB's signature
        'in_out',    # The IN/OUT/OUTLIST etc value (if any)
        'var',       # the name of the parameter
        'arg_num',   # The arg number (starting at 1) mapped to this param
        'default',   # default value (if any)
        'default_usage', # how to report default value in "usage:..." error
        'is_ansi',   # param's type was specified in signature
        'is_length', # param is declared as 'length(foo)' in signature
        'len_name' , # the 'foo' in 'length(foo)' in signature
        'is_synthetic',# var like 'THIS' - we pretend it was in the sig

        # values derived from both the XSUB's signature and/or INPUT line
        'type',      # The C type of the parameter
        'no_init',   # don't initialise the parameter

        # values derived from the XSUB's INPUT line
        'init_op',   # initialisation type: one of =/+/;
        'init',      # initialisation template code
        'is_addr',   # INPUT var declared as '&foo'
        'is_alien',  # var declared in INPUT line, but not in signature
        'in_input',  # the parameter has appeared in an INPUT statement

        # values derived from the XSUB's OUTPUT line
        'in_output',   # the parameter has appeared in an OUTPUT statement
        'do_setmagic', # 'SETMAGIC: ENABLE' was active for this parameter
        'output_code', # the optional setting-code for this parameter

        # derived values calculated later
        'defer',     # deferred initialisation template code
        'proto',     # overridden prototype char(s) (if any) from typemap
    );

    fields->import(@FIELDS) if $USING_FIELDS;
}



# check(): for a parsed INPUT line and/or typed parameter in a signature,
# update some global state and do some checks
#
# Return true if checks pass.

sub check {
    my ExtUtils::ParseXS::Node::Param $self = shift;
    my ExtUtils::ParseXS              $pxs  = shift;
  
    my $type = $self->{type};

    # Get the overridden prototype character, if any, associated with the
    # typemap entry for this var's type.
    # Note that something with a provisional type such as THIS can get
    # the type changed later. It is important to update each time.
    # It also can't be looked up only at BOOT code emitting time, because
    # potentiall, the typmap may been bee updated last in the XS file
    # after the XSUB was parsed.
    if ($self->{arg_num}) {
        my $typemap = $pxs->{typemaps_object}->get_typemap(ctype => $type);
        my $p = $typemap && $typemap->proto;
        $self->{proto} = $p if defined $p && length $p;
    }
  
    return 1;
}


# $self->as_code():
#
# Emit the param object as C code which declares and initialise the variable.
# See also the as_output_code() method, which emits code to return the value
# of that local var.

sub as_code {
    my ExtUtils::ParseXS::Node::Param $self = shift;
    my ExtUtils::ParseXS              $pxs  = shift;
  
    my ($type, $arg_num, $var, $init, $no_init, $defer, $default)
        = @{$self}{qw(type arg_num var init no_init defer default)};
  
    my $arg = $pxs->ST($arg_num);
  
    if ($self->{is_length}) {
        # Process length(foo) parameter.
        # Basically for something like foo(char *s, int length(s)),
        # create *two* local C vars: one with STRLEN type, and one with the
        # type specified in the signature. Eventually, generate code looking
        # something like:
        #   STRLEN  STRLEN_length_of_s;
        #   int     XSauto_length_of_s;
        #   char *s = (char *)SvPV(ST(0), STRLEN_length_of_s);
        #   XSauto_length_of_s = STRLEN_length_of_s;
        #   RETVAL = foo(s, XSauto_length_of_s);
        #
        # Note that the SvPV() code line is generated via a separate call to
        # this sub with s as the var (as opposed to *this* call, which is
        # handling length(s)), by overriding the normal T_PV typemap (which
        # uses PV_nolen()).
  
        my $name = $self->{len_name};
  
        print "\tSTRLEN\tSTRLEN_length_of_$name;\n";
        # defer this line until after all the other declarations
        $pxs->{xsub_deferred_code_lines} .=
                "\n\tXSauto_length_of_$name = STRLEN_length_of_$name;\n";
  
        # this var will be declared using the normal typemap mechanism below
        $var = "XSauto_length_of_$name";
    }
  
    # Emit the variable's type and name.
    #
    # Includes special handling for function pointer types. An INPUT line
    # always has the C type followed by the variable name. The C code
    # which is emitted normally follows the same pattern. However for
    # function pointers, the code is different: the variable name has to
    # be embedded *within* the type. For example, these two INPUT lines:
    #
    #    char *        s
    #    int (*)(int)  fn_ptr
    #
    # cause the following lines of C to be emitted;
    #
    #    char *              s = [something from a typemap]
    #    int (* fn_ptr)(int)   = [something from a typemap]
    #
    # So handle specially the specific case of a type containing '(*)' by
    # embedding the variable name *within* rather than *after* the type.
  
  
    if ($type =~ / \( \s* \* \s* \) /x) {
        # for a fn ptr type, embed the var name in the type declaration
        print "\t" . $pxs->map_type($type, $var);
    }
    else {
        print "\t",
                    ((defined($pxs->{xsub_class}) && $var eq 'CLASS')
                        ? $type
                        : $pxs->map_type($type, undef)),
              "\t$var";
    }
  
    # whitespace-tidy the type
    $type = ExtUtils::Typemaps::tidy_type($type);
  
    # Specify the environment for when the initialiser template is evaled.
    # Only the common ones are specified here. Other fields may be added
    # later.
    my $eval_vars = {
        type          => $type,
        var           => $var,
        num           => $arg_num,
        arg           => $arg,
    };
  
    # The type looked up in the eval is Foo__Bar rather than Foo::Bar
    $eval_vars->{type} =~ tr/:/_/
        unless $pxs->{config_RetainCplusplusHierarchicalTypes};
  
    my $init_template;
  
    if (defined $init) {
        # Use the supplied code template rather than getting it from the
        # typemap
  
        $pxs->death(
                "Internal error: ExtUtils::ParseXS::Node::Param::as_code(): "
              . "both init and no_init supplied")
            if $no_init;
  
        $eval_vars->{init} = $init;
        $init_template = "\$var = $init";
    }
    elsif ($no_init) {
        # don't add initialiser
        $init_template = "";
    }
    else {
        # Get the initialiser template from the typemap
  
        my $typemaps = $pxs->{typemaps_object};
  
        # Normalised type ('Foo *' becomes 'FooPtr): one of the valid vars
        # which can appear within a typemap template.
        (my $ntype = $type) =~ s/\s*\*/Ptr/g;
  
        # $subtype is really just for the T_ARRAY / DO_ARRAY_ELEM code below,
        # where it's the type of each array element. But it's also passed to
        # the typemap template (although undocumented and virtually unused).
        (my $subtype = $ntype) =~ s/(?:Array)?(?:Ptr)?$//;
  
        # look up the TYPEMAP entry for this C type and grab the corresponding
        # XS type name (e.g. $type of 'char *'  gives $xstype of 'T_PV'
        my $typemap = $typemaps->get_typemap(ctype => $type);
        if (not $typemap) {
            $pxs->report_typemap_failure($typemaps, $type);
            return;
        }
        my $xstype = $typemap->xstype;
  
        # An optimisation: for the typemaps which check that the dereferenced
        # item is blessed into the right class, skip the test for DESTROY()
        # methods, as more or less by definition, DESTROY() will be called
        # on an object of the right class. Basically, for T_foo_OBJ, use
        # T_foo_REF instead. T_REF_IV_PTR was added in v5.22.0.
        $xstype =~ s/OBJ$/REF/ || $xstype =~ s/^T_REF_IV_PTR$/T_PTRREF/
            if $pxs->{xsub_func_name} =~ /DESTROY$/;
  
        # For a string-ish parameter foo, if length(foo) was also declared
        # as a pseudo-parameter, then override the normal typedef - which
        # would emit SvPV_nolen(...) - and instead, emit SvPV(...,
        # STRLEN_length_of_foo)
        if (    $xstype eq 'T_PV'
                and exists $pxs->{xsub_sig}{names}{"length($var)"})
        {
            print " = ($type)SvPV($arg, STRLEN_length_of_$var);\n";
            die "default value not supported with length(NAME) supplied"
                if defined $default;
            return;
        }
  
        # Get the ExtUtils::Typemaps::InputMap object associated with the
        # xstype. This contains the template of the code to be embedded,
        # e.g. 'SvPV_nolen($arg)'
        my $inputmap = $typemaps->get_inputmap(xstype => $xstype);
        if (not defined $inputmap) {
            $pxs->blurt("Error: No INPUT definition for type '$type', typekind '$xstype' found");
            return;
        }
  
        # Get the text of the template, with a few transformations to make it
        # work better with fussy C compilers. In particular, strip trailing
        # semicolons and remove any leading white space before a '#'.
        my $expr = $inputmap->cleaned_code;
  
        my $argoff = $arg_num - 1;
  
        # Process DO_ARRAY_ELEM. This is an undocumented hack that makes the
        # horrible T_ARRAY typemap work. "DO_ARRAY_ELEM" appears as a token
        # in the INPUT and OUTPUT code for for T_ARRAY, within a "for each
        # element" loop, and the purpose of this branch is to substitute the
        # token for some real code which will process each element, based
        # on the type of the array elements (the $subtype).
        #
        # Note: This gruesome bit either needs heavy rethinking or
        # documentation. I vote for the former. --Steffen, 2011
        # Seconded, DAPM 2024.
        if ($expr =~ /\bDO_ARRAY_ELEM\b/) {
            my $subtypemap  = $typemaps->get_typemap(ctype => $subtype);
            if (not $subtypemap) {
                $pxs->report_typemap_failure($typemaps, $subtype);
                return;
            }
  
            my $subinputmap =
                $typemaps->get_inputmap(xstype => $subtypemap->xstype);
            if (not $subinputmap) {
                $pxs->blurt("Error: No INPUT definition for type '$subtype',
                            typekind '" . $subtypemap->xstype . "' found");
                return;
            }
  
            my $subexpr = $subinputmap->cleaned_code;
            $subexpr =~ s/\$type/\$subtype/g;
            $subexpr =~ s/ntype/subtype/g;
            $subexpr =~ s/\$arg/ST(ix_$var)/g;
            $subexpr =~ s/\n\t/\n\t\t/g;
            $subexpr =~ s/is not of (.*\")/[arg %d] is not of $1, ix_$var + 1/g;
            $subexpr =~ s/\$var/${var}\[ix_$var - $argoff]/;
            $expr =~ s/\bDO_ARRAY_ELEM\b/$subexpr/;
        }
  
        if ($expr =~ m#/\*.*scope.*\*/#i) {  # "scope" in C comments
            $pxs->{xsub_SCOPE_enabled} = 1;
        }
  
        # Specify additional environment for when a template derived from a
        # *typemap* is evalled.
        @$eval_vars{qw(ntype subtype argoff)} = ($ntype, $subtype, $argoff);
        $init_template = $expr;
    }
  
    # Now finally, emit the actual variable declaration and initialisation
    # line(s). The variable type and name will already have been emitted.
  
    my $init_code =
        length $init_template
            ? $pxs->eval_input_typemap_code("qq\a$init_template\a", $eval_vars)
            : "";
  
  
    if (defined $default
        # XXX for now, for backcompat, ignore default if the
        # param has a typemap override
        && !(defined $init)
        # XXX for now, for backcompat, ignore default if the
        # param wouldn't otherwise get initialised
        && !$no_init
    ) {
        # Has a default value. Just terminate the variable declaration, and
        # defer the initialisation.
  
        print ";\n";
  
        # indent the code 1 step further
        $init_code =~ s/(\t+)/$1    /g;
        $init_code =~ s/        /\t/g;
  
        if ($default eq 'NO_INIT') {
            # for foo(a, b = NO_INIT), add code to initialise later only if
            # an arg was supplied.
            $pxs->{xsub_deferred_code_lines}
                .= sprintf "\n\tif (items >= %d) {\n%s;\n\t}\n",
                           $arg_num, $init_code;
        }
        else {
            # for foo(a, b = default), add code to initialise later to either
            # the arg or default value
            my $else = ($init_code =~ /\S/) ? "\telse {\n$init_code;\n\t}\n" : "";
  
            $default =~ s/"/\\"/g; # escape double quotes
            $pxs->{xsub_deferred_code_lines}
                .= sprintf "\n\tif (items < %d)\n\t    %s = %s;\n%s",
                        $arg_num,
                        $var,
                        $pxs->eval_input_typemap_code("qq\a$default\a",
                                                       $eval_vars),
                        $else;
        }
    }
    elsif ($pxs->{xsub_SCOPE_enabled} or $init_code !~ /^\s*\Q$var\E =/) {
        # The template is likely a full block rather than a '$var = ...'
        # expression. Just terminate the variable declaration, and defer the
        # initialisation.
        # Note that /\Q$var\E/ matches the string containing whatever $var
        # was expanded to in the eval.
  
        print ";\n";
  
        $pxs->{xsub_deferred_code_lines} .= sprintf "\n%s;\n", $init_code
            if $init_code =~ /\S/;
    }
    else {
        # The template starts with '$var = ...'. The variable name has already
        # been emitted, so remove it from the typemap before evalling it,
  
        $init_code =~ s/^\s*\Q$var\E(\s*=\s*)/$1/
            or $pxs->death("panic: typemap doesn't start with '\$var='\n");
  
        printf "%s;\n", $init_code;
    }
  
    if (defined $defer) {
        $pxs->{xsub_deferred_code_lines}
            .= $pxs->eval_input_typemap_code("qq\a$defer\a", $eval_vars) . "\n";
    }
}



# $param->as_output_code($ParseXS_object, $out_num])
#
# Emit code to: possibly create, then set the value of, and possibly
# push, an output SV, based on the values in the $param object.
#
# $out_num is optional and its presence indicates that an OUTLIST var is
# being pushed: it indicates the position on the stack of that SV.
#
# This function emits code such as "sv_setiv(ST(0), (IV)foo)", based on
# the typemap OUTPUT entry associated with $type. It passes the typemap
# code through a double-quotish context eval first to expand variables
# such as $arg and $var. It also tries to optimise the emitted code in
# various ways, such as using TARG where available rather than calling
# sv_newmortal() to obtain an SV to set to the return value.
#
# It expects to handle three categories of variable, with these general
# actions:
#
#   RETVAL, i.e. the return value
#
#     Create a new SV; use the typemap to set its value to RETVAL; then
#     store it at ST(0).
#
#   OUTLIST foo
#
#     Create a new SV; use the typemap to set its value to foo; then store
#     it at ST($out_num-1).
#
#   OUTPUT: foo / OUT foo
#
#     Update the value of the passed arg ST($num-1), using the typemap to
#     set its value
#
# Note that it's possible for this function to be called *twice* for the
# same variable: once for OUTLIST, and once for an 'OUTPUT:' entry.
#
# It treats output typemaps as falling into two basic categories,
# exemplified by:
#
#     sv_setFoo($arg, (Foo)$var));
#
#     $arg = newFoo($var);
#
# The first form is the most general and can be used to set the SV value
# for all of the three variable categories above. For the first two
# categories it typically uses a new mortal, while for the last, it just
# uses the passed arg SV.
#
# The assign form of the typemap can be considered an optimisation of
# sv_setsv($arg, newFoo($var)), and is applicable when newFOO() is known
# to return a new SV. So rather than copying it to yet another new SV,
# just return as-is, possibly after mortalising it,
#
# Some typemaps evaluate to different code depending on whether the var is
# RETVAL, e.g T_BOOL is currently defined as:
#
#    ${"$var" eq "RETVAL" ? \"$arg = boolSV($var);"
#                         : \"sv_setsv($arg, boolSV($var));"}
#
# So we examine the typemap *after* evaluation to determine whether it's
# of the form '$arg = ' or not.
#
# Note that *currently* we generally end up with the pessimised option for
# OUTLIST vars, since the typmaps onlt check for RETVAL.
#
# Currently RETVAL and 'OUTLIST var' mostly share the same code paths
# below, so they both benefit from optimisations such as using TARG
# instead of creating a new mortal, and using the RETVALSV C var to keep
# track of the temp SV, rather than repeatedly retrieving it from ST(0)
# etc. Note that RETVALSV is private and shouldn't be referenced within XS
# code or typemaps.

sub as_output_code {
  my ExtUtils::ParseXS::Node::Param $self = shift;
  my ExtUtils::ParseXS              $pxs  = shift;
  my $out_num = shift;

  my ($type, $num, $var, $do_setmagic, $output_code)
    = @{$self}{qw(type arg_num var do_setmagic output_code)};

  if ($var eq 'RETVAL') {
    # Do some preliminary RETVAL-specific checks and settings.

    # Only OUT/OUTPUT vars (which update one of the passed args) should be
    # calling set magic; RETVAL and OUTLIST should be setting the value of
    # a fresh mortal or TARG. Note that a param can be both OUTPUT and
    # OUTLIST - the value of $do_setmagic only applies to its use as an
    # OUTPUT (updating) value.

    $pxs->death("Internal error: do set magic requested on RETVAL")
      if $do_setmagic;

    # RETVAL normally has an undefined arg_num, although it can be
    # set to a real index if RETVAL is also declared as a parameter.
    # But when returning its value, it's always stored at ST(0).
    $num = 1;

    # It is possible for RETVAL to have multiple types, e.g.
    #     int
    #     foo(long RETVAL)
    #
    # In the above, 'long' is used for the RETVAL C var's declaration,
    # while 'int' is used to generate the return code (for backwards
    # compatibility).
    $type = $pxs->{xsub_return_type};
  }


  # ------------------------------------------------------------------
  # Do initial processing of $type, including creating various derived
  # values

  unless (defined $type) {
    $pxs->blurt("Can't determine output type for '$var'");
    return;
  }

  # $ntype: normalised type ('Foo *' becomes 'FooPtr' etc): one of the
  # valid vars which can appear within a typemap template.
  (my $ntype = $type) =~ s/\s*\*/Ptr/g;
  $ntype =~ s/\(\)//g;

  # $subtype is really just for the T_ARRAY / DO_ARRAY_ELEM code below,
  # where it's the type of each array element. But it's also passed to
  # the typemap template (although undocumented and virtually unused).
  # Basically for a type like FooArray or FooArrayPtr, the subtype is Foo.
  (my $subtype = $ntype) =~ s/(?:Array)?(?:Ptr)?$//;

  # whitespace-tidy the type
  $type = ExtUtils::Typemaps::tidy_type($type);

  # The type as supplied to the eval is Foo__Bar rather than Foo::Bar
  my $eval_type = $type;
  $eval_type =~ tr/:/_/ unless $pxs->{config_RetainCplusplusHierarchicalTypes};

  # We can be called twice for the same variable: once to update the
  # original arg (via an entry in OUTPUT) and once to push the param's
  # value (via OUTLIST). When doing the latter, any override code on an
  # OUTPUT line should not be used.
  undef $output_code if defined $out_num;


  # ------------------------------------------------------------------
  # Find the template code (pre any eval) and store it in $expr.
  # This is typically obtained via a typemap lookup, but can be
  # overridden. Also set vars ready for evalling the typemap template.

  my $expr;
  my $outputmap;
  my $typemaps = $pxs->{typemaps_object};

  if (defined $output_code) {
    # An override on an OUTPUT line: use that instead of the typemap.
    # Note that we don't set $expr here, because $expr holds a template
    # string pre-eval, while OUTPUT override code is *not*
    # template-expanded, so $output_code is effectively post-eval code.
  }
  elsif ($type =~ /^array\(([^,]*),(.*)\)/) {
    # Specially handle the implicit array return type, "array(type, nlelem)"
    # rather than using a typemap entry. It returns a string SV whose
    # buffer is a copy of $var, which it assumes is a C array of
    # type 'type' with 'nelem' elements.

    my ($atype, $nitems) = ($1, $2);

    if ($var ne 'RETVAL') {
      # This special type is intended for use only as the return type of
      # an XSUB
      $pxs->blurt("Can't use array(type,nitems) type for "
                    . (defined $out_num ? "OUTLIST" : "OUT")
                    . " parameter");
      return;
    }

    $expr = "\tsv_setpvn(\$arg, (char *)\$var, $nitems * sizeof($atype));\n";
  }
  else {
    # Handle a normal return type via a typemap.

    # Get the output map entry for this type; complain if not found.
    my $typemap = $typemaps->get_typemap(ctype => $type);
    if (not $typemap) {
      $pxs->report_typemap_failure($typemaps, $type);
      return;
    }

    $outputmap = $typemaps->get_outputmap(xstype => $typemap->xstype);
    if (not $outputmap) {
      $pxs->blurt("Error: No OUTPUT definition for type '$type', typekind '"
                   . $typemap->xstype . "' found");
      return;
    }

    # Get the text of the typemap template, with a few transformations to
    # make it work better with fussy C compilers. In particular, strip
    # trailing semicolons and remove any leading white space before a '#'.

    $expr = $outputmap->cleaned_code;
  }

  my $arg = $pxs->ST(defined $out_num ? $out_num + 1 : $num);

  # Specify the environment for if/when the code template is evalled.
  my $eval_vars = {
                    num         => $num,
                    var         => $var,
                    do_setmagic => $do_setmagic,
                    subtype     => $subtype,
                    ntype       => $ntype,
                    arg         => $arg,
                    type        => $eval_type,
                  };


  # ------------------------------------------------------------------
  # Handle DO_ARRAY_ELEM token as a very special case

  if (!defined $output_code and $expr =~ /\bDO_ARRAY_ELEM\b/) {
    # See the comments in ExtUtils::ParseXS::Node::Param::as_code() that
    # explain the similar code for the DO_ARRAY_ELEM hack there.

    if ($var ne 'RETVAL') {
      # Typemap templates containing DO_ARRAY_ELEM are assumed to contain
      # a loop which explicitly stores a new mortal SV at each of the
      # locations ST(0) .. ST(n-1), and which then uses the code from the
      # typemap for the underlying array element to set each SV's value.
      #
      # This is a horrible hack for RETVAL, which would probably fail with
      # OUTLIST due to stack offsets being wrong, and definitely would
      # fail with OUT, which is supposed to be updating parameter SVs, not
      # pushing anything on the stack. So forbid all except RETVAL.
      $pxs->blurt("Can't use typemap containing DO_ARRAY_ELEM for "
                    . (defined $out_num ? "OUTLIST" : "OUT")
                    . " parameter");
      return;
    }

    my $subtypemap = $typemaps->get_typemap(ctype => $subtype);
    if (not $subtypemap) {
      $pxs->report_typemap_failure($typemaps, $subtype);
      return;
    }

    my $suboutputmap = $typemaps->get_outputmap(xstype => $subtypemap->xstype);
    if (not $suboutputmap) {
      $pxs->blurt("Error: No OUTPUT definition for type '$subtype', typekind '" . $subtypemap->xstype . "' found");
      return;
    }

    my $subexpr = $suboutputmap->cleaned_code;
    $subexpr =~ s/ntype/subtype/g;
    $subexpr =~ s/\$arg/ST(ix_$var)/g;
    $subexpr =~ s/\$var/${var}\[ix_$var]/g;
    $subexpr =~ s/\n\t/\n\t\t/g;
    $expr =~ s/\bDO_ARRAY_ELEM\b/$subexpr/;

    # We do our own code emitting and return here (rather than control
    # passing on to normal RETVAL processing) since that processing is
    # expecting to push a single temp onto the stack, while our code
    # pushes several temps.
    print $pxs->eval_output_typemap_code("qq\a$expr\a", $eval_vars);
    return;
  }


  # ------------------------------------------------------------------
  # Now emit code for the three types of return value:
  #
  #   RETVAL           - The usual case: store an SV at ST(0) which is set
  #                      to the value of RETVAL. This is typically a new
  #                      mortal, but may be optimised to use TARG.
  #
  #   OUTLIST param    - if $out_num is defined (and will be >= 0) Push
  #                      after any RETVAL, new mortal(s) containing the
  #                      current values of the local var set from that
  #                      parameter. (May also use TARG if not already used
  #                      by RETVAL).
  #
  #   OUT/OUTPUT param - update passed arg SV at ST($num-1) (which
  #                      corresponds to param) with the current value of
  #                      the local var set from that parameter.

  if ($var ne 'RETVAL' and not defined $out_num) {
    # This is a normal OUTPUT var: i.e. a named parameter whose
    # corresponding arg on the stack should be updated with the
    # parameter's current value by using the code contained in the
    # output typemap.
    #
    # Note that for args being *updated* (as opposed to replaced), this
    # branch relies on the typemap to Do The Right Thing. For example,
    # T_BOOL currently has this typemap entry:
    #
    # ${"$var" eq "RETVAL" ? \"$arg = boolSV($var);"
    #                      : \"sv_setsv($arg, boolSV($var));"}
    #
    # which means that if we hit this branch, $evalexpr will have been
    # expanded to something like "sv_setsv(ST(2), boolSV(foo))".

    unless (defined $num) {
      $pxs->blurt("Internal error: OUT parameter has undefined argument number");
      return;
    }

    # Use the code on the OUTPUT line if specified, otherwise use the
    # typemap
    my $code = defined $output_code
        ? "\t$output_code\n"
        : $pxs->eval_output_typemap_code("qq\a$expr\a", $eval_vars);
    print $code;

    # For parameters in the OUTPUT section, honour the SETMAGIC in force
    # at the time. For parameters instead being output because of an OUT
    # keyword in the signature, assume set magic always.
    print "\tSvSETMAGIC($arg);\n" if !$self->{in_output} || $do_setmagic;
    return;
  }


  # ------------------------------------------------------------------
  # The rest of this main body handles RETVAL or "OUTLIST foo".

  if (defined $output_code and !defined $out_num) {
    # Handle this (just emit overridden code as-is):
    #    OUTPUT:
    #       RETVAL output_code
    print "\t$output_code\n";
    print "\t++SP;\n" if $pxs->{xsub_stack_was_reset};
    return;
  }

  # Emit a standard RETVAL/OUTLIST return


  # ------------------------------------------------------------------
  # First, evaluate the typemap, expanding any vars like $var and $arg,
  # for example,
  #
  #     $arg = newFoo($var);
  # or
  #     sv_setFoo($arg, $var);
  #
  # However, rather than using the actual destination (such as ST(0))
  # for the value of $arg, we instead set it initially to RETVALSV. This
  # is because often the SV will be used in more than one statement,
  # and so it is more efficient to temporarily store it in a C auto var.
  # So we normally emit code such as:
  #
  #  {
  #     SV *RETVALSV;
  #     RETVALSV = newFoo(RETVAL);
  #     RETVALSV = sv_2mortal(RETVALSV);
  #     ST(0) = RETVALSV;
  #  }
  #
  # Rather than
  #
  #     ST(0) = newFoo(RETVAL);
  #     sv_2mortal(ST(0));
  #
  # Later we sometimes modify the evalled typemap to change 'RETVALSV'
  # to some other value:
  #   - back to e.g. 'ST(0)' if there is no other use of the SV;
  #   - to TARG when we are using the OP_ENTERSUB's targ;
  #   - to $var when then return type is SV* (and thus ntype is SVPtr)
  #     and so the variable will already have been declared as type 'SV*'
  #     and thus there is no need for a RETVALSV too.
  #
  # Note that we evaluate the typemap early here so that the various
  # regexes below such as /^\s*\Q$arg\E\s*=/ can be matched against
  # the *evalled* result of typemap entries such as
  #
  # ${ "$var" eq "RETVAL" ? \"$arg = $var;" : \"sv_setsv_mg($arg, $var);" }
  #
  # which may eval to something like "RETVALSV = RETVAL" and
  # subsequently match /^\s*\Q$arg\E =/ (where $arg is "RETVAL"), but
  # couldn't have matched against the original typemap.
  # This is why we *always* set $arg to 'RETVALSV' first and then modify
  # the typemap later - we don't know what final value we want for $arg
  # until after we've examined the evalled result.

  my $orig_arg = $arg;
  $eval_vars->{arg} = $arg = 'RETVALSV';
  my $evalexpr = $pxs->eval_output_typemap_code("qq\a$expr\a", $eval_vars);


  # ------------------------------------------------------------------
  # Examine the just-evalled typemap code to determine what optimisations
  # etc can be performed and what sort of code needs emitting. The two
  # halves of this following if/else examine the two forms of evalled
  # typemap:
  #
  #     RETVALSV = newFoo((Foo)RETVAL);
  # and
  #     sv_setFoo(RETVALSV, (Foo)RETVAL);
  #
  # In particular, the first form is assumed to be returning an SV which
  # the function has generated itself (e.g. newSVREF()) and which may
  # just need mortalising; while the second form generally needs a call
  # to sv_newmortal() first to create an SV which the function can then
  # set the value of.

  my $do_mortalize   = 0;  # Emit an sv_2mortal()
  my $want_newmortal = 0;  # Emit an sv_newmortal()
  my $retvar = 'RETVALSV'; # The name of the C var which holds the SV
                           # (likely tmp) to set to the value of the var

  if ($evalexpr =~ /^\s*\Q$arg\E\s*=/) {
    # Handle this form: RETVALSV = newFoo((Foo)RETVAL);
    # newFoo creates its own SV: we just need to mortalise and return it

    # Is the SV one of the immortal SVs?
    if ($evalexpr =~
        /^\s*
          \Q$arg\E
          \s*=\s*
          (  boolSV\(.*\)
          |  &PL_sv_yes
          |  &PL_sv_no
          |  &PL_sv_undef
          |  &PL_sv_zero
          )
          \s*;\s*$
        /x)
    {
      # If so, we can skip mortalising it to stop it leaking.
      $retvar = $orig_arg; # just assign to ST(N) directly
    }
    else {
      # general '$arg = newFOO()' typemap
      $do_mortalize = 1;

      # If $var is already of type SV*, then use that instead of
      # declaring 'SV* RETVALSV' as an intermediate var.
      $retvar = $var if $ntype eq "SVPtr";
    }
  }
  else {
    # Handle this (eval-expanded) form of typemap:
    #     sv_setFoo(RETVALSV, (Foo)var);
    # We generally need to supply a mortal SV for the typemap code to
    # set, and then return it on the stack,

    # First, see if we can use the targ (if any) attached to the current
    # OP_ENTERSUB, to avoid having to create a new mortal.
    #
    # The targetable() OutputMap class method looks at whether the code
    # snippet is of a form suitable for using TARG as the destination.
    # It looks for one of a known list of well-behaved setting function
    # calls, like sv_setiv() which will set the TARG to a value that
    # doesn't include magic, tieing, being a reference (which would leak
    # as the TARG is never freed), etc. If so, emit dXSTARG and replace
    # RETVALSV with TARG.
    #
    # For backwards-compatibility, dXSTARG may have already been emitted
    # early in the XSUB body, when a more restrictive set of targ-
    # compatible typemap entries were checked for. Note that dXSTARG is
    # defined as something like:
    #
    #   SV * targ = (PL_op->op_private & OPpENTERSUB_HASTARG)
    #               ? PAD_SV(PL_op->op_targ) : sv_newmortal()

    if (   $pxs->{config_optimize}
        && ExtUtils::Typemaps::OutputMap->targetable($evalexpr)
        && !$pxs->{xsub_targ_used})
    {
      # So TARG is available for use.
      $retvar = 'TARG';
      $pxs->{xsub_targ_used} = 1;  # can only use TARG to return one value

      # Since we're using TARG for the return SV, see if we can use the
      # TARG[iun] macros as appropriate to speed up setting it.
      # If so, convert "sv_setiv(RETVALSV, val)" to "TARGi(val,1)" and
      # similarly for uv and nv. These macros skip a function call for the
      # common case where TARG is already a simple IV/UV/NV. Convert the
      # _mg forms too: since we're setting the TARG, there shouldn't be
      # set magic on it, so the _mg action can be safely ignored.

      $evalexpr =~ s{
                    ^
                    (\s*)
                    sv_set([iun])v(?:_mg)?
                    \(
                      \s* RETVALSV \s* ,
                      \s* (.*)
                    \)
                    ( \s* ; \s*)
                    $
                    }
                    {$1TARG$2($3, 1)$4}x;
    }
    else {
      # general typemap: give it a fresh SV to set the value of.
      $want_newmortal = 1;
    }
  }


  # ------------------------------------------------------------------
  # Now emit the return C code, based on the various flags and values
  # determined above.

  my $do_scope; # wrap code in a {} block
  my @lines;    # Lines of code to eventually emit

  # Do any declarations first

  if ($retvar eq 'TARG' && !$pxs->{xsub_targ_declared_early}) {
    push @lines, "\tdXSTARG;\n";
    $do_scope = 1;
  }
  elsif ($retvar eq 'RETVALSV') {
    push @lines, "\tSV * $retvar;\n";
    $do_scope = 1;
  }

  push @lines, "\tRETVALSV = sv_newmortal();\n" if $want_newmortal;

  # Emit the typemap, while changing the name of the destination SV back
  # from RETVALSV to one of the other forms (varname/TARG/ST(N)) if was
  # determined earlier to be necessary.
  # Skip emitting it if it's of the trivial form "var = var", which is
  # generated when the typemap is of the form '$arg = $var' and the SVPtr
  # optimisation is using $var for the destination.

  $evalexpr =~ s/\bRETVALSV\b/$retvar/g if $retvar ne 'RETVALSV';

  unless ($evalexpr =~ /^\s*\Q$var\E\s*=\s*\Q$var\E\s*;\s*$/) {
    push @lines, split /^/, $evalexpr
  }

  # Emit mortalisation on the result SV if needed
  push @lines, "\t$retvar = sv_2mortal($retvar);\n" if $do_mortalize;

  # Emit the final 'ST(n) = RETVALSV' or similar, unless ST(n)
  # was already assigned to earlier directly by the typemap.
  push @lines, "\t$orig_arg = $retvar;\n" unless $retvar eq $orig_arg;

  if ($do_scope) {
    # Add an extra 4-indent, then wrap the output code in a new block
    for (@lines) {
      s/\t/        /g;   # break down all tabs into spaces
      s/^/    /;         # add 4-space extra indent
      s/        /\t/g;   # convert 8 spaces back to tabs
    }
    unshift @lines,  "\t{\n";
    push    @lines,  "\t}\n";
  }

  print @lines;
  print "\t++SP;\n" if $pxs->{xsub_stack_was_reset};
}

# ======================================================================

package ExtUtils::ParseXS::Node::Sig;

# Node subclass which holds the state of an XSUB's signature, based on the
# XSUB's actual signature plus any INPUT lines. It is a mainly a list of
# Node::Param children.

BEGIN {
    our @ISA = qw(ExtUtils::ParseXS::Node);

    our @FIELDS = (
        @ExtUtils::ParseXS::Node::FIELDS,
        'orig_params',   # Array ref of Node::Param objects representing
                         # the original (as parsed) parameters of this XSUB

        'params',        # Array ref of Node::Param objects representing
                         # the current parameters of this XSUB - this
                         # is orig_params plus any updated fields from
                         # processing INPUT and OUTPUT lines. Note that
                         # with multiple CASE: blocks, there can be
                         # multiple sets of INPUT and OUTPUT etc blocks.
                         # params is reset to the contents of orig_params
                         # after the start of each new CASE: block.

        'names',         # Hash ref mapping variable names to Node::Param
                         # objects

        'sig_text',      # The original text of the sig, e.g.
                         #   'param1, int param2 = 0'

        'seen_ellipsis', # Bool: XSUB signature has (   ,...)

        'nargs',         # The number of args expected from caller
        'min_args',      # The minimum number of args allowed from caller

        'auto_function_sig_override', # the C_ARGS value, if any

    );

    fields->import(@FIELDS) if $USING_FIELDS;
}


# ----------------------------------------------------------------
# Parse the XSUB's signature: $sig->{sig_text}
#
# Split the signature on commas into parameters, while allowing for
# things like '(a = ",", b)'. Then for each parameter, parse its
# various fields and store in a ExtUtils::ParseXS::Node::Param object.
# Store those Param objects within the Sig object, plus any other state
# deduced from the signature, such as min/max permitted number of args.
#
# A typical signature might look like:
#
#    OUT     char *s,             \
#            int   length(s),     \
#    OUTLIST int   size     = 10)
#
# ----------------------------------------------------------------

my ($C_group_rex, $C_arg);

# Group in C (no support for comments or literals)
#
# DAPM 2024: I'm not entirely clear what this is supposed to match.
# It appears to match balanced and possibly nested [], {} etc, with
# similar but possibly unbalanced punctuation within. But the balancing
# brackets don't have to correspond: so [} is just as valid as [] or {},
# as is [{{{{] or even [}}}}}

$C_group_rex = qr/ [({\[]
             (?: (?> [^()\[\]{}]+ ) | (??{ $C_group_rex }) )*
             [)}\]] /x;

# $C_arg: match a chunk in C without comma at toplevel (no comments),
# i.e. a single arg within an XS signature, such as
#   foo = ','
#
# DAPM 2024. This appears to match zero, one or more of:
#   a random collection of non-bracket/quote/comma chars (e.g, a word or
#        number or 'int *foo' etc), or
#   a balanced(ish) nested brackets, or
#   a "string literal", or
#   a 'c' char literal
# So (I guess), it captures the next item in a function signature

$C_arg = qr/ (?: (?> [^()\[\]{},"']+ )
       |   (??{ $C_group_rex })
       |   " (?: (?> [^\\"]+ )
         |   \\.
         )* "        # String literal
              |   ' (?: (?> [^\\']+ ) | \\. )* ' # Char literal
       )* /xs;


sub parse {
    my ExtUtils::ParseXS::Node::Sig $self = shift;
    my ExtUtils::ParseXS            $pxs  = shift;

    # remove line continuation chars (\)
    $self->{sig_text} =~ s/\\\s*/ /g;
    my $sig_text = $self->{sig_text};

    my @param_texts;
    my $opt_args = 0; # how many params with default values seen
    my $nargs    = 0; # how many args are expected

    # First, split signature into separate parameters

    if ($sig_text =~ /\S/) {
        my $sig_c = "$sig_text ,";
        use re 'eval'; # needed for 5.16.0 and earlier
        my $can_use_regex = ($sig_c =~ /^( (??{ $C_arg }) , )* $ /x);
        no re 'eval';

        if ($can_use_regex) {
            # If the parameters are capable of being split by using the
            # fancy regex, do so. This splits the params on commas, but
            # can handle things like foo(a = ",", b)
            use re 'eval';
            @param_texts = ($sig_c =~ /\G ( (??{ $C_arg }) ) , /xg);
        }
        else {
            # This is the fallback parameter-splitting path for when the
            # $C_arg regex doesn't work. This code path should ideally
            # never be reached, and indicates a design weakness in $C_arg.
            @param_texts = split(/\s*,\s*/, $sig_text);
            Warn($pxs, "Warning: cannot parse parameter list '$sig_text', fallback to split");
        }
    }
    else {
        @param_texts = ();
    }

    # C++ methods get a fake object/class param at the start.
    # This affects arg numbering.
    if (defined($pxs->{xsub_class})) {
        my ($var, $type) =
            ($pxs->{xsub_seen_static} or $pxs->{xsub_func_name} eq 'new')
                ? ('CLASS', "char *")
                : ('THIS',  "$pxs->{xsub_class} *");

        my ExtUtils::ParseXS::Node::Param $param
                = ExtUtils::ParseXS::Node::Param->new( {
                        var          => $var,
                        type         => $type,
                        is_synthetic => 1,
                        arg_num      => ++$nargs,
                    });
        push @{$self->{params}}, $param;
        $self->{names}{$var} = $param;
        $param->check($pxs)
    }

    # For non-void return types, add a fake RETVAL parameter. This triggers
    # the emitting of an 'int RETVAL;' declaration or similar, and (e.g. if
    # later flagged as in_output), triggers the emitting of code to return
    # RETVAL's value.
    #
    # Note that a RETVAL param can be in three main states:
    #
    #  fully-synthetic  What is being created here. RETVAL hasn't appeared
    #                   in a signature or INPUT.
    #
    #  semi-real        Same as fully-synthetic, but with a defined
    #                   arg_num, and with an updated position within
    #                   @{$self->{params}}.
    #                   A RETVAL has appeared in the signature, but
    #                   without a type yet specified, so it continues to
    #                   use {xsub_return_type}.
    #
    #  real             is_synthetic, no_init flags turned off. Its
    #                   type comes from the sig or INPUT line. This is
    #                   just a normal parameter now.

    if ($pxs->{xsub_return_type} ne 'void') {
        my ExtUtils::ParseXS::Node::Param $param =
            ExtUtils::ParseXS::Node::Param->new( {
                var          => 'RETVAL',
                type         => $pxs->{xsub_return_type},
                no_init      => 1, # just declare the var, don't initialise it
                is_synthetic => 1,
            } );

        push @{$self->{params}}, $param;
        $self->{names}{RETVAL} = $param;
        $param->check($pxs)
    }

    for (@param_texts) {
        # Process each parameter. A parameter is of the general form:
        #
        #    OUT char* foo = expression
        #
        #  where:
        #    IN/OUT/OUTLIST etc are only allowed under
        #                      $pxs->{config_allow_inout}
        #
        #    a C type       is only allowed under
        #                      $pxs->{config_allow_argtypes}
        #
        #    foo            can be a plain C variable name, or can be
        #    length(foo)    but only under $pxs->{config_allow_argtypes}
        #
        #    = default      default value - only allowed under
        #                      $pxs->{config_allow_argtypes}

        s/^\s+//;
        s/\s+$//;

        # Process ellipsis (...)

        $pxs->blurt("further XSUB parameter seen after ellipsis (...)")
            if $self->{seen_ellipsis};

        if ($_ eq '...') {
            $self->{seen_ellipsis} = 1;
            next;
        }

        # Decompose parameter into its components.
        # Note that $name can be either 'foo' or 'length(foo)'

        my ($out_type, $type, $name, $sp1, $sp2, $default) =
                /^
                     (?:
                         (IN|IN_OUT|IN_OUTLIST|OUT|OUTLIST)
                         \b\s*
                     )?
                     (.*?)                             # optional type
                     \s*
                     \b
                     (   \w+                           # var
                         | length\( \s*\w+\s* \)       # length(var)
                     )
                     (?:
                            (\s*) = (\s*) ( .*?)       # default expr
                     )?
                     \s*
                 $
                /x;

        unless (defined $name) {
            if (/^ SV \s* \* $/x) {
                # special-case SV* as a placeholder for backwards
                # compatibility.
                push @{$self->{params}},
                    ExtUtils::ParseXS::Node::Param->new( {
                        var     => 'SV *',
                        arg_num => ++$nargs,
                    });
            }
            else {
                $pxs->blurt("Unparseable XSUB parameter: '$_'");
            }
            next;
        }

        undef $type unless length($type) && $type =~ /\S/;

        my ExtUtils::ParseXS::Node::Param $param
                = ExtUtils::ParseXS::Node::Param->new( {
                        var => $name,
                    });

        # Check for duplicates

        my $old_param = $self->{names}{$name};
        if ($old_param) {
            if (    $name eq 'RETVAL'
                    and $old_param->{is_synthetic}
                    and !defined $old_param->{arg_num})
            {
                # RETVAL is currently fully synthetic. Now that it has
                # been declared as a parameter too, override any implicit
                # RETVAL declaration. Delete the original param from the
                # param list.
                @{$self->{params}} = grep $_ != $old_param, @{$self->{params}};
                # If the param declaration includes a type, it becomes a
                # real parameter. Otherwise the param is kept as
                # 'semi-real' (synthetic, but with an arg_num) until such
                # time as it gets a type set in INPUT, which would remove
                # the synthetic/no_init.
                $param = $old_param if !defined $type;
            }
            else {
                $pxs->blurt(
                        "Error: duplicate definition of parameter '$name' ignored");
                next;
            }
        }

        push @{$self->{params}}, $param;
        $self->{names}{$name} = $param;

        # Process optional IN/OUT etc modifier

        if (defined $out_type) {
            if ($pxs->{config_allow_inout}) {
                $out_type =  $out_type eq 'IN' ? '' : $out_type;
            }
            else {
                $pxs->blurt("parameter IN/OUT modifier not allowed under -noinout");
            }
        }
        else {
            $out_type = '';
        }

        # Process optional type

        if (defined($type) && !$pxs->{config_allow_argtypes}) {
            $pxs->blurt("parameter type not allowed under -noargtypes");
            undef $type;
        }

        # Process 'length(foo)' pseudo-parameter

        my $is_length;
        my $len_name;

        if ($name =~ /^length\( \s* (\w+) \s* \)\z/x) {
            if ($pxs->{config_allow_argtypes}) {
                $len_name = $1;
                $is_length = 1;
                if (defined $default) {
                    $pxs->blurt("Default value not allowed on length() parameter '$len_name'");
                    undef $default;
                }
            }
            else {
                $pxs->blurt("length() pseudo-parameter not allowed under -noargtypes");
            }
        }

        # Handle ANSI params: those which have a type or 'length(s)',
        # and which thus don't need a matching INPUT line.

        if (defined $type or $is_length) { # 'int foo' or 'length(foo)'
            @$param{qw(type is_ansi)} = ($type, 1);

            if ($is_length) {
                $param->{no_init}   = 1;
                $param->{is_length} = 1;
                $param->{len_name}  = $len_name;
            }
        }

        $param->{in_out} = $out_type if length $out_type;
        $param->{no_init} = 1        if $out_type =~ /^OUT/;

        # Process the default expression, including making the text
        # to be used in "usage: ..." error messages.
        my $report_def = '';
        if (defined $default) {
            $opt_args++;
            # The default expression for reporting usage. For backcompat,
            # sometimes preserve the spaces either side of the '='
            $report_def =    ((defined $type or $is_length) ? '' : $sp1)
                           . "=$sp2$default";
            $param->{default_usage} = $report_def;
            $param->{default} = $default;
        }

        if ($out_type eq "OUTLIST" or $is_length) {
            $param->{arg_num} = undef;
        }
        else {
            $param->{arg_num} = ++$nargs;
        }
    } # for (@param_texts)

    $self->{nargs}    = $nargs;
    $self->{min_args} = $nargs - $opt_args;
}


# Return a string to be used in "usage: .." error messages.

sub usage_string {
    my ExtUtils::ParseXS::Node::Sig $self = shift;

    my @args = map  {
                          $_->{var}
                        . (defined $_->{default_usage}
                            ?$_->{default_usage}
                            : ''
                          )
                    }
               grep {
                        defined $_->{arg_num},
                    }
               @{$self->{params}};

    push @args, '...' if $self->{seen_ellipsis};
    return join ', ', @args;
}


# $self->C_func_signature():
#
# return a string containing the arguments to pass to an autocall C
# function, e.g. 'a, &b, c'.

sub C_func_signature {
    my ExtUtils::ParseXS::Node::Sig $self = shift;
    my ExtUtils::ParseXS            $pxs  = shift;

    my @args;
    for my $param (@{$self->{params}}) {
        next if    $param->{is_synthetic} # THIS/CLASS/RETVAL
                   # if a synthetic RETVAL has acquired an arg_num, then
                   # it's appeared in the signature (although without a
                   # type) and has become semi-real.
                && !($param->{var} eq 'RETVAL' && defined($param->{arg_num}));

        if ($param->{is_length}) {
            push @args, "XSauto_length_of_$param->{len_name}";
            next;
        }

        if ($param->{var} eq 'SV *') {
            #backcompat placeholder
            $pxs->blurt("Error: parameter 'SV *' not valid as a C argument");
            next;
        }

        my $io = $param->{in_out};
        $io = '' unless defined $io;

        # Ignore fake/alien stuff, except an OUTLIST arg, which
        # isn't passed from perl (so no arg_num), but *is* passed to
        # the C function and then back to perl.
        next unless defined $param->{arg_num} or $io eq 'OUTLIST';
        
        my $a = $param->{var};
        $a = "&$a" if $param->{is_addr} or $io =~ /OUT/;
        push @args, $a;
    }

    return join(", ", @args);
}


# $self->proto_string():
#
# return a string containing the perl prototype string for this XSUB,
# e.g. '$$;$$@'.

sub proto_string {
    my ExtUtils::ParseXS::Node::Sig $self = shift;

    # Generate a prototype entry for each param that's bound to a real
    # arg. Use '$' unless the typemap for that param has specified an
    # overridden entry.
    my @p = map  defined $_->{proto} ? $_->{proto} : '$',
            grep defined $_->{arg_num} && $_->{arg_num} > 0,
            @{$self->{params}};

    my @sep = (';'); # separator between required and optional args
    my $min = $self->{min_args};
    if ($min < $self->{nargs}) {
        # has some default vals
        splice (@p, $min, 0, ';');
        @sep = (); # separator already added
    }
    push @p, @sep, '@' if $self->{seen_ellipsis};  # '...'
    return join '', @p;
}

1;

# vim: ts=4 sts=4 sw=4: et:
