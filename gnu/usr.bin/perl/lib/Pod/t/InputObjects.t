#!perl -Tw

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
}

use Test::More;

plan tests => 33;

use_ok( 'Pod::InputObjects' );


{ # test package Pod::InputSource
    local *FH;
    my $p_is = Pod::InputSource->new( -handle => \*FH );

    isa_ok( $p_is, 'Pod::InputSource', 'Pod::InputSource constructor' );

    is( $p_is->name, '(unknown)', 'Pod::InputSource->name()' );
    is( $p_is->name( 'test' ), 'test', 'set Pod::InputSource->name( test )' );
    is( $p_is->filename, 'test', 'Pod::InputSource->filename() alias' );

    is( $p_is->handle, \*FH, 'Pod::InputSource->handle()' );

    is( $p_is->was_cutting(), 0, 'Pod::InputSource->was_cutting()' );
    is( $p_is->was_cutting( 1 ), 1, 'set Pod::InputSource->was_cutting( 1 )' );
}

{ # test package Pod::Paragraph
    my $p_p1 = Pod::Paragraph->new( -text => 'NAME', -name => 'head2' );
    my $p_p2 = Pod::Paragraph->new( 'test - This is the test suite' );
    isa_ok( $p_p1, 'Pod::Paragraph', 'Pod::Paragraph constuctor' );
    isa_ok( $p_p2, 'Pod::Paragraph', 'Pod::Paragraph constructor revisited' );

    is( $p_p1->cmd_name(), 'head2', 'Pod::Paragraph->cmd_name()' );
    is( $p_p1->cmd_name( 'head1' ), 'head1', 
        'Pod::Paragraph->cmd_name( head1 )' );
    ok( !$p_p2->cmd_name(),
        'Pod::Paragraph->cmd_name() revisited' );

    is( $p_p1->text(), 'NAME', 'Pod::Paragraph->text()' );
    is( $p_p2->text(), 'test - This is the test suite', 
        'Pod::Paragraph->text() revisited' );
    my $new_text = 'test - This is the test suite.';
    is( $p_p2->text( $new_text ), $new_text, 
        'Pod::Paragraph->text( ... )' );
    
    is( $p_p1->raw_text, '=head1 NAME', 
        'Pod::Paragraph->raw_text()' );
    is( $p_p2->raw_text, $new_text, 
        'Pod::Paragraph->raw_text() revisited' );
    
    is( $p_p1->cmd_prefix, '=', 
        'Pod::Paragraph->cmd_prefix()' );
    is( $p_p1->cmd_separator, ' ', 
        'Pod::Paragraph->cmd_separator()' );

    # Pod::Parser->parse_tree() / ptree()
    
    is( $p_p1->file_line(), '<unknown-file>:0', 
        'Pod::Paragraph->file_line()' );
    $p_p2->{ '-file' } = 'test'; $p_p2->{ '-line' } = 3;
    is( $p_p2->file_line(), 'test:3', 
        'Pod::Paragraph->file_line()' );
}

{ # test package Pod::InteriorSequence

    my $p_pt = Pod::ParseTree->new();
    my $pre_txt = 'test - This is the ';
    my $cmd_txt = 'test suite';
    my $pst_txt ='.';
	$p_pt->append( $cmd_txt );

    my $p_is = Pod::InteriorSequence->new( 
        -name => 'I', -ldelim => '<', -rdelim => '>',
        -ptree => $p_pt
    );
    isa_ok( $p_is, 'Pod::InteriorSequence', 'P::InteriorSequence constructor' );
	
    is( $p_is->cmd_name(), 'I', 'Pod::InteriorSequence->cmd_name()' );
    is( $p_is->cmd_name( 'B' ), 'B', 
        'set Pod::InteriorSequence->cmd_name( B )' );

    is( $p_is->raw_text(), "B<$cmd_txt>", 
        'Pod::InteriorSequence->raw_text()' );

    $p_is->prepend( $pre_txt );
    is( $p_is->raw_text(), "B<$pre_txt$cmd_txt>", 
        'raw_text() after prepend()' );

    $p_is->append( $pst_txt );
    is( $p_is->raw_text(), "B<$pre_txt$cmd_txt$pst_txt>",
        'raw_text() after append()' );    
}

{ # test package Pod::ParseTree
    my $p_pt1 = Pod::ParseTree->new();
    my $p_pt2 = Pod::ParseTree->new();
    isa_ok( $p_pt1, 'Pod::ParseTree', 
            'Pod::ParseTree constructor' );
    
    is( $p_pt1->top(), $p_pt1, 'Pod::ParseTree->top()' );
    is( $p_pt1->top( $p_pt1, $p_pt2 ), $p_pt1, 
        'set new Pod::ParseTree->top()' );

    ok( eq_array( [ $p_pt1->children() ], [ $p_pt1, $p_pt2] ),
        'Pod::ParseTree->children()' );

	my $text = 'This is the test suite.';
	$p_pt2->append( $text );
	is( $p_pt2->raw_text(), $text, 'Pod::ParseTree->append()' );
}

__END__

=head1 NAME

InputObjects.t - The tests for Pod::InputObjects

=head AUTHOR

20011220 Abe Timmerman <abe@ztreet.demon.nl>

=cut
