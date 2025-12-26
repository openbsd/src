#!/usr/bin/perl -w

use v5.41;
use Text::Tabs;

# Unconditionally regenerate:
#
#    pod/perlintern.pod
#    pod/perlapi.pod

my $api = "pod/perlapi.pod";
my $intern = "pod/perlintern.pod";
#
# from information stored in
#
#    embed.fnc
#    plus all the core .c, .h, and .pod files listed in MANIFEST
#    plus %extra_input_pods

my %extra_input_pods = ( 'dist/ExtUtils-ParseXS/lib/perlxs.pod' => 1 );

# Has an optional arg, which is the directory to chdir to before reading
# MANIFEST and the files
#
# This script is invoked as part of 'make all'
#
# The generated pod consists of sections of related elements, functions,
# macros, and variables.  The keys of %valid_sections give the current legal
# ones.  Just add a new key to add a section.
#
# Throughout the files read by this script are lines like
#
# =for apidoc_section Section Name
# =for apidoc_section $section_name_variable
#
# "Section Name" (after having been stripped of leading space) must be one of
# the legal section names, or an error is thrown.  $section_name_variable must
# be one of the legal section name variables defined below; these expand to
# legal section names.  This form is used so that minor wording changes in
# these titles can be confined to this file.  All the names of the variables
# end in '_scn'; this suffix is optional in the apidoc_section lines.
#
# All API elements defined between this line and the next 'apidoc_section'
# line will go into the section "Section Name" (or $section_name_variable),
# sorted by dictionary order within it.  perlintern and perlapi are parallel
# documents, each potentially with a section "Section Name".  Each element is
# marked as to which document it goes into.  If there are none for a
# particular section in perlapi, that section is omitted.
#
# Also, in .[ch] files, there may be
#
# =head1 Section Name
#
# lines in comments.  These are also used by this program to switch to section
# "Section Name".  The difference is that if there are any lines after the
# =head1, inside the same comment, and before any =for apidoc-ish lines, they
# are used as a heading for section "Section Name" (in both perlintern and
# perlapi).  This includes any =head[2-5].  If more than one '=head1 Section
# Name' line has content, they appear in the generated pod in an undefined
# order.  Note that you can't use a $section_name_variable in =head1 lines
#
# The next =head1, =for apidoc_section, or file end terminates what goes into
# the current section
#
# The %valid_sections hash below also can have header content, which will
# appear before any =head1 content.  The hash can also have footer content
# content, which will appear at the end of the section, after all the
# elements.
#
# The lines that define the actual functions, etc are documented in embed.fnc,
# because they have flags which must be kept in sync with that file.

use strict;
use warnings;

my $known_flags_re =
                qr/[aA bC dD eE fF Gh iI mM nN oO pP rR sS T uU vW xX y;#?]/xx;

# Flags that don't apply to this program, like implementation details.
my $irrelevant_flags_re = qr/[ab eE G iI P rR vX?]/xx;

# Only certain flags dealing with what gets displayed, are acceptable for
# apidoc_item
my $item_flags_re = qr/[dD fF mM nN oO pT uU Wx;]/xx;

use constant {
              NOT_APIDOC         => -1,
              ILLEGAL_APIDOC     =>  0,  # Must be 0 so evaluates to 'false'
              APIDOC_DEFN        =>  1,
              PLAIN_APIDOC       =>  2,
              APIDOC_ITEM        =>  3,
              APIDOC_SECTION     =>  4,

              # This is the line type used for elements parsed in config.h.
              # Since that file is parsed after everything else, everything is
              # resolved by then; and this is just a way to allow prototypes
              # elsewhere in the source code to override the simplistic
              # prototypes that config.h mostly deals with.  Hence Configure
              # doesn't have to get involved.  There are just a few of these,
              # with little likelihood of changes needed.  They were manually
              # added to handy.h via 51b56f5c7c7.
              CONDITIONAL_APIDOC =>  5,
             };

my $config_h = 'config.h';
if (@ARGV >= 2 && $ARGV[0] eq "-c") {
    shift;
    $config_h = shift;
}

my $nroff_min_indent = 4;   # for non-heading lines
# 80 column terminal - 2 for pager using 2 columns for itself;
my $max_width = 80 - 2 - $nroff_min_indent;
my $standard_indent = 4;  # Any additional indentations

# In the usage (signature) section of entries, how many spaces should separate
# the return type from the name of the function.
my $usage_ret_name_sep_len = 2;

if (@ARGV) {
    my $workdir = shift;
    chdir $workdir
        or die "Couldn't chdir to '$workdir': $!";
}
require './regen/regen_lib.pl';
require './regen/embed_lib.pl';

# This is the main data structure.  Each key is the name of a potential API
# element available to be documentable.  Each value is a hash containing
# information about that element, such as its prototype.  It is initialized
# from embed.fnc, and added to as we go along.
my %elements;

# This hash is used to organize the data in %elements for output.  The top
# level keys are either 'api' or 'intern' for the two pod files generated by
# this program.
#
# Under those are hashes for each section in its corresponding pod.  There is
# a section for API elements involved with SV handling; another for AV
# handling, etc.
#
# Under each section are hashes for each group in the section.  Each group
# consists of one or more API elements that share the same pod.  Each group
# hash contains fields for the common parts of the group, and then an array of
# all the API elements that comprise it.  The array determines the ordering of
# the output for the API elements.
#
# The elements are pointers to the leaf nodes in %elements.  These contain all
# the information needed to know what to output for each.
my %docs;

# This hash is populated with the names of other pod files that we determine
# contain relevant information about the API elements.  It is used just for
# the SEE ALSO section
my %described_elsewhere;

# keys are the full 'Perl_FOO' names in proto.h.  values are currently
# unlooked at
my %protos;

my $described_in = "Described in";

my $description_indent = 4;
my $usage_indent = 3;   # + initial verbatim block blank yields 4 total

my $AV_scn = 'AV Handling';
my $callback_scn = 'Callback Functions';
my $casting_scn = 'Casting';
my $casing_scn = 'Character case changing';
my $classification_scn = 'Character classification';
my $names_scn = 'Character names';
my $scope_scn = 'Compile-time scope hooks';
my $compiler_scn = 'Compiler and Preprocessor information';
my $directives_scn = 'Compiler directives';
my $concurrency_scn = 'Concurrency';
my $COP_scn = 'COPs and Hint Hashes';
my $CV_scn = 'CV Handling';
my $custom_scn = 'Custom Operators';
my $debugging_scn = 'Debugging';
my $display_scn = 'Display functions';
my $embedding_scn = 'Embedding, Threads, and Interpreter Cloning';
my $errno_scn = 'Errno';
my $exceptions_scn = 'Exception Handling (simple) Macros';
my $filesystem_scn = 'Filesystem configuration values';
my $filters_scn = 'Source Filters';
my $floating_scn = 'Floating point';
my $genconfig_scn = 'General Configuration';
my $globals_scn = 'Global Variables';
my $GV_scn = 'GV Handling and Stashes';
my $hook_scn = 'Hook manipulation';
my $HV_scn = 'HV Handling';
my $io_scn = 'Input/Output';
my $io_formats_scn = 'I/O Formats';
my $integer_scn = 'Integer';
my $lexer_scn = 'Lexer interface';
my $locale_scn = 'Locales';
my $magic_scn = 'Magic';
my $memory_scn = 'Memory Management';
my $MRO_scn = 'MRO';
my $multicall_scn = 'Multicall Functions';
my $numeric_scn = 'Numeric Functions';
my $rpp_scn = 'Reference-counted stack manipulation';

# Now combined, as unclear which functions go where, but separate names kept
# to avoid 1) other code changes; 2) in case it seems better to split again
my $optrees_scn = 'Optrees';
my $optree_construction_scn = $optrees_scn; # Was 'Optree construction';
my $optree_manipulation_scn = $optrees_scn; # Was 'Optree Manipulation Functions'
my $pack_scn = 'Pack and Unpack';
my $pad_scn = 'Pad Data Structures';
my $password_scn = 'Password and Group access';
my $reports_scn = 'Reports and Formats';
my $paths_scn = 'Paths to system commands';
my $prototypes_scn = 'Prototype information';
my $regexp_scn = 'REGEXP Functions';
my $signals_scn = 'Signals';
my $site_scn = 'Site configuration';
my $sockets_scn = 'Sockets configuration values';
my $stack_scn = 'Stack Manipulation Macros';
my $string_scn = 'String Handling';
my $SV_flags_scn = 'SV Flags';
my $SV_scn = 'SV Handling';
my $tainting_scn = 'Tainting';
my $time_scn = 'Time';
my $typedefs_scn = 'Typedef names';
my $unicode_scn = 'Unicode Support';
my $utility_scn = 'Utility Functions';
my $versioning_scn = 'Versioning';
my $warning_scn = 'Warning and Dieing';
my $XS_scn = 'XS';

# Kept separate at end
my $undocumented_scn = 'Undocumented elements';

my %valid_sections = (
    $AV_scn => {},
    $callback_scn => {},
    $casting_scn => {},
    $casing_scn => {},
    $classification_scn => {},
    $scope_scn => {},
    $compiler_scn => {},
    $directives_scn => {},
    $concurrency_scn => {},
    $COP_scn => {},
    $CV_scn => {
        header => <<~'EOT',
            This section documents functions to manipulate CVs which are
            code-values, meaning subroutines.  For more information, see
            L<perlguts>.
            EOT
    },

    $custom_scn => {},
    $debugging_scn => {},
    $display_scn => {},
    $embedding_scn => {},
    $errno_scn => {},
    $exceptions_scn => {},
    $filesystem_scn => {
        header => <<~'EOT',
            Also see L</List of capability HAS_foo symbols>.
            EOT
        },
    $filters_scn => {},
    $floating_scn => {
        header => <<~'EOT',
            Also L</List of capability HAS_foo symbols> lists capabilities
            that arent in this section.  For example C<HAS_ASINH>, for the
            hyperbolic sine function.
            EOT
        },
    $genconfig_scn => {
        header => <<~'EOT',
            This section contains configuration information not otherwise
            found in the more specialized sections of this document.  At the
            end is a list of C<#defines> whose name should be enough to tell
            you what they do, and a list of #defines which tell you if you
            need to C<#include> files to get the corresponding functionality.
            EOT

        footer => <<~EOT,

            =head2 List of capability C<HAS_I<foo>> symbols

            This is a list of those symbols that dont appear elsewhere in ths
            document that indicate if the current platform has a certain
            capability.  Their names all begin with C<HAS_>.  Only those
            symbols whose capability is directly derived from the name are
            listed here.  All others have their meaning expanded out elsewhere
            in this document.  This (relatively) compact list is because we
            think that the expansion would add little or no value and take up
            a lot of space (because there are so many).  If you think certain
            ones should be expanded, send email to
            L<perl5-porters\@perl.org|mailto:perl5-porters\@perl.org>.

            Each symbol here will be C<#define>d if and only if the platform
            has the capability.  If you need more detail, see the
            corresponding entry in F<$config_h>.  For convenience, the list is
            split so that the ones that indicate there is a reentrant version
            of a capability are listed separately

            __HAS_LIST__

            And, the reentrant capabilities:

            __HAS_R_LIST__

            Example usage:

            =over $standard_indent

             #ifdef HAS_STRNLEN
               use strnlen()
             #else
               use an alternative implementation
             #endif

            =back

            =head2 List of C<#include> needed symbols

            This list contains symbols that indicate if certain C<#include>
            files are present on the platform.  If your code accesses the
            functionality that one of these is for, you will need to
            C<#include> it if the symbol on this list is C<#define>d.  For
            more detail, see the corresponding entry in F<$config_h>.

            __INCLUDE_LIST__

            Example usage:

            =over $standard_indent

             #ifdef I_WCHAR
               #include <wchar.h>
             #endif

            =back
            EOT
      },
    $globals_scn => {},
    $GV_scn => {},
    $hook_scn => {},
    $HV_scn => {},
    $io_scn => {},
    $io_formats_scn => {
        header => <<~'EOT',
            These are used for formatting the corresponding type For example,
            instead of saying

             Perl_newSVpvf(pTHX_ "Create an SV with a %d in it\n", iv);

            use

             Perl_newSVpvf(pTHX_ "Create an SV with a " IVdf " in it\n", iv);

            This keeps you from having to know if, say an IV, needs to be
            printed as C<%d>, C<%ld>, or something else.
            EOT
      },
    $integer_scn => {},
    $lexer_scn => {},
    $locale_scn => {},
    $magic_scn => {},
    $memory_scn => {},
    $MRO_scn => {},
    $multicall_scn => {},
    $numeric_scn => {},
    $optrees_scn => {},
    $optree_construction_scn => {},
    $optree_manipulation_scn => {},
    $pack_scn => {},
    $pad_scn => {},
    $password_scn => {},
    $paths_scn => {},
    $prototypes_scn => {},
    $regexp_scn => {},
    $reports_scn => {
        header => <<~"EOT",
            These are used in the simple report generation feature of Perl.
            See L<perlform>.
            EOT
      },
    $rpp_scn => {
        header => <<~'EOT',
            Functions for pushing and pulling items on the stack when the
            stack is reference counted. They are intended as replacements
            for the old PUSHs, POPi, EXTEND etc pp macros within pp
            functions.
            EOT
      },
    $signals_scn => {},
    $site_scn => {
        header => <<~'EOT',
            These variables give details as to where various libraries,
            installation destinations, I<etc.>, go, as well as what various
            installation options were selected
            EOT
      },
    $sockets_scn => {},
    $stack_scn => {},
    $string_scn => {
        header => <<~EOT,
            See also C<L</$unicode_scn>>.
            EOT
      },
    $SV_flags_scn => {},
    $SV_scn => {},
    $tainting_scn => {},
    $time_scn => {},
    $typedefs_scn => {},
    $unicode_scn => {
        header => <<~EOT,
            L<perlguts/Unicode Support> has an introduction to this API.

            See also C<L</$classification_scn>>,
            C<L</$casing_scn>>,
            and C<L</$string_scn>>.
            Various functions outside this section also work specially with
            Unicode.  Search for the string "utf8" in this document.
            EOT
      },
    $utility_scn => {},
    $versioning_scn => {},
    $warning_scn => {},
    $XS_scn => {},
);

# The section that is in effect at the beginning of the given file.  If not
# listed here, an apidoc_section line must precede any apidoc lines.
# This allows the files listed here that generally are single-purpose, to not
# have to worry about the autodoc section
my %initial_file_section = (
                            'av.c' => $AV_scn,
                            'av.h' => $AV_scn,
                            'cv.h' => $CV_scn,
                            'deb.c' => $debugging_scn,
                            'dist/ExtUtils-ParseXS/lib/perlxs.pod' => $XS_scn,
                            'doio.c' => $io_scn,
                            'gv.c' => $GV_scn,
                            'gv.h' => $GV_scn,
                            'hv.h' => $HV_scn,
                            'locale.c' => $locale_scn,
                            'malloc.c' => $memory_scn,
                            'numeric.c' => $numeric_scn,
                            'opnames.h' => $optree_construction_scn,
                            'pad.h'=> $pad_scn,
                            'patchlevel.h' => $versioning_scn,
                            'perlio.h' => $io_scn,
                            'pod/perlapio.pod' => $io_scn,
                            'pod/perlcall.pod' => $callback_scn,
                            'pod/perlembed.pod' => $embedding_scn,
                            'pod/perlfilter.pod' => $filters_scn,
                            'pod/perliol.pod' => $io_scn,
                            'pod/perlmroapi.pod' => $MRO_scn,
                            'pod/perlreguts.pod' => $regexp_scn,
                            'pp_pack.c' => $pack_scn,
                            'pp_sort.c' => $SV_scn,
                            'regcomp.c' => $regexp_scn,
                            'regexp.h' => $regexp_scn,
                            'sv.h' => $SV_scn,
                            'sv.c' => $SV_scn,
                            'sv_inline.h' => $SV_scn,
                            'taint.c' => $tainting_scn,
                            'unicode_constants.h' => $unicode_scn,
                            'utf8.c' => $unicode_scn,
                            'utf8.h' => $unicode_scn,
                            'vutil.c' => $versioning_scn,
                           );

sub where_from_string ($file, $line_num = 0) {

    # Returns a string of hopefully idiomatic text about the location given by
    # the input parameters.  The line number is not always available, and this
    # centralizes into one function the logic to deal with that

    return "in $file" unless $line_num;
    return "at $file, line $line_num";
}

sub check_and_add_proto_defn {
    my ($element, $file, $line_num, $raw_flags, $ret_type, $args_ref,
        $definition_type
       ) = @_;

    # This function constructs a hash describing what we know so far about the
    # API element '$element', as determined by 'apidoc'-type lines scattered
    # throughout the source code tree.
    #
    # This includes how to use it, and if documentation is expected for it,
    # and if any such documentation has been found.  (The documentation itself
    # is stored separately, as any number of elements may share the same pod.)
    #
    # Except in limited circumstances, only one definition is allowed; that is
    # checked for.
    #
    # The parameters: $flags, $ret_type, and $args_ref are sufficient to
    # define the usage.  They are checked for legality, to the limited extent
    # possible.  The arguments may include conventions like NN, which is a
    # hint internal to embed.fnc, but which someone using the API should not
    # be expected to know.  This function strips those.
    #
    # As we parse the source, we may find an apidoc-type line that refers to
    # $element before we have found the usage information.  A hash is
    # constructed in this case that is effectively a place-holder for the
    # usage, waiting to be filled in later in the parse.  A place-holder call
    # is signalled by all three mentioned parameters being empty.  These
    # lines, however, are markers in the code where $element is documented.
    # So they indicate that there is pod available for it.  That information
    # is added to the hash.
    #
    # It is possible for this to be called with a line that both defines the
    # usage signature for $element, and marks the place in the source where
    # the documentation is found.  Handling that happens naturally here.

    # This definition type is currently used only by config.h.  See comments
    # at the definition of this line type.  If there is an existing prototype
    # definition, defer to that (by setting the parameters to empty);
    # otherwise use the one passed in.
    if ($definition_type == CONDITIONAL_APIDOC) {
        if (exists $elements{$element}) {
            my @dummy;
            $raw_flags = "";
            $ret_type = "";
            $args_ref = \@dummy;
        }

        $definition_type = PLAIN_APIDOC;
    }

    my $flags = $raw_flags =~ s/$irrelevant_flags_re//gr;
    my $illegal_flags = $flags =~ s/$known_flags_re//gr;
    if ($illegal_flags) {
        die "flags [$illegal_flags] not legal for function"
          . " $element " . where_from_string($file, $line_num);
    }

    $flags .= "m" if $flags =~ /M/;

    my @munged_args= $args_ref->@*;
    s/\b(?:NN|NULLOK)\b\s+//g for @munged_args;

    my $flags_sans_d = $flags;
    my $docs_expected = $flags_sans_d =~ s/d//g;
    my $docs_hidden = $flags =~ /h/;

    # Does this call define the signature for $element?  It always does for
    # APIDOC_DEFN lines, and for the other types when one of the usage
    # parameters is non-empty, except when the flags indicate the actual
    # definition is in some other pod.
    my $is_usage_defining_occurrence =
            (    $definition_type == APIDOC_DEFN
             || (   ! $docs_hidden
                 && (   $flags_sans_d
                     || $ret_type
                     || ($args_ref && $args_ref->@*))));

    # Check this new entry against an existing one.
    if (   $is_usage_defining_occurrence
        && $elements{$element} && $elements{$element}{proto_defined})
    {
            # Some functions in embed.fnc have multiple definitions depending
            # on the platform's Configuration.  Currently we just use the
            # first one encountered in that file.
            return \$elements{$element}
                                   if $file eq 'embed.fnc'
                                   && $elements{$element}{file} eq 'embed.fnc';

            # Use the existing entry if both it and this new attempt to create
            # one have the 'h' flag set.  This flag indicates that the entry
            # is just a reference to the pod where the element is actually
            # documented, so multiple such lines can peacefuly coexist.
            return \$elements{$element} if $docs_hidden
                                       && $elements{$element}{flags} =~ /h/;

            die "There already is an existing prototype for '$element' defined "
              . where_from_string($elements{$element}{proto_defined}{file},
                                  $elements{$element}{proto_defined}{line_num})
              . " new one is " . where_from_string($file, $line_num);
    }

    # Here, any existing entry for this element is a placeholder.  If none,
    # create one.  If a placeholder entry, override it with this new
    # information.
    if ($is_usage_defining_occurrence || ! $elements{$element}) {
        $elements{$element}{name} = $element;
        $elements{$element}{raw_flags} = $raw_flags; # Keep for debugging, etc.
        $elements{$element}{flags} = $flags_sans_d;
        $elements{$element}{ret_type} =$ret_type;
        $elements{$element}{args} = \@munged_args;
        $elements{$element}{file} = $file;
        $elements{$element}{line_num} = $line_num // 0;

        # Don't reset expecting documentation.
        $elements{$element}{docs_expected} = $docs_expected
                                      unless $elements{$element}{docs_expected};

        if ($is_usage_defining_occurrence) {
            $elements{$element}{proto_defined} = {
                                                  type => $definition_type,
                                                  file     => $file,
                                                  line_num => $line_num // 0,
                                                 };
        }
    }

    # All but this type are for defining pod
    if ($definition_type != APIDOC_DEFN) {
        if (   $elements{$element}{docs_found}
            && ! $docs_hidden
            && $elements{$element}{flags} !~ /h/ )
        {
            die "Attempting to document '$element' "
              .  where_from_string($file, $line_num)
              . "; it was already documented "
              . where_from_string($elements{$element}{docs_found}{file},
                                  $elements{$element}{docs_found}{line_num});
        }
        else {
            $elements{$element}{docs_found}{file} = $file;
            $elements{$element}{docs_found}{line_num} = $line_num // 0;
        }

        if ($definition_type == PLAIN_APIDOC) {
            $elements{$element}{is_leader} = 1;
        }
    }

    return \$elements{$element};
}

sub classify_input_line ($file, $line_num, $input, $is_file_C) {

    # Looks at an input line and classifies it as to if it is of use to us or
    # not, and if so, what class of apidoc line it is.  It looks for common
    # typos in the input lines in order to do the classification, but dies
    # when one is encountered.
    #
    # It returns a pair of values.  The first is the classification; the
    # second the trailing text of the input line, trimmed of leading and
    # trailing spaces.  This is viewed as the argument.
    #
    # The returned classification is one of the constants defined at the
    # beginning of the program, like NOT_APIDOC.
    #
    # For NOT_APIDOC only, the returned argument is not trimmed; it is the
    # whole line, including any \n.
    #
    # In C files only, a =head1 line is equivalent to an apidoc_section line,
    # so the latter is returned for this case.

    # Use simple patterns to quickly rule out lines that are of no interest to
    # us, which are the vast majority.
    return (NOT_APIDOC, $input)
                    if                   $input !~ / api [-_]? doc /x
                    and (! $is_file_C || $input !~ / head \s* 1 \s+ (.) /x);

    # Only the head1 lines have a capture group.  That capture was done solely
    # to be able to use its existence as a shortcut to distinguish between the
    # patterns here.
    if (defined $1) {

        # We repeat the match above, to handle the case where there is more
        # than one head1 strings on the line
        return (NOT_APIDOC, $input) unless $input =~ / ^
                                                       (\s*)  # $1
                                                       (=?)   # $2
                                                       head
                                                       (\s*)  # $3
                                                       1 \s+
                                                       (.*)   # $4
                                                     /x;
        # Here, it looks like the line was meant to be a =head1.  This is
        # equivalent to an apidoc_section line if properly formed
        return (APIDOC_SECTION, $4) if length $1 == 0
                                    && length $2 == 1
                                    && length $3 == 0;
        # Drop down to give error
    }
    else {

        # Here, the input has something like 'apidoc' in it.  See if we think
        # it was meant to be one.
        return (NOT_APIDOC, $input) unless $input =~ / ^
                                                       (\s*)      # $1
                                                       (=?)       # $2
                                                       (\s*)      # $3
                                                       (for)?     # $4
                                                       (\s*)      # $5
                                                       api
                                                       ([_-]?)    # $6
                                                       doc
                                                       ([-_]?)    # $7
                                                       (\w*)      # $8
                                                       (\s*)      # $9
                                                       (.*?)      # $10
                                                       \s* \n
                                                     /x;
        my $type_name = $8;
        my $arg = $10;

        my $type = ($type_name eq "")
                   ? PLAIN_APIDOC
                   : ($type_name eq 'item')
                     ? APIDOC_ITEM
                     : ($type_name eq 'defn')
                       ? APIDOC_DEFN
                       : ($type_name eq 'section')
                         ? APIDOC_SECTION
                         : ILLEGAL_APIDOC;

        my $mostly_proper_form =
                   (   $type != ILLEGAL_APIDOC
                    && length $2 == 1       # Must have '='
                    && length $3 == 0       # Must not have space after '='
                    && defined $4           # Must have 'for'
                    && length $5 > 0        # Must have space after =for
                    && length $6 == 0       # 'apidoc' is one word

                        # plain apidoc has no trailing underscore; others have
                        # an underscore separator
                    && (   ($type == PLAIN_APIDOC && length $7 == 0)
                        || ($type != PLAIN_APIDOC && $7 eq '_'))

                        # Must have space before argument except if
                        # apidoc_item and first char of arg is '|'
                    && (   length $9 != 0
                        || (   $type == APIDOC_ITEM
                            && substr($10, 0, 1) eq '|')));
        if ($mostly_proper_form) {
            return ($type, $arg) if length $1 == 0; # Is fully correct if left
                                                    # justified

            # Ordinarily not being left justified is an error, but special
            # case perlguts which has examples of how to use apidoc in
            # verbatim blocks, which we don't want to confuse with a real
            # instance and reject because it isn't left justified.  Use a
            # precise indent to get the precise lines that have this.
            return (NOT_APIDOC, $input) if length $1 == 1
                                        && $file eq 'pod/perlguts.pod';
        }
    }

    chomp $input;
    my $where_from = where_from_string($file, $line_num);
    die <<~EOS;
        '$input' $where_from is not of proper form

        Expected ([...] means optional; | is literal, not a meta char):
        =for apidoc         name
        =for apidoc         [flags] | [returntype] | name [|arg1] [|arg2] [|...]
        =for apidoc_item    name
        =for apidoc_item    [flags] | [returntype] | name [|arg1] [|arg2] [|...]
        =for apidoc_defn    flags|returntype|name[|arg|arg|...]
        =for apidoc_section name
        =for apidoc_section \$variable
        EOS
}

sub handle_apidoc_line ($file, $line_num, $type, $arg) {

    # This just does a couple of checks that would otherwise have to be
    # duplicated in the calling code, and calls check_and_add_proto_defn() to
    # do the real work.

    my $proto_as_written = $arg;
    my $proto = $proto_as_written;
    $proto = "||$proto" if $proto !~ /\|/;
    my ($flags, $ret_type, $name, @args) = split /\s*\|\s*/, $proto;

    if ($type == APIDOC_ITEM) {
        if (my $non_item_flags = $flags =~ s/$item_flags_re//gr) {
            die "[$non_item_flags] illegal in apidoc_item "
              . where_from_string($file, $line_num)
              . " :\n$arg";
        }
    }

    if ($flags =~ /#/) {
        die "Return type must be empty for '$name' "
          . where_from_string($file, $line_num) if $ret_type;
        $ret_type = '#ifdef';
    }

    warn ("'$name' not \\w+ in '$proto_as_written' "
       .  where_from_string($file, $line_num))
                        if $flags !~ /N/
                        && $name !~ / ^ (?:struct\s+)? [_[:alpha:]] \w* $ /x;

    # Here, done handling any existing information about this element.  Add
    # this definition (which has the side effect of cleaning up any NN or
    # NULLOK in @args)
    my $updated = check_and_add_proto_defn($name, $file, $line_num,

                    # The fact that we have this line somewhere in the source
                    # code means we implicitly have the 'd' flag
                    $flags . "d",
                    $ret_type, \@args,
                    $type);

    return $updated;
}

sub destination_pod ($flags) {  # Into which pod should the element go whose
                                # flags are $1
    return "unknown" if $flags eq "";
    return $api if $flags =~ /A/;
    return $intern;
}

sub autodoc ($fh, $file) {  # parse a file and extract documentation info

    my $section = $initial_file_section{$file}
                                    if defined $initial_file_section{$file};
    my $file_is_C = $file =~ / \. [ch] $ /x;

    # Count lines easier and handle apidoc continuation lines
    my $line_num;
    my $prev_type;
    my $prev_arg;
    my $do_unget = 0;

    my $unget_next_line = sub () {
        die "Attempt to unget more than one line" if $do_unget;
        $do_unget = 1;
    };

    # Reads, categorizes, and returns the relevant portion of the next input
    # line, while joining apidoc-type lines that have continuations into a
    # single line.  For non-apidoc-type lines, the possibility of continuation
    # lines is not considered (avoiding unintended consequences).  For these,
    # the entire line is returned, including trailing \n.
    #
    # For apidoc-type lines, only the argument portion of the line is
    # returned, chomped.  (The returned type tells you what the beginning
    # was.)  A continuation happens when the final non-space character on it
    # is a backslash.
    #
    # (If a non-api-doc line ends with a backslash, and the next line looks
    # like an apidoc-ish line, this algorithm causes it to be treated as an
    # apidoc line.  This might be considered a bug, or the right thing to do.)
    my $get_next_line = sub {

            if ($do_unget) {
                $do_unget = 0;
                return ($prev_type, $prev_arg);
            }

            my $contents = <$fh>;
            if (! defined $contents) {
                undef $prev_type;
                undef $prev_arg;
                return;
            }

            $line_num++;
            ($prev_type, $prev_arg) = classify_input_line($file, $line_num,
                                                          $contents,
                                                          $file_is_C);
            return ($prev_type, $prev_arg) if $prev_type == NOT_APIDOC;

            # Replace all spaces around a backslash at the end of a line with
            # a single space to prepare for the continuation line to be joined
            # with this.  (This includes lines with spaces betweeen the
            # backslash and \n, since a human reader would not readily see the
            # distinction.)
            while ($prev_arg =~ s/ \s* \\ \s* $ / /x) {
                my $next = <$fh>;
                last unless defined $next;

                $line_num++;
                $prev_arg .= $next;
            }

            return ($prev_type, $prev_arg);
    };

    # Read the file.  Most lines are of no interest to this program, but
    # individual 'apidoc_defn' lines are, as well as are blocks introduced by
    # 'apidoc_section' and 'apidoc'.  Text in those blocks is used
    # respectively for the section heading or pod.  Between plain 'apidoc'
    # lines and its pod, may be any number of 'apidoc_item' lines that give
    # additional api elements that the pod applies to.

    my $destpod;
    while (1) {
        my ($outer_line_type, $arg) = $get_next_line->();
        last unless defined $outer_line_type;
        next if $outer_line_type == NOT_APIDOC;

        my $element_name;
        my @items;
        my $flags = "";
        my $text = "";

        if ($outer_line_type == APIDOC_ITEM) {
            die "apidoc_item doesn't immediately follow an apidoc entry:"
              . " '$arg' " . where_from_string($file, $line_num);
        }
        elsif ($outer_line_type == APIDOC_DEFN) {
            handle_apidoc_line($file, $line_num, $outer_line_type, $arg);
            next;   # 'handle_apidoc_line' handled everything for this type
        }
        elsif ($outer_line_type == APIDOC_SECTION) {

            # Here the line starts a new section ...
            $section = $arg;

            # Convert $foo to its value
            if ($section =~ / ^ \$ /x) {
                $section .= '_scn' unless $section =~ / _scn $ /x;
                $section = eval "$section";
                die "Unknown \$section variable '$section' "
                  . where_from_string($file, $line_num) . "\n$@"  if $@;
            }
            die "Unknown section name '$section' in $file near line $line_num\n"
                                    unless defined $valid_sections{$section};

            # Drop down to accumulate the heading text for this section.
        }
        elsif ($outer_line_type == PLAIN_APIDOC) {
            my $leader_ref =
                  handle_apidoc_line($file, $line_num, $outer_line_type, $arg);
            $destpod = destination_pod($$leader_ref->{flags});

            push @items, $leader_ref;

            # Now look for any 'apidoc_item' lines.  These are in a block
            # consisting solely of them, or all-blank lines
            while (1) {
                (my $item_line_type, $arg) = $get_next_line->();
                last unless defined $item_line_type;

                # Absorb blank lines
                if ($item_line_type == NOT_APIDOC && $arg !~ /\S/) {
                    $text .= $arg;
                    next;
                }

                last unless $item_line_type == APIDOC_ITEM;

                # Reset $text; those blank lines it contains merely are
                # separating 'apidoc_item' lines
                $text = "";

                my $item_ref =
                  handle_apidoc_line($file, $line_num, $item_line_type, $arg);

                push @items, $item_ref;
            }

            # Put back the line that terminated this block of items, so that
            # the code below will get it as the first line.
            $unget_next_line->();

            # Drop down to accumulate the pod for this group.  $text contains
            # any blank lines that follow the final 'apidoc_item' line.
            # $input is the next line to process
        }
        else {
            die "Unknown apidoc-type line '$arg' "
              . where_from_string($file, $line_num);
        }

        # Here, we are ready to accumulate text into either a heading, or the
        # pod for an apidoc line.  $text may already contain blank lines that
        # are part ot this text.
        #
        # Accumulation stops at a terminating line, which is one of:
        # 1) =cut
        # 2) =headN (N must be 1 in a C file)
        # 3) an end comment line in a C file: m: ^ \s* [*] / :x
        # 4) =for apidoc... (except apidoc_item lines)
        my $head_ender_num = ($file_is_C) ? 1 : "";
        while (1) {
            my ($inner_line_type, $inner_arg) = $get_next_line->();
            last unless defined $inner_line_type;

            last unless $inner_line_type == NOT_APIDOC;
            last if $inner_arg =~ /^=cut/x;
            last if $inner_arg =~ /^=head$head_ender_num/;

            if ($file_is_C && $inner_arg =~ m: ^ \s* \* / $ :x) {

                # End of comment line in C files is a fall-back
                # terminator, but warn only if there actually is some
                # accumulated text
                warn "=cut missing? "
                   . where_from_string($file, $line_num)
                   . "\n$inner_arg"                             if $text =~ /\S/;
                last;
            }

            $text .= $inner_arg;
        }

        # Here, are done accumulating the text for this element.  Trim it
        $text =~ s/ ^ \s* //x;
        $text =~ s/ \s* $ //x;
        $text .= "\n" if $text ne "";

        # And treat all-spaces as nothing at all
        undef $text unless $text =~ /\S/;

        if ($outer_line_type == APIDOC_SECTION) {
            if ($text) {
                $valid_sections{$section}{header} = "" unless
                                    defined $valid_sections{$section}{header};
                $valid_sections{$section}{header} .= "\n$text";
            }
        }
        else {
            my $item0 = ${$items[0]};
            my $element_name = $item0->{name};

            # Here, into $text, we have accumulated the pod for $element_name

            die "No =for apidoc_section nor =head1 "
              . where_from_string($file, $line_num)
              . " for'$element_name'\n"             unless defined $section;
            my $is_link_only = ($item0->{flags} =~ /h/);
            if (   ! $is_link_only
                && exists $docs{$destpod}{$section}{$element_name})
            {
                warn "$0: duplicate API entry for '$element_name'"
                   . " $destpod/$section "
                   . where_from_string($file, $line_num);
                next;
            }

            # Override the text with just a link if the flags call for that
            if ($is_link_only) {
                if ($file_is_C) {
                    die "Can't currently handle link with items to it "
                      . where_from_string($file, $line_num)
                      . ":\n$arg"                           if @items > 1;
                    $docs{$destpod}{$section}{X_tags}{$element_name} = $file;

                    # Don't put anything if C source
                    $unget_next_line->();
                    next;
                }

                # Here, is an 'h' flag in pod.  We add a reference to the pod
                # (and nothing else) to perlapi/intern.  (It would be better
                # to add a reference to the correct =item,=header, but
                # something that makes it harder is that it that might be a
                # duplicate, like '=item *'; so that is a future enhancement
                # XXX.  Another complication is there might be more than one
                # deserving candidates.)
                my $podname = $file =~ s!.*/!!r;    # Rmv directory name(s)
                $podname =~ s/\.pod//;
                $text = "$described_in L<$podname>.\n";

                # Keep track of all the pod files that we refer to.
                push $described_elsewhere{$podname}->@*, $podname;
            }

            $docs{$destpod}{$section}{$element_name}{pod} = $text;
            $docs{$destpod}{$section}{$element_name}{items} = \@items;
        }

        # We already have the first line of what's to come in $arg
        $unget_next_line->();

    } # End of loop through input
}

my %configs;
my @has_defs;
my @has_r_defs;     # Reentrant symbols
my @include_defs;

sub parse_config_h {
    use re '/aa';   # Everything is ASCII in this file

    # Process config.h
    die "Can't find $config_h" unless -e $config_h;
    open my $fh, '<', $config_h or die "Can't open $config_h: $!";
    while (<$fh>) {

        # Look for lines like /* FOO_BAR:
        # By convention all config.h descriptions begin like that
        my $line_num = $.;
        if (m[ ^ /\* [ ] ( [[:alpha:]] \w+ ) : \s* $ ]ax) {
            my $name = $1;
            $configs{$name}{docs_line_num} = $line_num;

            # Here we are starting the description for $name in config.h.  We
            # accumulate the entire description for it into @description.
            # Flowing text from one input line to another is appended into the
            # same array element to make a single flowing line element, but
            # verbatim lines are kept as separate elements in @description.
            # This will facilitate later doing pattern matching without regard
            # to line boundaries on non-verbatim text.

            die "Multiple $config_h entries for '$name'"
                                        if defined $configs{$name}{description};

            # Get first line of description
            $_ = <$fh>;

            # Each line in the description begins with blanks followed by '/*'
            # and some spaces.
            die "Unexpected $config_h initial line for $name: '$_'"
                                            unless s/ ^ ( \s* \* \s* ) //x;
            my $initial_text = $1;

            # Initialize the description with this first line (after having
            # stripped the prefix text)
            my @description = $_;

            # The first line is used as a template for how much indentation
            # each normal succeeding line has.  Lines indented further
            # will be considered as intended to be verbatim.  But, empty lines
            # likely won't have trailing blanks, so just strip the whole thing
            # for them.
            my $strip_initial_qr = qr!   \s* \* \s* $
                                    | \Q$initial_text\E
                                    !x;
            $configs{$name}{verbatim} = 0;

            # Read in the remainder of the description
            while (<$fh>) {
                last if s| ^ \s* \* / ||x;  # A '*/' ends it

                die "Unexpected $config_h description line for $name: '$_'"
                                                unless s/$strip_initial_qr//;

                # Fix up the few flawed lines in config.h wherein a new
                # sentence begins with a tab (and maybe a space after that).
                # Although none of them currently do, let it recognize
                # something like
                #
                #   "... text").  The next sentence ...
                #
                s/ ( \w "? \)? \. ) \t \s* ( [[:alpha:]] ) /$1  $2/xg;

                # If this line has extra indentation or looks to have columns,
                # it should be treated as verbatim.  Columns are indicated by
                # use of interior: tabs, 3 spaces in a row, or even 2 spaces
                # not preceded by punctuation.
                if ($_ !~ m/  ^ \s
                              | \S (?:                    \t
                                    |                     \s{3}
                                    |  (*nlb:[[:punct:]]) \s{2}
                                   )
                           /x)
                {
                    # But here, is not a verbatim line.  Add an empty line if
                    # this is the first non-verbatim after a run of verbatims
                    if ($description[-1] =~ /^\s/) {
                        push @description, "\n", $_;
                    }
                    else {  # Otherwise, append this flowing line to the
                            # current flowing line
                        $description[-1] .= $_;
                    }
                }
                else {
                    $configs{$name}{verbatim} = 1;

                    # The first verbatim line in a run of them is separated by
                    # an empty line from the flowing lines above it
                    push @description, "\n" if $description[-1] =~ /^\S/;

                    $_ = Text::Tabs::expand($_);

                    # Only a single space so less likely to wrap
                    s/ ^ \s* / /x;

                    push @description, $_;
                }
            }

            push $configs{$name}{description}->@*, @description

        }   # Not a description; see if it is a macro definition.
        elsif (m! ^
                  (?: / \* )?                   # Optional commented-out
                                                # indication
                      \# \s* define \s+ ( \w+ ) # $1 is the name
                  (   \s* )                     # $2 indicates if args or not
                  (   .*? )                     # $3 is any definition
                  (?: / \s* \* \* / )?          # Optional trailing /**/
                                                # or / **/
                  $
                !x)
        {
            my $name = $1;

            # There can be multiple definitions for a name.  We want to know
            # if any of them has arguments, and if any has a body.
            $configs{$name}{has_args} //= $2 eq "";
            $configs{$name}{has_args} ||= $2 eq "";
            $configs{$name}{has_defn} //= $3 ne "";
            $configs{$name}{has_defn} ||= $3 ne "";
            $configs{$name}{defn_line_num} = $line_num;
        }
    }

    # We now have stored the description and information about every #define
    # in the file.  The description is in a form convenient to operate on to
    # convert to pod.  Do that now.
    foreach my $name (keys %configs) {
        next unless defined $configs{$name}{description};

        # All adjacent non-verbatim lines of the description are appended
        # together in a single element in the array.  This allows the patterns
        # to work across input line boundaries.

        my $pod = "";
        while (defined ($_ = shift $configs{$name}{description}->@*)) {
            chomp;

            if (/ ^ \S /x) {  # Don't edit verbatim lines

                # Enclose known file/path names not already so enclosed
                # with <...>.  (Some entries in config.h are already
                # '<path/to/file>')
                my $file_name_qr = qr! [ \w / ]+ \.
                                    (?: c | h | xs | p [lm] | pmc | PL
                                        | sh | SH | exe ) \b
                                    !xx;
                my $path_name_qr = qr! (?: / \w+ )+ !x;
                my $file_or_path_name_qr = qr!
                        $file_name_qr
                    |
                        $path_name_qr
                    |
                        INSTALL \b
                !x;
                s! (*nlb:[ < \w / ]) ( $file_or_path_name_qr ) !<$1>!gxx;

                # Enclose <... file/path names with F<...> (but no double
                # angle brackets)
                s! < ( $file_or_path_name_qr ) > !F<$1>!gxx;

                # Explain metaconfig units
                s/ ( \w+ \. U \b ) /$1 (part of metaconfig)/gx;

                # Convert "See foo" to "See C<L</foo>>" if foo is described in
                # this file.
                # And, to be more general, handle "See also foo and bar", and
                # "See also foo, bar, and baz"
                while (m/ \b [Ss]ee \s+
                         (?: also \s+ )?    ( \w+ )
                         (?: ,  \s+         ( \w+ ) )?
                         (?: ,? \s+ and \s+ ( \w+ ) )? /xg) {
                    my @links = $1;
                    push @links, $2 if defined $2;
                    push @links, $3 if defined $3;
                    foreach my $link (@links) {
                        if (grep { $link =~ / \b $_ \b /x } keys %configs) {
                            s| \b $link \b |C<L</$link>>|xg;
                            $configs{$link}{linked} = 1;
                            $configs{$name}{linked} = 1;
                        }
                    }
                }

                # Enclose what we think are symbols with C<...>.
                s/ (*nlb:<)
                   (
                        # Any word followed immediately with parens or
                        # brackets
                        \b \w+ (?: \( [^)]* \)    # parameter list
                                 | \[ [^]]* \]    # or array reference
                               )
                    | (*nlb: \S ) -D \w+    # Also -Dsymbols.
                    | \b (?: struct | union ) \s \w+

                        # Words that contain underscores (which are
                        # definitely not text) or three uppercase letters in
                        # a row.  Length two ones, like IV, aren't enclosed,
                        # because they often don't look as nice.
                    | \b \w* (?: _ | [[:upper:]]{3,} ) \w* \b
                   )
                    (*nla:>)
                 /C<$1>/xg;

                # These include foo when the name is HAS_foo.  This is a
                # heuristic which works in most cases.
                if ($name =~ / ^ HAS_ (.*) /x) {
                    my $symbol = lc $1;

                    # Don't include path components, nor things already in
                    # <>, or with trailing '(', '['
                    s! \b (*nlb:[/<]) $symbol (*nla:[[/>(]) \b !C<$symbol>!xg;
                }
            }

            $pod .=  "$_\n";
        }
        delete $configs{$name}{description};

        $configs{$name}{pod} = $pod;
    }

    # Now have converted the description to pod.  We also now have enough
    # information that we can do cross checking to find definitions without
    # corresponding pod, and see if they are mentioned in some description;
    # otherwise they aren't documented.
  NAME:
    foreach my $name (keys %configs) {

        # A definition without pod
        if (! defined $configs{$name}{pod}) {

            # Leading/trailing underscore means internal to config.h, e.g.,
            # _GNU_SOURCE
            next if $name =~ / ^ _ /x;
            next if $name =~ / _ $ /x;

            # MiXeD case names are internal to config.h; the first 4
            # characters are sufficient to determine this
            next if $name =~ / ^ [[:upper:]] [[:lower:]]
                                 [[:upper:]] [[:lower:]]
                            /x;

            # Here, not internal to config.h.  Look to see if this symbol is
            # mentioned in the pod of some other.  If so, assume it is
            # documented.
            foreach my $check_name (keys %configs) {
                my $this_element = $configs{$check_name};
                my $this_pod = $this_element->{pod};
                if (defined $this_pod) {
                    next NAME if $this_pod =~ / \b $name \b /x;
                }
            }

            warn "$name has no documentation "
               . where_from_string($config_h, $configs{$name}{defn_line_num});

            next;
        }

        my $has_defn = $configs{$name}{has_defn};
        my $has_args = $configs{$name}{has_args};

        # Check if any section already has an entry for this element.
        # If so, it better be a placeholder, in which case we replace it
        # with this entry.
        foreach my $section (keys $docs{$api}->%*) {
            if (exists $docs{$api}{$section}{$name}) {
                my $was = $docs{$api}{$section}{$name}->{pod};
                $was = "" unless $was;
                chomp $was;
                if ($was ne "" && $was !~ m/$described_in/) {
                    die "Multiple descriptions for $name\n"
                      . "The '$section' section contained\n'$was'";
                }
                $docs{$api}{$section}{$name}->{pod} = $configs{$name}{pod};
                $configs{$name}{section} = $section;
                last;
            }
            elsif (exists $docs{$intern}{$section}{$name}) {
                die "'$name' is in '$config_h' meaning it is part of the API,\n"
                  . " but it is also in 'perlintern', meaning it isn't API\n";
            }
        }

        my $handled = 0;    # Haven't handled this yet

        if (defined $configs{$name}{'section'}) {
            # This has been taken care of elsewhere.
            $handled = 1;
        }
        else {
            my $flags = "";
            if ($has_defn && ! $has_args) {
                $configs{$name}{args} = 1;
            }

            # Symbols of the form I_FOO are for #include files.  They have
            # special usage information
            if ($name =~ / ^ I_ ( .* ) /x) {
                my $file = lc $1 . '.h';
                $configs{$name}{usage} = <<~"EOT";
                    #ifdef $name
                        #include <$file>
                    #endif
                    EOT
            }

            # Compute what section this variable should go into.  This
            # heuristic was determined by manually inspecting the current
            # things in config.h, and should be adjusted as necessary as
            # deficiencies are found.
            #
            # This is the default section for macros with a definition but
            # no arguments, meaning it is replaced unconditionally
            #
            my $sb = qr/ _ | \b /x; # segment boundary
            my $dash_or_spaces = qr/ - | \s+ /x;
            my $pod = $configs{$name}{pod};
            if ($name =~ / ^ USE_ /x) {
                $configs{$name}{'section'} = $site_scn;
            }
            elsif ($name =~ / SLEEP | (*nlb:SYS_) TIME | TZ | $sb TM $sb /x)
            {
                $configs{$name}{'section'} = $time_scn;
            }
            elsif (   $name =~ / ^ [[:alpha:]]+ f $ /x
                   && $configs{$name}{pod} =~ m/ \b format \b /ix)
            {
                $configs{$name}{'section'} = $io_formats_scn;
            }
            elsif ($name =~ /  DOUBLE | FLOAT | LONGDBL | LDBL | ^ NV
                            | $sb CASTFLAGS $sb
                            | QUADMATH
                            | $sb (?: IS )? NAN
                            | $sb (?: IS )? FINITE
                            /x)
            {
                $configs{$name}{'section'} =
                                    $floating_scn;
            }
            elsif ($name =~ / (?: POS | OFF | DIR ) 64 /x) {
                $configs{$name}{'section'} = $filesystem_scn;
            }
            elsif (   $name =~ / $sb (?: BUILTIN | CPP ) $sb | ^ CPP /x
                   || $configs{$name}{pod} =~ m/ \b align /x)
            {
                $configs{$name}{'section'} = $compiler_scn;
            }
            elsif ($name =~ / ^ [IU] [ \d V ]
                            | ^ INT | SHORT | LONG | QUAD | 64 | 32 /xx)
            {
                $configs{$name}{'section'} = $integer_scn;
            }
            elsif ($name =~ / $sb t $sb /x) {
                $configs{$name}{'section'} = $typedefs_scn;
                $flags .= 'y';
            }
            elsif (   $name =~ / ^ PERL_ ( PRI | SCN ) | $sb FORMAT $sb /x
                    && $configs{$name}{pod} =~ m/ \b format \b /ix)
            {
                $configs{$name}{'section'} = $io_formats_scn;
            }
            elsif ($name =~ / BACKTRACE /x) {
                $configs{$name}{'section'} = $debugging_scn;
            }
            elsif ($name =~ / ALLOC $sb /x) {
                $configs{$name}{'section'} = $memory_scn;
            }
            elsif (   $name =~ /   STDIO | FCNTL | EOF | FFLUSH
                                | $sb FILE $sb
                                | $sb DIR $sb
                                | $sb LSEEK
                                | $sb INO $sb
                                | $sb OPEN
                                | $sb CLOSE
                                | ^ DIR
                                | ^ INO $sb
                                | DIR $
                                | FILENAMES
                                /x
                    || $configs{$name}{pod} =~ m!  I/O | stdio
                                                | file \s+ descriptor
                                                | file \s* system
                                                | statfs
                                                !x)
            {
                $configs{$name}{'section'} = $filesystem_scn;
            }
            elsif ($name =~ / ^ SIG | SIGINFO | signal /ix) {
                $configs{$name}{'section'} = $signals_scn;
            }
            elsif ($name =~ / $sb ( PROTO (?: TYPE)? S? ) $sb /x) {
                $configs{$name}{'section'} = $prototypes_scn;
            }
            elsif (   $name =~ / ^ LOC_ /x
                    || $configs{$name}{pod} =~ /full path/i)
            {
                $configs{$name}{'section'} = $paths_scn;
            }
            elsif ($name =~ / $sb LC_ | LOCALE | langinfo /xi) {
                $configs{$name}{'section'} = $locale_scn;
            }
            elsif ($configs{$name}{pod} =~ /  GCC | C99 | C\+\+ /xi) {
                $configs{$name}{'section'} = $compiler_scn;
            }
            elsif ($name =~ / PASSW (OR)? D | ^ PW | ( PW | GR ) ENT /x)
            {
                $configs{$name}{'section'} = $password_scn;
            }
            elsif ($name =~ /  SOCKET | $sb SOCK /x) {
                $configs{$name}{'section'} = $sockets_scn;
            }
            elsif (   $name =~ / THREAD | MULTIPLICITY /x
                    || $configs{$name}{pod} =~ m/ \b pthread /ix)
            {
                $configs{$name}{'section'} = $concurrency_scn;
            }
            elsif ($name =~ /  PERL | ^ PRIV | SITE | ARCH | BIN
                                | VENDOR | ^ USE
                            /x)
            {
                $configs{$name}{'section'} = $site_scn;
            }
            elsif (   $pod =~ / \b floating $dash_or_spaces point \b /ix
                   || $pod =~ / \b (double | single) $dash_or_spaces
                                precision \b /ix
                   || $pod =~ / \b doubles \b /ix
                   || $pod =~ / \b (?: a | the | long ) \s+
                                (?: double | NV ) \b /ix)
            {
                $configs{$name}{'section'} =
                                    $floating_scn;
            }
            else {
                # Above are the specific sections.  The rest go into a
                # grab-bag of general configuration values.  However, we put
                # two classes of them into lists of their names, without their
                # descriptions, when we think that the description doesn't add
                # any real value.  One list contains the #include variables:
                # the description is basically boiler plate for each of these.
                # The other list contains the very many things that are of the
                # form HAS_foo, and \bfoo\b is contained in its description,
                # and there is no verbatim text in the pod or links to/from it
                # (which would add value).  That means that it is likely the
                # intent of the variable can be gleaned from just its name,
                # and unlikely the description adds significant value, so just
                # listing them suffices.  Giving their descriptions would
                # expand this pod significantly with little added value.
                if (   ! $has_defn
                    && ! $configs{$name}{verbatim}
                    && ! $configs{$name}{linked})
                {
                    if ($name =~ / ^ I_ ( .* ) /x) {
                        push @include_defs, $name;
                        next;
                    }
                    elsif ($name =~ / ^ HAS_ ( .* ) /x) {
                        my $canonical_name = $1;
                        $canonical_name =~ s/_//g;

                        my $canonical_pod = $configs{$name}{pod};
                        $canonical_pod =~ s/_//g;

                        if ($canonical_pod =~ / \b $canonical_name \b /xi) {
                            if ($name =~ / $sb R $sb /x) {
                                push @has_r_defs, $name;
                            }
                            else {
                                push @has_defs, $name;
                            }
                            next;
                        }
                    }
                }

                $configs{$name}{'section'} = $genconfig_scn;
            }

            my $section = $configs{$name}{'section'};
            die "Internal error: '$section' not in \%valid_sections"
                            unless grep { $_ eq $section } keys %valid_sections;
            $flags .= 'AdmnT';
            $flags .= 'U' unless defined $configs{$name}{usage};
            my $data = check_and_add_proto_defn($name, $config_h,
                                     $configs{$name}{defn_line_num},
                                     $flags,
                                     "void",    # No return type
                                     [],
                                     CONDITIONAL_APIDOC
                                    );

            # All the information has been gathered; save it
            push $docs{$api}{$section}{$name}{items}->@*, $data;
            $docs{$api}{$section}{$name}{pod} = $configs{$name}{pod};
            $docs{$api}{$section}{$name}{usage}
                = $configs{$name}{usage} if defined $configs{$name}{usage};
        }
    }
}

sub format_pod_indexes ($entries_ref) {

    # Output the X<> references to the names, packed since they don't get
    # displayed, but not too many per line so that when someone is editing the
    # file, it doesn't run on
    return "" unless $entries_ref && $entries_ref->@*;

    my $text ="";
    my $line_length = 0;
    for my $name (sort dictionary_order $entries_ref->@*) {
        my $entry = "X<$name>";
        my $entry_length = length $entry;

        # Don't loop forever if we have a verrry long name, and don't go too
        # far to the right.
        if ($line_length > 0 && $line_length + $entry_length > $max_width) {
            $text .= "\n";
            $line_length = 0;
        }

        $text .= $entry;
        $line_length += $entry_length;
    }

    return $text;
}

    # output the docs for one function group
sub docout ($fh, $section_name, $element_name, $docref) {
    # Trim trailing space
    $element_name =~ s/\s*$//;

    my $pod = $docref->{pod} // "";
    my @items = $docref->{items}->@*;

    my $item0 = ${$items[0]};
    my $flags = $item0->{flags};

    if ($pod !~ /\S/) {
        warn "Empty pod for $element_name ("
           . where_from_string($item0->{file}, $item0->{line_num})
           . ')';
    }

    print $fh "\n=over $description_indent\n";
    print $fh "\n=item C<${$_}->{name}>\n" for @items;

    my @where_froms;
    my @deprecated;
    my @experimental;
    my @xrefs;

    for (my $i = 0; $i < @items; $i++) {
        last if $docref->{'xref_only'}; # Place holder

        my $item = ${$items[$i]};
        next if $item->{xref_only}; # Place holder
        my $name = $item->{name};

        if ($item->{flags}) {

            # If we're printing only a link to an element, this isn't the major
            # entry, so no X<> here.
            push @xrefs, $name unless $item->{flags} =~ /h/;

            push @deprecated,   "C<$name>" if $item->{flags} =~ /D/;
            push @experimental, "C<$name>" if $item->{flags} =~ /x/;
        }

        # While we're going though the items, construct a nice list of where
        # things are declared and documented.  Spend a bit of time to make it
        # easier on the humans who later read it.
        my $entry = "";
        if ($item->{proto_defined}) {
            $entry .= "declared "
                   .  where_from_string($item->{proto_defined}{file},
                                        $item->{proto_defined}{line_num});
        }

        if ($item->{docs_found}) {
            if ($i == 0) {
                # Special consolidated text if a single item in the list, and
                # it's declared and documented in the same place.
                if (   $entry
                    && @items == 1
                    && $item->{proto_defined}
                    && $item->{docs_found}
                    && $item->{proto_defined}{file} eq $item->{docs_found}{file}
                    && $item->{proto_defined}{line_num} eq
                                                $item->{docs_found}{line_num})
                {
                    $entry =~ s/declared/declared and documented/;
                }
                else {
                    $entry .= "; " if $entry;
                    $entry .= "all in group " if @items > 1;
                    $entry .= "documented "
                        .  where_from_string($item->{docs_found}{file},
                                             $item->{docs_found}{line_num});
                }
            }
        }

        # If there is a single item in the group, no need to give the name,
        # but capitalize the first word.
        if (@items == 1) {
            $entry = ucfirst($entry);
        }
        else {
            $entry = "$name $entry";
        }

        push @where_froms, $entry;
    }

    print $fh format_pod_indexes(\@xrefs);
    print $fh "\n" if @xrefs;

    for my $which (\@deprecated, \@experimental) {
        next unless $which->@*;

        my $is;
        my $it;
        my $list;

        if ($which->@* == 1) {
            $is = 'is';
            $it = 'it';
            $list = $which->[0];
        }
        elsif ($which->@* == @items) {
            $is = 'are';
            $it = 'them';
            $list = (@items == 2)
                        ? "both forms"
                        : "all these forms";
        }
        else {
            $is = 'are';
            $it = 'them';
            my $final = pop $which->@*;
            $list = "the " . join ", ", $which->@*;
            $list .= "," if $which->@* > 1;
            $list .= " and $final forms";
        }

        if ($which == \@deprecated) {
            print $fh <<~"EOT";

                C<B<DEPRECATED!>>  It is planned to remove $list
                from a future release of Perl.  Do not use $it for
                new code; remove $it from existing code.
                EOT
        }
        else {
            print $fh <<~"EOT";

                NOTE: $list $is B<experimental> and may change or be
                removed without notice.
                EOT
        }
    }

    chomp $pod;     # Make sure prints pod with a single trailing \n
    print $fh "\n", $pod, "\n";

    # Accumulate the usage section of the entry into this array.  Output below
    # only when non-empty
    my @usage;
    if (defined $docref->{usage}) {

        # A complete override of the usage section.  Note that the O flag
        # isn't checked for, as that usage is never output in this case
        push @usage, ($docref->{usage} =~ s/^/ /mrg), "\n";
    }
    else {
        my @outputs;    # The items actually to output, annotated

        # Look through all the items in this entry.  Find the longest of
        # certain fields, so that if multiple items are shown, they can be
        # nicely vertically aligned.
        my $max_name_len = 0;
        my $max_retlen = 0;
        my $any_has_pTHX_ = 0;

        # All functions (but not macros) have long names that begin with
        # 'Perl_'.  Many have short names as well, without the 'Perl_' prefix.
        # All legal styles are displayed.  The short name is first, then any
        # long one.  This is accomplished by redoing the loop with the long
        # name.  This variable controls if to do that.
        my $additional_long_form = 0;

        # Each group of elements is displayed nicely vertically aligned so
        # that any 'Perl_' prefix is outdented from the rest.  That is
        # accomplished by instead doing an extra indent on the short name.
        # A short name may hence occupy more space than its name indicates.
        # As we go through the names, we determine what the name column width
        # should be should this group need to have the outdented 'Perl_'.
        # This is done in parallel with the calculations using the variables
        # above.  At the end, one or the other set is selected.
        my $any_has_additional_long_form = 0;
        my $max_name_len_if_needs_extra_indent = 0;

        my $may_need_extra_indent = 1;
        for my $item_ref (@items) {
            my $item = $$item_ref;
            next if $item->{xref_only}; # Place holder

            my $name = $item->{name};
            my $flags = $item->{flags};

            if (! $additional_long_form && $flags =~ /O/) {
                my $real_proto = delete $protos{"perl_$name"};
                if (! $real_proto) {
                    warn "Unexpectedly there isn't a 'perl_$name' even though"
                       . " there is an 'O' flag "
                       . where_from_string($item->{file}, $item->{line_num})
                       . "; omitting the deprecation warning";
                }
                else {
                    print $fh "\nNOTE: the C<perl_$name()> form is",
                              " B<deprecated>.\n"
                }
            }

            die "'u' flag must also have 'm' or 'y' flags' for $name "
              . where_from_string($item->{proto_defined}{file},
                                  $item->{proto_defined}{line_num})
                                        if $flags =~ /u/ && $flags !~ /[my]/;

            my $has_semicolon = $flags =~ /;/;
            warn "'U' and ';' flags are incompatible"
               . where_from_string($item->{file}, $item->{line_num})
                                            if $flags =~ /U/ && $has_semicolon;

            # U means to not display the prototype; and there really isn't a
            # single good canonical signature for a typedef, so they aren't
            # displayed
            next if $flags =~ /[Uy]/;

            my $has_args = $flags !~ /n/;
            if (! $has_args) {
                warn "$name: n flag without m"
                   . where_from_string($item->{file}, $item->{line_num})
                                                        unless $flags =~ /m/;

                if ($item->{args} && $item->{args}->@*) {
                    warn "$name: n flag but apparently has args"
                       . where_from_string($item->{file}, $item->{line_num});
                    $flags =~ s/n//g;
                    $has_args = 1;
                }
            }

            my $ret = $item->{ret_type} // "";
            my @args;
            my $this_has_pTHX = 0;

            # If none of these exist, the prototype will be trivial, just
            # the name of the item, so don't display it.
            next unless $ret || $has_semicolon || $has_args;

            if ($has_args) {
                @args = $item->{args}->@*;

                # A thread context parameter is the default for functions (not
                # macros) unless the T flag is specified.  Those functions
                # that don't have long names (indicated by not having any of
                # the [IipS] flags have such an implicit parameter, that we
                # need to make explicit for the documentation.
                if ($item->{flags} !~ /[ IipS Mm T ]/xx)  {
                    $this_has_pTHX = 1;     # This 3 line design pattern is
                    unshift @args, "pTHX";  # repeated a few lines below
                    $any_has_pTHX_ = 1 if @args > 1;
                }

                # If only the Perl_foo form is to be displayed, change the
                # name of this item to be that.  This happens for either of
                # two reasons:
                #   1) The flags say we want "Perl_", but also to not create
                #      an entry in embed.h to #define a short name for it.
                my $needs_Perl_entry = (   $flags =~ /p/
                                        && $flags =~ /o/
                                        && $flags !~ /M/);

                #   2) The function takes a format string and a thread context
                #      parameter.  We can't cope with that because our macros
                #      expect both the thread context and the format to be the
                #      first parameter to the function; and only one can be in
                #      that position.
                my $cant_use_short_name = (   $flags =~ /f/
                                           && $flags !~ /T/
                                           && $name !~ /strftime/);

                # We also create a 'Perl_foo' entry if $additional_long_form
                # is set, as that explicitly indicates we want one
                if (   $additional_long_form
                    || $needs_Perl_entry
                    || $cant_use_short_name)
                {
                    # An all uppercase macro name gets an uppercase prefix.
                    my $perl = ($flags =~ /m/ && $name !~ /[[:lower:]]/)
                               ? "PERL_"
                               : "Perl_";
                    $name = "$perl$name";

                    # We can't hide the existence of any thread context
                    # parameter when using the "Perl_" long form.  So it must
                    # be the first parameter to the function.
                    if ($flags !~ /T/) {
                        $this_has_pTHX = 1;
                        unshift @args, "pTHX";
                        $any_has_pTHX_ = 1 if @args > 1;
                    }

                    $additional_long_form = 0;

                    # This will already be outdented
                    $may_need_extra_indent = 0;
                }
                elsif ($flags =~ /p/ && $flags !~ /o/) {

                    # Here, has a long name and we didn't create one just
                    # above.  Check that there really is a long name entry.
                    my $real_proto = delete $protos{"Perl_$name"};
                    if ($real_proto || $flags =~ /m/) {

                        # Set up to redo the loop at the end.  This iteration
                        # adds the short form; the redo causes its long form
                        # equivalent to be added too.
                        $additional_long_form = 1;
                        $any_has_additional_long_form = 1;
                    }
                    else {
                        warn "$name unexpectedly doesn't have a long name;"
                           . " only short name used\n("
                           . where_from_string($item->{file}, $item->{line_num})
                           . ')';
                    }

                    # Will need to indent this item to vertically align
                    $may_need_extra_indent = 1;
                }
                else {
                    # May need to indent this item to vertically align
                    $may_need_extra_indent = 1;
                }
            }

            my $retlen = length $ret;
            $max_retlen = $retlen if $retlen > $max_retlen;

            my $name_len = length $name;
            $max_name_len = $name_len if $name_len > $max_name_len;

            # length("Perl_") is 5
            if ($may_need_extra_indent) {
                $max_name_len_if_needs_extra_indent = $name_len + 5
                        if $name_len + 5 > $max_name_len_if_needs_extra_indent;
            }

            # Start creating this item's hash to guide its output
            push @outputs, {
                            ret => $ret, retlen => $retlen,
                            name => $name, name_len => $name_len,
                            has_pTHX => $this_has_pTHX,
                            may_need_extra_indent => $may_need_extra_indent,
                           };

            $outputs[-1]->{args}->@* = @args if $has_args;
            $outputs[-1]->{semicolon} = ";" if $has_semicolon;

            redo if $additional_long_form;
        }

        my $indent = 1; # Minimum space to get a verbatim block.

        # Above, we went through all the items in the group, discarding the
        # ones with trivial usage/prototype lines.  Now go through the
        # remaining ones, and add them to the list of output text.
        if (@outputs) {

            # We have available to us the remaining portion of the line after
            # subtracting all the indents this text is subject to.
            my $usage_max_width = $max_width
                                - $description_indent
                                - $usage_indent
                                - $indent;

            # Basically, there are three columns.  The first column is always
            # a blank to make this a verbatim block, and the return type
            # starts in the column after that.  The name column follows a
            # little to the right of the widest return type entry.
            my $name_column = $indent + $max_retlen + $usage_ret_name_sep_len;

            # And the arguments column follows immediately to the right of the
            # widest name entry.  Use the correct maximum calculated above for
            # the two cases of this group having or not having an entry that
            # requires 'Perl_' to be outdented.
            my $args_column = $name_column
                            + (($any_has_additional_long_form)
                               ? $max_name_len_if_needs_extra_indent
                               : $max_name_len);

            for my $element (@outputs) {

                # $running_length keeps track of which column we are currently
                # at.
                push @usage, " " x $indent;
                my $running_length = $indent;

                # Output the return type, followed by enough blanks to get us
                # to the beginning of the name
                push @usage, $element->{ret} if $element->{retlen};
                $running_length += $element->{retlen};
                push @usage, " " x ($name_column - $running_length);

                # If the group has a 'Perl_' long form entry, and this element
                # is not such a one, it will need to be indented past that
                # prefix.
                my $extra_indent = (   $any_has_additional_long_form
                                    && $element->{may_need_extra_indent})
                                   ? 5
                                   : 0;
                push @usage, " " x $extra_indent;

                # Then output the name
                push @usage, $element->{name};
                $running_length = $extra_indent
                                + $name_column
                                + $element->{name_len};

                # If there aren't any arguments, we are done, except for maybe
                # a semi-colon.
                if (! defined $element->{args}) {
                    push @usage, $element->{semicolon} // "";
                }
                else {

                    # Otherwise get to the first arguments column and output
                    # the left parenthesis
                    push @usage, " " x ($args_column - $running_length);
                    push @usage, "(";
                    $running_length = $args_column + 1;

                    # We know the final ending text.
                    my $tail = ")" . ($element->{semicolon} // "");

                    # Now ready to output the arguments.  It's quite possible
                    # that not all will fit on the remainder of the line, so
                    # will have to be wrapped onto subsequent line(s) with a
                    # hanging indent to make them into an aligned block.  It
                    # also does happen that one single argument can be so wide
                    # that it won't fit in the remainder of the line by
                    # itself.  In this case, we outdent the entire block by
                    # the excess width; this retains vertical alignment, like
                    # so:
                    #                   void  function_name(pTHX_ short1,
                    #                                    short2,
                    #                                    very_long_argument,
                    #                                    short3)
                    #
                    # First we have to find the width of the widest argument.
                    my $max_arg_len = 0;
                    for my $arg ($element->{args}->@*) {

                        # +1 because of attached comma or right paren
                        my $arg_len = 1 + length $arg;

                        $max_arg_len = $arg_len if $arg_len > $max_arg_len;
                    }

                    # Set the hanging indent to get to the '(' column.  All
                    # arguments but the first are output with a space
                    # separating them from the previous argument.  This is
                    # done even when not all arguments fit on the first line,
                    # so there is a second (etc.) line.  The first argument on
                    # those lines will have a leading space which causes those
                    # lines to automatically align to the next column after
                    # the '(', without us having to consider it further than
                    # the +1 in the excess width calculation
                    my $hanging_indent = $args_column;

                    # Continuation lines begin past the pTHX.  This makes all
                    # the "real" parameters be output in a vertically aligned
                    # block.
                    $hanging_indent += 6 if $any_has_pTHX_;

                    # See if there is an argument too wide to fit
                    my $excess_width = $hanging_indent
                                     + 1  # To space past the '('
                                     + $max_arg_len
                                     - $usage_max_width;

                    # Outdent if necessary
                    $hanging_indent -= $excess_width if $excess_width > 0;

                    if (     $any_has_pTHX_
                        &&   $element->{args}->@*
                        && ! $element->{has_pTHX}) {

                        # If this item has arguments but not a pTHX, but
                        # others do, indent the args for this one so that
                        # their block starts in the column as the ones with
                        # pTHX.  Typically, the arguments will be the same
                        # except for the pTHX, and this causes them to all
                        # line up, like so:
                        #
                        #  void  Perl_deb     (pTHX_ const char *pat, ...)
                        #  void  deb_nocontext(      const char *pat, ...)
                        #
                        # But, don't indent the full amount if any next lines
                        # would begin before that amount.  That leaves
                        # all lines indented to the same amount.  -1 to
                        # account for the left parenthesis
                        if ($running_length + 6 - 1 <= $hanging_indent) {
                            push @usage, " " x 6;
                            $running_length += 6;
                        }
                        elsif ($running_length < $hanging_indent) {
                            push @usage, (" " x (  1
                                                 + $hanging_indent
                                                 - $running_length));
                            $running_length = $hanging_indent;
                        }
                    }

                    # Go through the argument list.  Calculate how much space
                    # each takes, and start a new line if this won't fit on
                    # the current one.
                    for (my $i = 0; $i < $element->{args}->@*; $i++) {
                        my $arg = $element->{args}[$i];
                        my $is_final = $i == $element->{args}->@* - 1;

                        # +1 for the comma or right paren afterwards
                        my $this_length = 1 + length $arg;

                        # All but the first one have a blank separating them
                        # from the previous argument.
                        $this_length += 1 if $i != 0;

                        # With an extra +1 for the final one if needs a
                        # semicolon
                        $this_length += 1 if defined $element->{semicolon}
                                          && $is_final;

                        # If this argument doesn't fit on the line, start a
                        # new line, with the appropriate indentation.  Note
                        # that this value has been calculated above so that
                        # the argument will definitely fit on this new line.
                        if ($running_length + $this_length > $usage_max_width) {
                            push @usage, "\n", " " x $hanging_indent;
                            $running_length = $hanging_indent;
                        }

                        # Ready to output; first a blank separator for all but
                        # the first item
                        push @usage, " " if $i != 0;

                        push @usage, $arg;

                        # A comma-equivalent character for all but the final
                        # one indicates there is more to come; "pTHX" has an
                        # underscore, not a comma
                        if (! $is_final) {
                            push @usage, ($i == 0 && $element->{has_pTHX})
                                         ? "_"
                                         : ",";
                        }

                        $running_length += $this_length;
                    }

                    push @usage, $tail;
                }

                push @usage, "\n";
            }
        }
    }

    if (grep { /\S/ } @usage) {
        print $fh "\n=over $usage_indent\n\n";
        print $fh join "", @usage;
        print $fh "\n=back\n";
    }

    print $fh "\n=back\n";

    if (@where_froms) {
        print $fh "\n=for hackers\n";
        print $fh join "\n", @where_froms;
        print $fh "\n";
    }
}

sub construct_missings_section ($missings_hdr, $missings_ref) {
    my $text = "";

    $text .= "$missings_hdr\n" . format_pod_indexes($missings_ref);

    if ($missings_ref->@* == 0) {
        return $text . "\nThere are currently no items of this type\n";
    }

    # Sort the elements.
    my @missings = sort dictionary_order $missings_ref->@*;

    # Make a table of the missings in columns.  Give the subroutine a width
    # one less than you might expect, as we indent each line by one, to mark
    # it as verbatim.
    my $table .= columnarize_list(\@missings, $max_width - 1);
    $table =~ s/^/ /gm;

    return $text . "\n\n" . $table;
}

sub dictionary_order {
    # Do a case-insensitive dictionary sort, falling back in stages to using
    # everything for determinancy.  The initial comparison ignores
    # all non-word characters and non-trailing underscores and digits, with
    # trailing ones collating to after any other characters.  This collation
    # order continues in case tie breakers are needed; sequences of digits
    # that do get looked at always compare numerically.  The first tie
    # breaker takes all digits and underscores into account.  The next tie
    # breaker uses a caseless character-by-character comparison of everything
    # (including non-word characters).  Finally is a cased comparison.
    #
    # This gives intuitive results, but obviously could be tweaked.

    no warnings 'non_unicode';

    my $mod_string_for_dictionary_order = sub {
        my $string = shift;

        # Convert all digit sequences to be the same length with leading
        # zeros, so that, for example '8' will sort before '16' (using a fill
        # length value that should be longer than any sequence in the input).
        $string =~ s/(\d+)/sprintf "%06d", $1/ge;

        # Translate any underscores so they sort lowest.  This causes
        # 'word1_word2' to sort before 'word1word2' for all words.  And
        # translate any digits so they come after anything else.  This causes
        # digits to sort highest)
        $string =~ tr[_0-9]/\0\x{110000}-\x{110009}/;

        # Then move leading underscores to the end, translating them to above
        # everything else.  This causes '_word_' to compare just after 'word_'
        $string .= "\x{11000A}" x length $1 if $string =~ s/ ^ (\0+) //x;

        return $string;
    };

    # Modify \w, \W to reflect what the above sub does.
    state $w = "\\w\0\x{110000}-\x{11000A}";   # new \w string
    state $mod_w = qr/[$w]/;
    state $mod_W = qr/[^$w]/;

    local $a = $mod_string_for_dictionary_order->($a);
    local $b = $mod_string_for_dictionary_order->($b);

    # If the strings stripped of \W differ, use that as the comparison.
    my $cmp = lc ($a =~ s/$mod_W//gr) cmp lc ($b =~ s/$mod_W//gr);
    return $cmp if $cmp;

    # For the first tie breaker use a plain caseless comparison of the
    # modified strings
    $cmp = lc $a  cmp lc $b;
    return $cmp if $cmp;

    # Finally a straight comparison
    return $a cmp $b;
}

sub output ($destpod) {  # Output a complete pod file
    my $podname = $destpod->{podname};
    my $dochash = $destpod->{docs};

    # strip leading '|' from each line which had been used to hide pod from
    # pod checkers.
    s/^\|//gm for $destpod->{hdr}, $destpod->{footer};

    my $fh = open_new($podname, undef,
                      {by => "$0 extracting documentation",
                       from => 'the C source files'}, 1);

    print $fh $destpod->{hdr}, "\n";

    for my $section_name (sort dictionary_order keys %valid_sections) {
        my $section_info = $dochash->{$section_name};

        # We allow empty sections in perlintern.
        if (! $section_info && $podname eq $api) {
            warn "Empty section '$section_name' for $podname; skipped";
            next;
        }

        print $fh "\n=head1 $section_name\n";

        if ($section_info->{X_tags}) {
            print $fh format_pod_indexes([ keys $section_info->{X_tags}->%* ]);
            print $fh "\n";
            delete $section_info->{X_tags};
        }

        if ($podname eq $api) {
            print $fh "\n", $valid_sections{$section_name}{header}, "\n"
                 if defined $valid_sections{$section_name}{header};

            # Output any heading-level documentation and delete so won't get in
            # the way later
            if (exists $section_info->{""}) {
                print $fh "\n", $section_info->{""}, "\n";
                delete $section_info->{""};
            }
        }

        if (! $section_info || ! keys $section_info->%*) {
            my $pod_type = ($podname eq $api) ? "public" : "internal";
            print $fh "\nThere are currently no $pod_type API items in ",
                      $section_name, "\n";
        }
        else {

            # First go through the entries in this section, looking for ones
            # in each group that are out-of-order, and create place holder
            # entries for them.  Each will be output where they should go
            # according to dictionary order, and each points to where the
            # full documentation is found.  This allows closely related
            # entries whose names aren't alphabetically adjacent to be
            # combined into a single group and still be easily findable
            # by someone scanning through the pod alphabetically.
            #
            # Groups are arranged in the final output alphabetically by the
            # name of the first API element in the group.  (Within a group,
            # the ordering determined by the code is retained.)  That first
            # API element is considered its group leader.  The algorithm is to
            # look at the current group and the one just prior to it.  If
            # there are items in the current group that really sort
            # alphabetically to before the previous group, create placeholders
            # for them.  If there are items in the previous group that sort
            # after this group, create placeholders for them.  At the end of
            # comparing each group with its adjacent one, all out-of-order
            # items are found.

            my %out_of_orders;  # List of the elements that need placeholders

            my @leaders = sort dictionary_order keys %$section_info;

            # We need two groups to compare.  Set the first, and then compare
            # with the next.  At the end of each iteration, move on to
            # comparing the next two adjacent groups by moving the current
            # leader to be the previous one, and getting the next group and
            # setting it to be the current one.
            my $previous_leader = shift @leaders;
            for my $this_leader (@leaders) {

                # We look for both
                #   1) items in the second group that should come before the
                #      first group being examined;
                #   2) those in the first group that should come after the
                #      following adjacent group.
                # We use the same loop for both, setting up some arrays to
                # avoid conditionals cluttering up the logic.
                my @leaders = ( $previous_leader, $this_leader );
                my @groups = (
                                $section_info->{$leaders[0]},
                                $section_info->{$leaders[1]},
                             );

                # In the 0th loop iteration, we place the items in the second
                # group plus the group leader of the first, and sort
                # alphabetically.  The items that sort before the leader are
                # the items in the second group which should have placeholders
                # earlier in the output pointing to the second group.  We
                # splice the sorted array to include only the items before the
                # leader, as these are the out-of-order items.
                #
                # On the next loop iteration, we place the items in the first
                # group plus the group leader of the second, and sort
                # alphabetically.  The items that sort after the leader are
                # the items in the first group which should have placeholders
                # later in the output pointing to the first group.  We
                # splice the sorted array to include only the items after the
                # leader, as these are the out-of-order items.
                #
                # Below are the arguments to the splice command that gets
                # eval'd.  In the first iteration, we splice out the leader
                # and everything after it.  The splice command looks like
                #   splice $leader_index;
                # For the second iteration, we splice out the leader and
                # everything before it.  The splice command looks like
                #   splice 0, 1 + $leader_index;
                my @splice_low = ( "", "0, 1 + " );

                for my $which (0 .. 1) {
                    my $other = $which ^ 1;

                    # Create a list of items to compare, initialized with the
                    # other group's sub-items.  The group's leader is
                    # excluded.  It is always in position [0], so just shift
                    # it off.
                    my @items = map { $${_}->{name} }
                                            $groups[$other]->{items}->@*;
                    shift @items;

                    # Then add this group's leader.
                    push @items, $leaders[$which];

                    # Then sort the bunch.  The result is the other group's
                    # items sorted with respect to the leader of this group
                    @items = sort dictionary_order @items;

                    # Find where in the list this leader is.
                    my $previous_index;
                    for (my $i = 0; $i < @items; $i++) {
                        next unless $leaders[$which] eq $items[$i];

                        # Then splice the array, leaving only the out-of-order
                        # items
                        eval "splice \@items, $splice_low[$which]\$i ;";
                        goto spliced;
                    }

                    die "Unexpecedly \@items doesn't contain $leaders[$which]";

                 spliced:
                    # The array now includes the out-of-order items.  Save,
                    # along with which leader they should point to.
                    $out_of_orders{$_} = $leaders[$other] for @items;
                }

                $previous_leader = $this_leader;
            }

            # Add a link entry for each out-of-order item
            for my $item (keys %out_of_orders) {

                my $linkto = $out_of_orders{$item};

                # If the linked-to item merely points to another pod, just
                # point to that, skipping the intermediary.
                my $pod = ($section_info->{$linkto}{pod} =~ /^$described_in/)
                          ? $section_info->{$linkto}{pod}
                          : "Described under C<L</$linkto>>";

                # Create a bare-bones entry; 'xref-only' marks it as such
                $section_info->{$item} = {
                                           xref_only => 1,
                                           usage => "",
                                           pod => $pod,
                                           items => [
                                                      \{ name => "$item*",
                                                         xref_only => 1,
                                                       }
                                                    ],
                                         };
            }

            my $leader_name;
            my $leader_pod;

            # To make things more compact, go through the list again, and
            # combine adjacent elements that have identical pod.
            for my $next_name (sort dictionary_order keys %$section_info) {
                my $this_pod = $section_info->{$next_name}{pod};

                # Combine if are the same pod
                if ($leader_pod && $this_pod && $leader_pod eq $this_pod) {

                    # But the combining may cause a new placeholder entry to
                    # be put back into the same group as its original.  So
                    # check for that.
                    foreach my $item ($section_info->{$next_name}{items}->@*)
                    {
                        next if grep { $$item->{name}  eq $$_->{name} }
                                    $section_info->{$leader_name}{items}->@*;
                        push $section_info->{$leader_name}{items}->@*, $item;
                    }
                    delete $section_info->{$next_name};
                }
                else {  # Set new pod otherwise
                    $leader_pod = $this_pod;
                    $leader_name = $next_name;
                }
            }

            # Then, output everything.
            for my $this_leader (sort dictionary_order keys %$section_info) {
                docout($fh, $section_name,
                       $this_leader, $section_info->{$this_leader});
            }
        }

        print $fh "\n", $valid_sections{$section_name}{footer}, "\n"
                            if $podname eq $api
                            && defined $valid_sections{$section_name}{footer};
    }

    print $fh <<~EOT;

        =head1 $undocumented_scn

        EOT

    # The missings section has multiple subsections, described by an array.
    # The first two items in the array give first the name of the variable
    # containing the text for the heading for the first subsection to output,
    # and the second is a reference to a list of names of items in that
    # subsection.
    #
    # The next two items are for the next output subsection, and so forth.
    while ($destpod->{missings}->@*) {
        my $hdr_name = shift $destpod->{missings}->@*;
        my $hdr = $destpod->{$hdr_name};

        # strip leading '|' from each line which had been used to hide pod
        # from pod checkers.
        $hdr =~  s/^\|//gm;

        my $ref = shift $destpod->{missings}->@*;

        print $fh construct_missings_section($hdr, $ref);
    }

    print $fh "\n$destpod->{footer}\n=cut\n";

    read_only_bottom_close_and_rename($fh);
}

# Beginning of actual processing.  First process embed.fnc
foreach (@{(setup_embed())[0]}) {
    my $embed= $_->{embed}
        or next;
    my $file = $_->{source};
    my ($flags, $ret_type, $func, $args) =
                                 @{$embed}{qw(flags return_type name args)};
    check_and_add_proto_defn($func, $file,
                             # embed.fnc data doesn't currently furnish the
                             # line number
                             undef,

                             $flags, $ret_type, $args,

                             # This is like an 'apidoc_defn' line, in that it
                             # defines the prototype without furnishing any
                             # documentation
                             APIDOC_DEFN
                            );
}

# glob() picks up docs from extra .c or .h files that may be in unclean
# development trees, so use MANIFEST instead
my @headers;
my @non_headers;
open my $fh, '<', 'MANIFEST'
    or die "Can't open MANIFEST: $!";
while (my $input = <$fh>) {
    next unless my ($file) = $input =~ /^(\S+\.(?:[ch]|pod))\t/;

    # Don't pick up pods from these.
    next if $file =~ m! ^ ( cpan | dist | ext ) / !x
         && ! defined $extra_input_pods{$file};

    # Process these two special files immediately.  Otherwise we add the file to
    # the appropriate list.
    if ($file eq "proto.h") {

        # proto.h won't have any apidoc lines in it.  Instead look for real
        # prototypes.  Then we can check later that a prototype actually
        # exists when we add a line to the pod that claims there is.
        open my $fh, '<', $file or die "Cannot open $file for docs: $!\n";
        my $prev = "";
        while (defined (my $input = <$fh>)) {

            # Look for a prototype.  As an extra little nicety, make sure that
            # the line previous to the prototype is one of the ones that
            # declares the return type of the function.  This is to try to
            # eliminate false positives.
            if ($prev =~ / ^ \s*  PERL_ (?: CALLCONV
                                          | STATIC (?: _FORCE)? _INLINE
                                        )
                         /x)
            {
                $protos{$1} = $2
                            if $input =~ s/ ^ \s* ( [Pp]erl_\w* ) (.*) \n /$1/x;
            }
            $prev = $input;
        }
        close $fh or die "Error closing $file: $!\n";
    }
    elsif ($file eq "embed.h") {

        # embed.h won't have any apidoc lines in it.  Instead look for lines
        # that define the obsolete 'perl_' lines.  Then we can check later
        # that such a definition actually exists when we encounter input that
        # claims there is
        open my $fh, '<', $file or die "Cannot open $file for docs: $!\n";
        while (defined (my $input = <$fh>)) {
            $protos{$1} = $2
                if $input =~ / ^\# \s* define \s+ ( perl_\w+ ) ( [^)]* \) ) /x;
        }
        close $fh or die "Error closing $file: $!\n";
    }
    elsif ($file =~ /\.h/) {
        push @headers, $file;
    }
    else {
        push @non_headers, $file;
    }
}
close $fh or die "Error whilst reading MANIFEST: $!";

# Now have the files to examine saved.  Do the headers first to minimize the
# number of forward references that we would have to deal with later
for my $file (@headers, @non_headers) {
    open my $fh, '<', $file or die "Cannot open $file for docs: $!\n";
    autodoc($fh, $file);
    close $fh or die "Error closing $file: $!\n";
}

# Code in this file depends on doing config.h last.
parse_config_h();

# Here, we have parsed everything

# Any apidoc group whose leader element's documentation wasn't known by the
# time it was parsed has been placed in 'unknown' member of %docs.  We should
# now be able to figure out which real pod file to place it in, and its usage.
my $unknown = $docs{unknown};
foreach my $section_name (keys $unknown->%*) {
    foreach my $group_name (keys $unknown->{$section_name}->%*) {

        # The leader is always the 0th element in the 'items' array.
        my $leader_ref = $unknown->{$section_name}{$group_name}{items}[0];
        my $item_name = $$leader_ref->{name};

        # We should have a usage definition by now.
        my $corrected = $elements{$item_name};
        if (! defined $corrected) {
            die "=for apidoc line without any usage definition $item_name in"
              . " $unknown->{$section_name}{$group_name}{file}";
        }

        my $destpod = destination_pod($corrected->{flags});
        warn "The destination pod for $item_name remains unknown."
          . "  It should have been determined by now" if $destpod eq "unknown";

        # $destpod now gives the correct pod for this group.  Prepare to move it
        # to there
        die "$destpod unexpectedly has an entry in $section_name for "
          . $group_name if defined $docs{$destpod}{$section_name}{$group_name};

        $docs{$destpod}{$section_name}{$group_name} =
                                 delete $unknown->{$section_name}{$group_name};
    }
}

# For convenience of the typist (and it makes the code easier to read),
# 'apidoc_item' lines often have empty fields in the source code, signifying
# that those values are inherited from the plain 'apidoc' leader.
#
# Now that all the deferred elements have been resolved, we can go through and
# fill in those inherited values.
for my $which_pod (keys %docs) {
    for my $section (keys $docs{$which_pod}->%*) {
        for my $group (keys $docs{$which_pod}{$section}->%*) {
            next unless $docs{$which_pod}{$section}{$group}{items};

            my $leader;
            for my $element_ref ($docs{$which_pod}{$section}{$group}{items}->@*)
            {
                my $element = $$element_ref;
                # If this element is a plain 'apidoc', it is the leader, and
                # its data do not need to be adjusted.
                if ($element->{is_leader}) {
                    $leader = $element;
                    next;
                }
                next if $element->{proto_defined}
                     && $element->{proto_defined}{type} != APIDOC_ITEM;

                # Otherwise, it is from an 'apidoc_item' line.  If it is
                # lacking 'flags', copy the leader's into it; otherwise add
                # the important ones.  We don't add flags that would change
                # how this item is displayed.
                if ($element->{flags}) {
                    $element->{flags} .=
                                       $leader->{flags} =~ s/$item_flags_re//r;
                }
                else {
                    $element->{flags} = $leader->{flags};
                }

                $element->{ret_type} = $leader->{ret_type}
                                                    unless $element->{ret_type};

                if ($element->{flags}) {
                    if (      $element->{flags} !~ /n/
                        &&    $leader->{args}
                        && (! $element->{args} || $element->{args}->@* == 0)
                    ) {
                        push $element->{args}->@*, $leader->{args}->@*;
                    }
                }
            }
        }
    }
}

# Here %docs is populated; and we're ready to output

my %api    = ( podname => $api, docs => $docs{$api} );
my %intern = ( podname => $intern, docs => $docs{$intern} );

# But first, look for inconsistencies and populate the lists of elements whose
# documentation is missing

for my $which (\%api, \%intern) {
    my (@deprecated, @experimental, @missings);
    for my $name (sort dictionary_order keys %elements) {
        my $element = $elements{$name};

        next if $which == \%api && $element->{flags} !~ /A/;
        next if $which == \%intern && $element->{flags} =~ /A/;

        if ($element->{docs_found}) {
            warn "'$name' missing 'd' flag"
               . where_from_string($element->{file}, $element->{line_num})
                                               if ! $element->{docs_expected};
        }
        elsif ($element->{docs_expected}) { # But no docs found
            warn "No documentation was found for $name, even though "
               . where_from_string($element->{file}, $element->{line_num})
               . " says there should be some available"
        }
        else {  # No documentation found, nor expected.  Is a problem only if
                # the flags indicate it is public
            if (   ($which == \%api && $element->{flags} =~ /A/)
                || ($which == \%intern && $element->{flags} !~ /[AS]/)
               ) {
                if ($element->{flags} =~ /D/) {
                    push @deprecated, $name;
                }
                elsif ($element->{flags} =~ /x/) {
                    push @experimental, $name;
                }
                else {
                    push @missings, $name;
                }
            }
        }
    }

    $which->{missings}->@* = (
                               missings_hdr     => \@missings,
                               experimental_hdr => \@experimental,
                               deprecated_hdr   => \@deprecated,
                             );
}

my @other_places = ( qw(perlclib ), keys %described_elsewhere );
my $places_other_than_intern = join ", ",
            map { "L<$_>" } sort dictionary_order 'perlapi', @other_places;
my $places_other_than_api = join ", ",
            map { "L<$_>" } sort dictionary_order 'perlintern', @other_places;

# The S< > makes things less densely packed, hence more readable
my $has_defs_text .= join ",S< > ", map { "C<$_>" }
                                             sort dictionary_order @has_defs;
my $has_r_defs_text .= join ",S< > ", map { "C<$_>" }
                                             sort dictionary_order @has_r_defs;
$valid_sections{$genconfig_scn}{footer} =~ s/__HAS_LIST__/$has_defs_text/;
$valid_sections{$genconfig_scn}{footer} =~ s/__HAS_R_LIST__/$has_r_defs_text/;

my $include_defs_text .= join ",S< > ", map { "C<$_>" }
                                            sort dictionary_order @include_defs;
$valid_sections{$genconfig_scn}{footer}
                                      =~ s/__INCLUDE_LIST__/$include_defs_text/;

my $section_list = join "\n\n", map { "=item L</$_>" }
                                sort(dictionary_order keys %valid_sections),
                                $undocumented_scn;  # Keep last

# Leading '|' is to hide these lines from pod checkers.  khw is unsure if this
# is still needed.
$api{hdr} = <<"_EOB_";
|=encoding UTF-8
|
|=head1 NAME
|
|perlapi - autogenerated documentation for the perl public API
|
|=head1 DESCRIPTION
|X<Perl API> X<API> X<api>
|
|This file contains most of the documentation of the perl public API, as
|generated by F<embed.pl>.  Specifically, it is a listing of functions,
|macros, flags, and variables that may be used by extension writers.  Besides
|L<perlintern> and F<$config_h>, some items are listed here as being actually
|documented in another pod.
|
|L<At the end|/$undocumented_scn> is a list of functions which have yet
|to be documented.  Patches welcome!  The interfaces of these are subject to
|change without notice.
|
|Some of the functions documented here are consolidated so that a single entry
|serves for multiple functions which all do basically the same thing, but have
|some slight differences.  For example, one form might process magic, while
|another doesn't.  The name of each variation is listed at the top of the
|single entry.
|
|The names of all API functions begin with the prefix C<Perl_> so as to
|prevent any name collisions with your code.  But, unless
|C<-Accflags=-DPERL_NO_SHORT_NAMES> has been specified in compiling your code
|(see L<perlembed/Hiding Perl_>), synonymous macros are also available to you
|that don't have this prefix, and also hide from you the need (or not) to have
|a thread context parameter passed to the function.  Generally, code is easier
|to write and to read when the short form is used, so in practice that
|compilation flag is not used.  Not all functions have the short form; both
|are listed here when available.
|
|Anything not listed here or in the other mentioned pods is not part of the
|public API, and should not be used by extension writers at all.  For these
|reasons, blindly using functions listed in F<proto.h> is to be avoided when
|writing extensions.
|
|In Perl, unlike C, a string of characters may generally contain embedded
|C<NUL> characters.  Sometimes in the documentation a Perl string is referred
|to as a "buffer" to distinguish it from a C string, but sometimes they are
|both just referred to as strings.
|
|Note that all Perl API global variables must be referenced with the C<PL_>
|prefix.  Again, those not listed here are not to be used by extension writers,
|and may be changed or removed without notice; same with macros.
|Some macros are provided for compatibility with the older,
|unadorned names, but this support may be disabled in a future release.
|
|Perl was originally written to handle US-ASCII only (that is characters
|whose ordinal numbers are in the range 0 - 127).
|And documentation and comments may still use the term ASCII, when
|sometimes in fact the entire range from 0 - 255 is meant.
|
|The non-ASCII characters below 256 can have various meanings, depending on
|various things.  (See, most notably, L<perllocale>.)  But usually the whole
|range can be referred to as ISO-8859-1.  Often, the term "Latin-1" (or
|"Latin1") is used as an equivalent for ISO-8859-1.  But some people treat
|"Latin1" as referring just to the characters in the range 128 through 255, or
|sometimes from 160 through 255.
|This documentation uses "Latin1" and "Latin-1" to refer to all 256 characters.
|
|Note that Perl can be compiled and run under either ASCII or EBCDIC (See
|L<perlebcdic>).  Most of the documentation (and even comments in the code)
|ignore the EBCDIC possibility.
|For almost all purposes the differences are transparent.
|As an example, under EBCDIC,
|instead of UTF-8, UTF-EBCDIC is used to encode Unicode strings, and so
|whenever this documentation refers to C<utf8>
|(and variants of that name, including in function names),
|it also (essentially transparently) means C<UTF-EBCDIC>.
|But the ordinals of characters differ between ASCII, EBCDIC, and
|the UTF- encodings, and a string encoded in UTF-EBCDIC may occupy a different
|number of bytes than in UTF-8.
|
|The organization of this document is tentative and subject to change.
|Suggestions and patches welcome
|L<perl5-porters\@perl.org|mailto:perl5-porters\@perl.org>.
|
|The API elements are grouped by functionality into sections, as follows.
|Within sections the elements are ordered alphabetically, ignoring case, with
|non-leading underscores sorted first, and leading underscores and digits
|sorted last.
|
|=over $standard_indent

|$section_list
|
|=back
|
|The listing below is alphabetical, case insensitive.
_EOB_

$api{footer} = <<"_EOE_";
|=head1 AUTHORS
|
|Until May 1997, this document was maintained by Jeff Okamoto
|<okamoto\@corp.hp.com>.  It is now maintained as part of Perl itself.
|
|With lots of help and suggestions from Dean Roehrich, Malcolm Beattie,
|Andreas Koenig, Paul Hudson, Ilya Zakharevich, Paul Marquess, Neil
|Bowers, Matthew Green, Tim Bunce, Spider Boardman, Ulrich Pfeifer,
|Stephen McCamant, and Gurusamy Sarathy.
|
|API Listing originally by Dean Roehrich <roehrich\@cray.com>.
|
|Updated to be autogenerated from comments in the source by Benjamin Stuhl.
|
|=head1 SEE ALSO
|
|F<$config_h>, $places_other_than_api
_EOE_

$api{missings_hdr} = <<'_EOT_';
|The following functions have been flagged as part of the public
|API, but are currently undocumented.  Use them at your own risk,
|as the interfaces are subject to change.  Functions that are not
|listed in this document are not intended for public use, and
|should NOT be used under any circumstances.
|
|If you feel you need to use one of these functions, first send
|email to L<perl5-porters@perl.org|mailto:perl5-porters@perl.org>.
|It may be that there is a good reason for the function not being
|documented, and it should be removed from this list; or it may
|just be that no one has gotten around to documenting it.  In the
|latter case, you will be asked to submit a patch to document the
|function.  Once your patch is accepted, it will indicate that the
|interface is stable (unless it is explicitly marked otherwise) and
|usable by you.
_EOT_

$api{experimental_hdr} = <<"_EOT_";
|
|Next are the API-flagged elements that are considered experimental.  Using one
|of these is even more risky than plain undocumented ones.  They are listed
|here because they should be listed somewhere (so their existence doesn't get
|lost) and this is the best place for them.
_EOT_

$api{deprecated_hdr} = <<"_EOT_";
|
|Finally are deprecated undocumented API elements.
|Do not use any for new code; remove all occurrences of all of these from
|existing code.
_EOT_
output(\%api);

$intern{hdr} = <<"_EOB_";
|=head1 NAME
|
|perlintern - autogenerated documentation of purely B<internal>
|Perl functions
|
|=head1 DESCRIPTION
|X<internal Perl functions> X<interpreter functions>
|
|This file is the autogenerated documentation of functions in the
|Perl interpreter that are documented using Perl's internal documentation
|format but are not marked as part of the Perl API.  In other words,
|B<they are not for use in extensions>!

|It has the same sections as L<perlapi>, though some may be empty.
|
_EOB_

$intern{footer} = <<"_EOE_";
|
|=head1 AUTHORS
|
|The autodocumentation system was originally added to the Perl core by
|Benjamin Stuhl.  Documentation is by whoever was kind enough to
|document their functions.
|
|=head1 SEE ALSO
|
|F<$config_h>, $places_other_than_intern
_EOE_

$intern{missings_hdr} = <<"_EOT_";
|
|This section lists the elements that are otherwise undocumented.  If you use
|any of them, please consider creating and submitting documentation for it.
|
|Experimental and deprecated undocumented elements are listed separately at the
|end.
|
_EOT_

$intern{experimental_hdr} = <<"_EOT_";
|
|Next are the experimental undocumented elements
|
_EOT_

$intern{deprecated_hdr} = <<"_EOT_";
|
|Finally are the deprecated undocumented elements.
|Do not use any for new code; remove all occurrences of all of these from
|existing code.
|
_EOT_

output(\%intern);
