#!/usr/bin/perl
#
#
# Atsushi Kobayashi <akoba@nttv6.net>
#
# Acknowledgments
# This module was supported by the Ministry of Internal Affairs and 
# Communications of Japan.
#
# Flow.pm - 2008/12/04
#
# Copyright (c) 2007-2008 NTT Information Sharing Platform Laboratories
#
# This package is free software and is provided "as is" without express
# or implied warranty.  It may be used, redistributed and/or modified
# under the terms of the Perl Artistic License (see
# http://www.perl.com/perl/misc/Artistic.html)
#

package Net::Flow;

use 5.008008;
use strict;
use warnings;

use Exporter;

our @EXPORT_OK = qw(decode encode);
our $VERSION = '0.04';

use constant NetFlowv5                        => 5 ;
use constant NetFlowv8                        => 8 ;
use constant NetFlowv9                        => 9 ;
use constant IPFIX                            => 10 ;
 
use constant NFWV9_DataTemplateSetId          => 0 ;
use constant NFWV9_OptionTemplateSetId        => 1 ;

use constant IPFIX_DataTemplateSetId          => 2 ;
use constant IPFIX_OptionTemplateSetId        => 3 ;

use constant MinDataSetId                     => 256 ;
use constant VariableLength                   => 65535 ;
use constant ShortVariableLength              => 255 ;

my %TemplateForNetFlowv5 = (
		    'SetId'     => 0,
		    'TemplateId'    => 0,
		    'FieldCount' => 20,
		    'Template' => [
				   { 'Length'=>4,'Id'=>8  }, # SRC_ADDR
				   { 'Length'=>4,'Id'=>12 }, # DST_ADDR
				   { 'Length'=>4,'Id'=>15 }, # NEXT-HOP
				   { 'Length'=>2,'Id'=>10 }, # INPUT
				   { 'Length'=>2,'Id'=>14 }, # OUTPUT
				   { 'Length'=>4,'Id'=>2  }, # PKTS
				   { 'Length'=>4,'Id'=>1  }, # BYTES
				   { 'Length'=>4,'Id'=>22 }, # FIRST
				   { 'Length'=>4,'Id'=>21 }, # LAST
				   { 'Length'=>2,'Id'=>7  }, # SRC_PORT 
				   { 'Length'=>2,'Id'=>11 }, # DST_PORT
				   { 'Length'=>1,'Id'=>0  }, # PADDING
				   { 'Length'=>1,'Id'=>6  }, # FLAGS
				   { 'Length'=>1,'Id'=>4  }, # PROT
				   { 'Length'=>1,'Id'=>5  }, # TOS
				   { 'Length'=>2,'Id'=>16 }, # SRC_AS
				   { 'Length'=>2,'Id'=>17 }, # DST_AS
				   { 'Length'=>1,'Id'=>9  }, # SRC_MASK
				   { 'Length'=>1,'Id'=>13 }, # DST_MASK
				   { 'Length'=>2,'Id'=>0  }  # PADDING
				  ],
		   ) ;

#################### START sub encode() ####################
sub encode {

  my ( $InputHeaderRef, $InputTemplateRef, $InputFlowRef, $MaxDatagram ) = @_;
  my @Payloads = () ;
  my @FlowPacks = () ;
  my %FlowSetPayloads = () ;
  my %FlowSetLength = () ;
  my $FlowCount = 0 ;
  my $DataCount = 0 ;
  my $HeaderLength = undef ;
  my $FlowSetHeaderLength = 4 ;
  my @Errors = ();
  my $Error = undef;

  #
  # check header reference
  # 

  my ( $HeaderRef, $ErrorRef ) =
      &check_header($InputHeaderRef) ;
  
  push( @Errors,@{$ErrorRef} ) 
	if( defined $ErrorRef ) ;

  if( $HeaderRef->{VersionNum} == IPFIX ){

      $HeaderLength = 16 ;

  }elsif( $HeaderRef->{VersionNum} == NetFlowv9 ){

      $HeaderLength = 20 ;

  }

  foreach my $FlowRef ( @{$InputTemplateRef}, @{$InputFlowRef} ){
      my $PackRef = undef ;
      my $ErrorRef = undef ;
      my $DecodeTemplateRef = undef ;

      unless( defined $FlowRef->{SetId} ){
	      $Error = "ERROR : NOTHING SETID VALUE" ;
	      push( @Errors, $Error ) ;
	      next ;
      }

      #
      # encode flow data
      # 

      if( $FlowRef->{SetId} >= MinDataSetId ){

	  # 
      	  # searching for particular template
      	  # 

          ( $DecodeTemplateRef,$Error ) =
	      &search_template(
			       $FlowRef->{SetId},
                               $InputTemplateRef
                               ) ;

	  if( defined $DecodeTemplateRef ){

	      ($PackRef,$ErrorRef) = 
	      	&flow_encode(
			$FlowRef,
			$DecodeTemplateRef
			) ;

	  }else{

	      $Error = "ERROR : NO TEMPLATE TEMPLATE ID=$FlowRef->{SetId}" ;
	      push( @Errors, $Error ) ;

	  }

      #
      # encode template data 
      # 

      }else{

	  ($PackRef,$ErrorRef) =
	      &template_encode(
		$FlowRef,
		$HeaderRef
		) ;

      }

      push(@FlowPacks, $PackRef) 
	if defined $PackRef ;

      push(@Errors, @{$ErrorRef}) 
	if defined $ErrorRef ;

  }

  if( $#FlowPacks < 0 ){

	$Error = "ERROR : NO FLOW DATA" ;
	push( @Errors,$Error ) ;
	return (
		$HeaderRef,
		\@Payloads,
		\@Errors
		);

  }

  #
  # encode NetFlowv9/IPFIX datagram 
  # 

  foreach my $FlowPackRef ( @FlowPacks ){
      
      next unless( defined $FlowPackRef->{Pack} );
      
      #
      # check datagram size
      #
      
      my $TotalLength = $HeaderLength ;
      
      foreach my $SetId ( keys %FlowSetLength ){

	  $TotalLength += 
	      $FlowSetLength{$SetId}+$FlowSetHeaderLength+4 ;

      }

      if( (length($FlowPackRef->{Pack})+$TotalLength) > $MaxDatagram ){

	  if( $FlowCount > 0 ){

	    #
	    # make NetFlow/IPFIX datagram
	    #

	    push( 
		 @Payloads,
		 &datagram_encode(
				  $HeaderRef,
				  \%FlowSetPayloads,
				  \$FlowCount,
				  $#Payloads+1,
				  \$DataCount
				 )
		) ;

	  }else{

	      $Error = "ERROR : TOO SHORT MAX DATA" ;
	      push( @Errors,$Error ) ;
	      return (
		      $HeaderRef,
		      \@Payloads,
		      \@Errors
		      );

	  }

	  %FlowSetPayloads = () ;
	  %FlowSetLength = () ;
	  $FlowCount = 0 ;
	  $DataCount = 0 ;
	  
      }

      $FlowSetLength{$FlowPackRef->{SetId}} += 
	  length($FlowPackRef->{Pack}) ;

      $FlowSetPayloads{$FlowPackRef->{SetId}} .= 
	  $FlowPackRef->{Pack} ;

      $DataCount += 1 
	if $FlowPackRef->{SetId} >= MinDataSetId ;

      $FlowCount += 1 ;

  }

  if( $FlowCount > 0 ){

    push( 
	 @Payloads,
	 &datagram_encode(
			  $HeaderRef,
			  \%FlowSetPayloads,
			  \$FlowCount,
			  $#Payloads+1,
			  \$DataCount
			 )
	) ;
    
  }

  return (
	  $HeaderRef,
	  \@Payloads,
	  \@Errors
	  );

}
#################### END sub encode() ######################

#################### START sub check_header() ##############

sub check_header{
    my ($InputHeaderRef) = @_ ;
    my %Header    = () ;
    my @Errors    = () ;
    my $Error     = undef ;
    my @Fields = ( "SysUpTime","UnixSecs","SequenceNum","SourceId"  ) ;

    if( defined( $InputHeaderRef->{VersionNum} ) ){

	if( $InputHeaderRef->{VersionNum} == IPFIX ){

	    $Header{VersionNum} = IPFIX ;
	    @Fields = ( "UnixSecs","SequenceNum","ObservationDomainId"  ) ;

	}elsif( $InputHeaderRef->{VersionNum} == NetFlowv9 ){ 

	    $Header{VersionNum} = NetFlowv9 ;

	}else{

	    $Error = "WARNING : NO SUPPORT HEADER VERSION NUMBER $InputHeaderRef->{VersionNum}" ;
	    push(@Errors,$Error) ;
	    $Header{VersionNum} = NetFlowv9 ;

	}

    }else{

	$Error = "WARNING : NO HEADER VERSION NUMBER" ;
	push(@Errors,$Error) ;
	$Header{VersionNum} = NetFlowv9 ;

    }

    foreach my $Field ( @Fields ){

	if(defined $InputHeaderRef->{$Field}){

	    $Header{$Field} = $InputHeaderRef->{$Field} ;

	}else{

	    #
	    # setting default data
	    #

	    $Error = "WARNING : NO HEADER $Field" ;
	    push(@Errors,$Error) ;
	    $Header{$Field} = 0 ;

	}

    }

    return(
	   \%Header,
	   \@Errors
	   ) ;

}
#################### END sub check_header() ################

#################### START sub datagram_encode() ###########
sub datagram_encode{
    my ($HeaderRef,$FlowSetPayloadRef,$FlowCountRef,$PayCount,$DataCountRef) = @_ ;
    my $Payload = undef ;
    my %Padding = () ;

    #
    # encode flow set data
    #

    foreach my $SetId ( sort { $a <=> $b } ( keys %{$FlowSetPayloadRef} ) ){

	#
	# make padding part
	#

	$Padding{$SetId} = "" ;

	while( (length($FlowSetPayloadRef->{$SetId})
		+length($Padding{$SetId}))%4 != 0 ){

	    $Padding{$SetId} .= pack( "c",  0  ) ;

	}

	$Payload .= 
	    pack( 
		  "nn",
		  $SetId,
		  (length($FlowSetPayloadRef->{$SetId})
			+length($Padding{$SetId})+4)
		  ). 
	    $FlowSetPayloadRef->{$SetId}. 
	    $Padding{$SetId} ;

    }


    if( $HeaderRef->{VersionNum} == NetFlowv9 ){

	$HeaderRef->{SequenceNum} += $PayCount ;
	$HeaderRef->{Count}        = $$FlowCountRef ;

	$Payload = 
	    pack(
		 "nnNNNN",
		 $HeaderRef->{VersionNum},
		 $HeaderRef->{Count},
		 $HeaderRef->{SysUpTime},
		 $HeaderRef->{UnixSecs},
		 $HeaderRef->{SequenceNum},
		 $HeaderRef->{SourceId}
		 ) . $Payload ;
    
    }elsif( $HeaderRef->{VersionNum} == IPFIX ){

	$HeaderRef->{SequenceNum} += $$DataCountRef ;

	$Payload = 
	    pack(
		 "nnNNN",
		 $HeaderRef->{VersionNum},,
		 (length($Payload)+16),
		 $HeaderRef->{UnixSecs},
		 $HeaderRef->{SequenceNum},
		 $HeaderRef->{ObservationDomainId}
		 ) . $Payload ;

    }

    return( 
	    \$Payload
	    ) ;

}
#################### END sub datagram_encode() #############

#################### START sub flow_encode() ###############
sub flow_encode{
    my ($FlowRef, $DecodeTemplateRef) = @_ ;
    my %FlowData = () ; 
    my @Errors = () ;
    my $Error = undef ;
    my %Count = () ;

    $FlowData{SetId} = $DecodeTemplateRef->{TemplateId} ;

    foreach my $TemplateArrayRef ( @{$DecodeTemplateRef->{Template}} ){

	my $FlowValue = undef ;
	
	$Count{$TemplateArrayRef->{Id}} = 0 
	    unless defined $Count{$TemplateArrayRef->{Id}} ;


	if( defined $FlowRef->{$TemplateArrayRef->{Id}} ){

	    #
	    # One Template has multiple same Ids. 
	    #

	    if( ref $FlowRef->{$TemplateArrayRef->{Id}} ){

		$FlowValue = 
		  @{$FlowRef->{$TemplateArrayRef->{Id}}}[$Count{$TemplateArrayRef->{Id}}] ;

	    #
	    # Each Id is different than others. 
	    # 

	    }else{
		
		$FlowValue = $FlowRef->{$TemplateArrayRef->{Id}} ;

	    }

	    #
	    # Variable Length Type
	    # 
		
	    if( $TemplateArrayRef->{Length} == VariableLength ){

		my $Length = length( $FlowValue ) ;
		
		#
		# Value Length  < 255
		#
 
		if( $Length < ShortVariableLength ){
		    
		    $FlowData{Pack}  .= 
			pack("C A$Length", 
			     $Length, 
			     $FlowValue ) ;

		    #
		    # Value Length > 255
		    #
			
		}else{
		    
		    $FlowData{Pack}  .= 
			pack("C n A$Length", 
			     ShortVariableLength, 
			     $Length, 
			     $FlowValue ) ;

		}

	    #
	    # Fixed Length Type
	    # 
		
	    }else{

		$FlowData{Pack}  .= 
		    pack( "A$TemplateArrayRef->{Length}", 
			  $FlowValue ) ;
		
	    }


	}else{

	    $Error = "WARNING : NOT FIELD DATA INFORMATION ELEMENT ID=$TemplateArrayRef->{Id}" ;
	    push( @Errors,$Error ) ;
	    
	    if( $TemplateArrayRef->{Length} == VariableLength ){

		$FlowData{Pack}  .=
		    pack("C",0 ) ;

	    }else{

		$FlowData{Pack}  .=
		    pack("a$TemplateArrayRef->{Length}" ) ;

	    }

	}

	$Count{$TemplateArrayRef->{Id}} += 1 ; 

    }

    return(
	   \%FlowData,
	   \@Errors
	   );

}
#################### END sub flow_encode() #################

#################### START sub template_encode() ###########
sub template_encode{
  my ($TemplateRef,$HeaderRef) = @_;
  my %TemplateData = () ;
  my $ScopeCount = 0 ;
  my @Errors = () ;
  my $Error  = undef ;

  #
  # check template hash reference
  #

  unless( defined $TemplateRef->{TemplateId} ){
      $Error = "ERROR : NO TEMPLATE ID" ;
      push(@Errors,$Error) ;
  }

  unless( defined $TemplateRef->{SetId} ){
      $Error = "ERROR : NO SET ID" ;
      push(@Errors,$Error) ;
  }

  if( $HeaderRef->{VersionNum} == NetFlowv9 ){

      if( $TemplateRef->{SetId} != NFWV9_DataTemplateSetId &&
	  $TemplateRef->{SetId} != NFWV9_OptionTemplateSetId ){
	
	$Error = "ERROR : UNMATCH SET ID FOR NETFLOWV9 TEMPLATE=$TemplateRef->{TemplateId}" ;
	push(@Errors,$Error) ;
	
      }

  }elsif( $HeaderRef->{VersionNum} == IPFIX ){

      if( $TemplateRef->{SetId} != IPFIX_DataTemplateSetId &&
	  $TemplateRef->{SetId} != IPFIX_OptionTemplateSetId ){

	  $Error = "ERROR : UNMATCH SET ID FOR IPFIX TEMPLATE=$TemplateRef->{TemplateId}" ;
	  push(@Errors,$Error) ;

      }
 
  }

  return(
	 \%TemplateData,
	 \@Errors
	 ) if $#Errors >= 0 ;

  $TemplateData{SetId} = $TemplateRef->{SetId} ;

  $ScopeCount = $TemplateRef->{ScopeCount} 
      if defined $TemplateRef->{ScopeCount}  ;

  $TemplateRef->{FieldCount} = $#{$TemplateRef->{Template}}+1 
      unless defined $TemplateRef->{FieldCount} ;

  #
  # NetFlow v9 pack data template header
  #
      
  if( $TemplateRef->{SetId} == NFWV9_DataTemplateSetId ){
	  
    $TemplateData{Pack} = 
      pack(
	   "nn", 
	   $TemplateRef->{TemplateId},
	   $TemplateRef->{FieldCount}
	  ) ;
    
  #
  # NetFlow v9 pack option template header
  #
    
  }elsif( $TemplateRef->{SetId} == NFWV9_OptionTemplateSetId ){

    $TemplateData{Pack} = 
      pack(
	   "nnn", 
	   $TemplateRef->{TemplateId},
	   $ScopeCount*4,
	   ($#{$TemplateRef->{Template}}+1-$ScopeCount)*4,
	  ) ;

  #
  # IPFIX pack data template header
  #

  }elsif( $TemplateRef->{SetId} == IPFIX_DataTemplateSetId ){

    #
    # Template Withdraw
    #
    
    if( $TemplateRef->{FieldCount} == 0 ){
      
      $TemplateData{Pack} = 
	pack(
	     "nn", 
	     $TemplateRef->{TemplateId},
	     0
	    ) ;
      
    }else{
	
      $TemplateData{Pack} = 
	pack(
	     "nn", 
	     $TemplateRef->{TemplateId},
	     $TemplateRef->{FieldCount}
	    ) ;
      
    }
   
  #
  # IPFIX pack option template header
  #
 
  }elsif( $TemplateRef->{SetId} == IPFIX_OptionTemplateSetId ){
      
    #
    # Template Withdraw
    #
    
    if( $TemplateRef->{FieldCount} == 0 ){
      
	$TemplateData{Pack} = 
	  pack(
	       "nn", 
	       $TemplateRef->{TemplateId},
	       0
	      ) ;
	
      }else{
	
	$TemplateData{Pack} = 
	  pack(
	       "nnn", 
	       $TemplateRef->{TemplateId},
	       ($#{$TemplateRef->{Template}}+1-$ScopeCount),
	       $ScopeCount,
	      ) ;
      }

  }

  #
  # pack template
  #

  if( $TemplateRef->{FieldCount} > 0 ){

      foreach my $Ref ( @{$TemplateRef->{Template}}  ){

	#
	# Enterprise Num
	#

	if( $Ref->{Id}  =~ /([\d]+)\.([\d]+)/ ){
	  
	  $TemplateData{Pack} .= 
	    pack(
		 "nnN", 
		 $2+0x8000,
		 $Ref->{Length},
		 $1,
		) ;
	  
	}else{
	  
	  $TemplateData{Pack} .= 
	    pack(
		 "nn", 
		 $Ref->{Id},
		 $Ref->{Length}
		) ;
	  
	}
	
      }
      
  }
  
  return(
	 \%TemplateData,
	 \@Errors
	);
  
}
#################### END sub template_encode() #############


#################### START sub decode() ####################
sub decode {
  my ($NetFlowPktRef,$InputTemplateRef) = @_;
  my $NetFlowHeaderRef = undef ;
  my $FlowSetHeaderRef = undef ;
  my $TemplateRef = undef ;
  my @Template = () ;
  my @Flows = ();
  my @Errors = ();
  my $Error = undef;

  my $OffSet = 0 ;
  my $FlowSetOffSet = 0 ;
  my $FlowCount = 0 ;

  #
  # check packet data
  #

  if( ref($NetFlowPktRef) ne "SCALAR"  ){

	$Error = "ERROR : NO PACKET DATA" ;
	push( @Errors,$Error ) ;

	return(
	 $NetFlowHeaderRef,
	 \@Template,
	 \@Flows,
	 \@Errors
	       );

  }

  #
  # insert template data
  #

  if( defined($InputTemplateRef) || ref($InputTemplateRef) eq "ARRAY" ){

      push( @Template,@{$InputTemplateRef} ) ;

  }elsif( defined($InputTemplateRef) ){

      $Error = "WARNING : NOT REF TEMPLATE DATA" ;
      push( @Errors,$Error ) ;

  }

  # 
  # header decode
  #

  ($NetFlowHeaderRef,$Error) = 
    &header_decode( $NetFlowPktRef, \$OffSet ) ; 

  #
  # IPFIX decode
  #

  if( $NetFlowHeaderRef->{VersionNum} == IPFIX ){

      while( $OffSet < $NetFlowHeaderRef->{Length} ){

	  my $DecodeTemplateRef = undef ;
	  my $FlowRef = undef ;
	  my $TemplateRef  = undef ;

	  if( (length($$NetFlowPktRef) - $OffSet) < 4 ){

	      if( $FlowCount ne $NetFlowHeaderRef->{Count} ){
		  $Error = "WARNING : UNMATCH FLOW COUNT" ;
		  push( @Errors,$Error ) ;
	      }

	      last ;
	  }

	  $FlowSetOffSet = $OffSet ;
	  
	  #
	  # decode flowset
	  #

	  $FlowSetHeaderRef = 
	      &flowset_decode(
			      $NetFlowPktRef,
			      \$OffSet) ;

	  #
	  # search for template
	  #

	  if( $FlowSetHeaderRef->{SetId} >= MinDataSetId ){

	      ( $DecodeTemplateRef,$Error ) = 
		  &search_template(
				   $FlowSetHeaderRef->{SetId},
				   \@Template
				   ) ; 

	      unless( defined $DecodeTemplateRef ){

		  push( @Errors,$Error ) ;
		  $OffSet = $FlowSetHeaderRef->{Length}+$FlowSetOffSet 
		      if defined $FlowSetHeaderRef->{Length} ;

		  next ;

	      }

	  }

	  while( $FlowSetHeaderRef->{Length} > ($OffSet-$FlowSetOffSet) ){

	      #
              # check word alignment
	      #

	      if( ($FlowSetHeaderRef->{Length}-($OffSet-$FlowSetOffSet)) < 4 ){

		  $OffSet = $FlowSetHeaderRef->{Length} + $FlowSetOffSet ;
		  last ;

	      }

	     #
             # decode data template or option Template
	     #
 
	     if( $FlowSetHeaderRef->{SetId} < MinDataSetId ){ 

		  ( $TemplateRef,$Error ) = 
		      &template_decode(
				     $NetFlowPktRef,
				     \$OffSet,
				     $FlowSetHeaderRef,
				     \$NetFlowHeaderRef->{VersionNum}  
				       ) ;

		  if( defined $Error ){

			push(@Errors,$Error) ;      
			last ;

	  	  }

		  $FlowCount += 1 ;

		  @Template = 
		    grep{
		      $_ if( $_->{TemplateId} ne $TemplateRef->{TemplateId} ) ;
		    } @Template ;

		  push(@Template,$TemplateRef) ;

	     #
             # decode flow records
	     #

	     }else{

		  ( $FlowRef, $Error ) = 
		      &flow_decode(
				   $NetFlowPktRef,
				   \$OffSet,
				   $DecodeTemplateRef,
				   \$NetFlowHeaderRef->{VersionNum}
				   ) ;

		  if( defined $Error ){
			push(@Errors,$Error) ;      
			last ;
	  	  }

		  $FlowCount += 1 ;
		  push(@Flows,$FlowRef) ;

	      }

	  }

      }

  #
  # NetFlow version 9 decode
  #

  }elsif( $NetFlowHeaderRef->{VersionNum} == NetFlowv9 ){

      while( $FlowCount < $NetFlowHeaderRef->{Count} ){ 
	  my $DecodeTemplateRef = undef ;
	  my $FlowRef = undef ;
	  my $TemplateRef  = undef ;

	  if( (length($$NetFlowPktRef) - $OffSet) < 4 ){

	      if( $FlowCount ne $NetFlowHeaderRef->{Count} ){
		  $Error = "WARNING : UNMATCH FLOW COUNT" ;
		  push( @Errors,$Error ) ;
	      }

	      last ;
	  }
	  
	  $FlowSetOffSet = $OffSet ;
	  
	  #
	  # decode flowset
	  #

	  $FlowSetHeaderRef = 
	      &flowset_decode(
			      $NetFlowPktRef,
			      \$OffSet) ;

	  #
	  # search for template
	  #

	  if( $FlowSetHeaderRef->{SetId} >= MinDataSetId ){

	      ( $DecodeTemplateRef,$Error ) = 
		  &search_template(
				   $FlowSetHeaderRef->{SetId},
				   \@Template
				   ) ; 

	      unless( defined $DecodeTemplateRef ){

		  push( @Errors,$Error ) ;
		  $OffSet = $FlowSetHeaderRef->{Length}+$FlowSetOffSet 
		      if defined $FlowSetHeaderRef->{Length} ;

		  next ;

	      }

	  }

	  while( $FlowSetHeaderRef->{Length} > ($OffSet-$FlowSetOffSet) ){

	      #
              # check word alignment
	      #

	      if( ($FlowSetHeaderRef->{Length}-($OffSet-$FlowSetOffSet)) < 4 ){

		  $OffSet = $FlowSetHeaderRef->{Length} + $FlowSetOffSet ;
		  last ;

	      }

	     #
             # decode data template or option Template
	     #
 
	      if( $FlowSetHeaderRef->{SetId} < MinDataSetId ){

		  ( $TemplateRef,$Error ) = 
		      &template_decode(
				     $NetFlowPktRef,
				     \$OffSet,
				     $FlowSetHeaderRef,
				     \$NetFlowHeaderRef->{VersionNum}
				       ) ;

		  if( defined $Error ){

			push(@Errors,$Error) ;      
			last ;

	  	  }

		  $FlowCount += 1 ;

		  @Template = 
		    grep{
		      $_ if( $_->{TemplateId} ne $TemplateRef->{TemplateId} ) ;
		    } @Template ;

		  push(@Template,$TemplateRef) ;

	     #
             # decode flow records
	     #

	     }else{

		  ( $FlowRef, $Error ) = 
		      &flow_decode(
				   $NetFlowPktRef,
				   \$OffSet,
				   $DecodeTemplateRef,
				   \$NetFlowHeaderRef->{VersionNum}
				   ) ;

		  if( defined $Error ){
			push(@Errors,$Error) ;      
			last ;
	  	  }

		  $FlowCount += 1 ;
		  push(@Flows,$FlowRef) ;

	      }

	  }

      }

  #
  # NetFlow version 5 Decode
  #      

  }elsif( $NetFlowHeaderRef->{VersionNum} == NetFlowv5 ){

      while( $FlowCount < $NetFlowHeaderRef->{Count} ){ 

	  my $FlowRef = undef ;

	  ($FlowRef,$Error) = 
	      &flow_decode(
		      $NetFlowPktRef,
		      \$OffSet,
		      \%TemplateForNetFlowv5
			   ) ;

	  $FlowRef->{SetId} = undef ;

	  if( defined $Error ){

		push(@Errors,$Error) ;
		last ;

	  }

	  $FlowCount += 1 ;
	  push(@Flows,$FlowRef) ;

      }

  #
  # NetFlow version 8 Decode
  #      

  }elsif( $NetFlowHeaderRef->{VersionNum} == NetFlowv8 ){

	$Error = "ERROR : NOT SUPPORT NETFLOW VER.8" ;
	push( @Errors,$Error ) ;

  }else{

	$Error = "ERROR : NOT NETFLOW DATA" ;
	push( @Errors,$Error ) ;

  }

  return(
	 $NetFlowHeaderRef,
	 \@Template,
	 \@Flows,
	 \@Errors
	 );

}
#################### END sub decode() ######################

#################### START sub search_template() ###########
sub search_template{
    my ( $TemplateId, $TemplatesArrayRef) = @_ ;
    my $DecodeTemplateRef = undef ;
    my $Error = undef ;

    ($DecodeTemplateRef,undef) = 
	grep{
	    $_ if $_->{TemplateId} eq $TemplateId ;
	} @{$TemplatesArrayRef} ;
	      
    #
    # nothing template for flow data
    #

    unless( defined $DecodeTemplateRef ){
	  $Error = "WARNING : NOT FOUND TEMPLATE=$TemplateId" ;
    }

    return(
	   $DecodeTemplateRef,
	   $Error
	   ) ;

}

#################### START sub header_decode() #############
sub header_decode{
  my ($NetFlowPktRef,$OffSetRef) = @_;
  my %NetFlowHeader = ();
  my $error = undef;

  #
  # Extract Version
  #

  ($NetFlowHeader{VersionNum}) 
      = unpack("n", $$NetFlowPktRef); 

  $$OffSetRef += 2 ;

  if( $NetFlowHeader{VersionNum} == IPFIX ){

      ($NetFlowHeader{Length},
       $NetFlowHeader{UnixSecs},
       $NetFlowHeader{SequenceNum},
       $NetFlowHeader{ObservationDomainId}) =
	   unpack("x$$OffSetRef nNNN", $$NetFlowPktRef);

      $$OffSetRef += 2 + 4*3;
 
  }elsif( $NetFlowHeader{VersionNum} == NetFlowv9 ){

      ($NetFlowHeader{Count},
       $NetFlowHeader{SysUpTime},
       $NetFlowHeader{UnixSecs},
       $NetFlowHeader{SequenceNum},
       $NetFlowHeader{SourceId}) =
	   unpack("x$$OffSetRef nNNNN", $$NetFlowPktRef);

      $$OffSetRef += 2 + 4*4; 

  }elsif( $NetFlowHeader{VersionNum} == NetFlowv8 ){
  }elsif( $NetFlowHeader{VersionNum} == NetFlowv5 ){

    my $Sampling = undef ;

      ($NetFlowHeader{Count},
       $NetFlowHeader{SysUpTime},
       $NetFlowHeader{UnixSecs},
       $NetFlowHeader{UnixNsecs},
       $NetFlowHeader{FlowSequenceNum},
       $NetFlowHeader{EngineType},
       $NetFlowHeader{EngineId},
       $Sampling ) =
	   unpack("x$$OffSetRef nNNNNCCn", $$NetFlowPktRef);

    $NetFlowHeader{SamplingMode}     =   $Sampling >> 14 ;
    $NetFlowHeader{SamplingInterval} =   $Sampling & 0x3FFF ;

    $$OffSetRef += 2*1 + 4*4 + 1*2 + 2*1 ; 

  }

  return(
	 \%NetFlowHeader,
	 $error
	 ) ;

}
#################### END sub header_decode() ###############

#################### START sub flowset_decode() ############
sub flowset_decode{
  my ($NetFlowPktRef,$OffSetRef) = @_;
  my %FlowSetHeader = ();
  my @errors = ();
  my $error = undef;

  ($FlowSetHeader{SetId},
   $FlowSetHeader{Length}) =
      unpack("x$$OffSetRef nn", $$NetFlowPktRef);

  $$OffSetRef += 2*2 ;

  return(
	 \%FlowSetHeader
	 ) ;

}
#################### END sub flowset_decode() ##############

#################### START sub template_decode() ###########
sub template_decode{
  my ($NetFlowPktRef,$OffSetRef,$FlowSetHeaderRef,$VerNumRef) = @_;
  my %Template = ();
  my $error = undef;

  $Template{SetId} = $FlowSetHeaderRef->{SetId} ;

  #
  # decode data template for NetFlow v9 or IPFIX
  #

  if( $FlowSetHeaderRef->{SetId} == NFWV9_DataTemplateSetId ||
      $FlowSetHeaderRef->{SetId} == IPFIX_DataTemplateSetId ){

      ($Template{TemplateId},
       $Template{FieldCount}) = 
	   unpack("x$$OffSetRef nn", $$NetFlowPktRef);

      $$OffSetRef += 2*2 ;

  #
  # decode option template for IPFIX
  #

  }elsif( $FlowSetHeaderRef->{SetId} ==  IPFIX_OptionTemplateSetId ){
      
      ($Template{TemplateId},
       $Template{FieldCount}) = 
	   unpack("x$$OffSetRef nn", $$NetFlowPktRef);

      $$OffSetRef += 2*2 ;

      #
      # template withdraw check
      #

      if( $Template{FieldCount} != 0 ){
	  
	  ($Template{ScopeCount} ) =
	      unpack("x$$OffSetRef n", $$NetFlowPktRef);
	  $$OffSetRef += 2*1 ;

      }

  #
  # decode option template for NetFlow v9
  #

  }elsif( $FlowSetHeaderRef->{SetId} ==  NFWV9_OptionTemplateSetId ){

      ($Template{TemplateId},
       $Template{OptionScopeLength},
       $Template{OptionLength}) = 
	   unpack("x$$OffSetRef nnn", $$NetFlowPktRef);
      
      $$OffSetRef += 2*3 ;
      
      $Template{FieldCount} =
	  int(($Template{OptionScopeLength}+$Template{OptionLength})/4) ;
      
      $Template{ScopeCount} =   
	  int(($Template{OptionScopeLength})/4) ;
      
  }

  return (undef, 'ERROR: No fieldcount') if (!defined($Template{FieldCount}));

  for( my $n = 0 ; $n<$Template{FieldCount} ; $n++ ){

      if( $FlowSetHeaderRef->{SetId} <= IPFIX_OptionTemplateSetId ){

	  ($Template{Template}->[$n]->{Id},
	   $Template{Template}->[$n]->{Length}) = 
	       unpack("x$$OffSetRef nn", $$NetFlowPktRef);
	  $$OffSetRef += 2*2 ;

	  #
	  # check enterprise number
	  #

	  if( $$VerNumRef >= 10 ){

	      if( ($Template{Template}->[$n]->{Id} >> 15) == 1 ){

		  $Template{Template}->[$n]->{Id}-=0x8000;

		  ($Template{Template}->[$n]->{EnterpriseNum}) =
		      unpack("x$$OffSetRef N", $$NetFlowPktRef);
	    
		  $Template{Template}->[$n]->{Id} 
		  = $Template{Template}->[$n]->{EnterpriseNum}.".".
		      $Template{Template}->[$n]->{Id} ;

		  $$OffSetRef += 4 ;

	      }

	  }

      }

  }

  return(
	 \%Template,
	 $error
	 );

}
#################### END sub template_decode() #############

#################### START sub flow_decode() ###############
sub flow_decode{
  my ($NetFlowPktRef,$OffSetRef,$TemplateRef) = @_;
  my %Flow =() ;
  my $error = undef ;
  my $Length = undef ;

  if( defined $TemplateRef->{TemplateId} ){
    
    $Flow{SetId} = $TemplateRef->{TemplateId} ;

  }else{

    $error = "ERROR: NOT FOUND TEMPLATE ID" ; 
    
  }
	
  foreach my $ref ( @{$TemplateRef->{Template}}  ){
      
    #
    # Variable Length Type
    #

    if( $ref->{Length} == VariableLength ){

      $Length = 
	unpack("x$$OffSetRef C",$$NetFlowPktRef) ;

      $$OffSetRef += 1 ;

      if( $Length == 255 ){
	
	$Length = 
	  unpack("x$$OffSetRef n",$$NetFlowPktRef) ;

	$$OffSetRef += 2 ;

      }

    #
    # Fixed Length Type
    #

    }else{

      $Length = $ref->{Length} ;

    }

    #
    # One Template has multiple same Ids.
    #

    if( defined $Flow{$ref->{Id}} ){

      my $Value =
	unpack("x$$OffSetRef a$Length",$$NetFlowPktRef);
      
      $Flow{$ref->{Id}} = [ $Flow{$ref->{Id}} ] unless ref $Flow{$ref->{Id}} ;
      
      push( @{$Flow{$ref->{Id}}}, $Value ) ;

    #
    # Each Id is different than others.
    #
      
    }else{

      $Flow{$ref->{Id}} = 
	unpack("x$$OffSetRef a$Length",$$NetFlowPktRef); 
      
    }

    $$OffSetRef += $Length ;
    
  }

  return( 
 	\%Flow,
	$error
	 );

}
#################### END sub flow_decode() #################

1;

__END__

=head1 NAME


Net::Flow - decode and encode NetFlow/IPFIX datagrams.


=head1 SYNOPSIS


=head2 EXAMPLE#1 - Output Flow Records of NetFlow v5, v9 and IPFIX -

The following script simply outputs the received Flow Records after decoding NetFlow/IPFIX datagrams. It can parse the NetFlow v5, v9 and IPFIX. If it receive NetFlow v9/IPFIX datagrams, several Templates of NetFlow/IPFIX can be kept as ARRAY reference $TemplateArrayRef. By adding it as the input parameter, it can parse the NetFlow/IPFIX datagrams without templates. If received Packet has same Template Id, this Template is overwritten by new one.

    use strict ;
    use Net::Flow qw(decode) ;
    use IO::Socket::INET;

    my $receive_port = 9993 ;
    my $packet = undef ;
    my $TemplateArrayRef = undef ;
    my $sock = IO::Socket::INET->new( LocalPort =>$receive_port, Proto => 'udp') ;

    while ($sock->recv($packet,1548)) {

        my ($HeaderHashRef,$FlowArrayRef,$ErrorsArrayRef)=() ;

          ( $HeaderHashRef,
            $TemplateArrayRef,
            $FlowArrayRef,
            $ErrorsArrayRef)
            = Net::Flow::decode(
                                \$packet,
                                $TemplateArrayRef
			        ) ;

        grep{ print "$_\n" }@{$ErrorsArrayRef} if( @{$ErrorsArrayRef} ) ;

        print "\n- Header Information -\n" ;
        foreach my $Key ( sort keys %{$HeaderHashRef} ){
            printf " %s = %3d\n",$Key,$HeaderHashRef->{$Key} ;
        }

        foreach my $TemplateRef ( @{$TemplateArrayRef} ){
            print "\n-- Template Information --\n" ;

            foreach my $TempKey ( sort keys %{$TemplateRef} ){
                if( $TempKey eq "Template" ){
                    printf "  %s = \n",$TempKey ;
                    foreach my $Ref ( @{$TemplateRef->{Template}}  ){
                        foreach my $Key ( keys %{$Ref} ){
                            printf "   %s=%s", $Key, $Ref->{$Key} ;
                        }
                        print "\n" ;
                    }
                }else{
		    printf "  %s = %s\n", $TempKey, $TemplateRef->{$TempKey} ;
                }
            }
        }

        foreach my $FlowRef ( @{$FlowArrayRef} ){
            print "\n-- Flow Information --\n" ;

            foreach my $Id ( sort keys %{$FlowRef} ){
               if( $Id eq "SetId" ){
                   print "  $Id=$FlowRef->{$Id}\n" if defined $FlowRef->{$Id} ;
               }elsif( ref $FlowRef->{$Id} ){
                   printf "  Id=%s Value=",$Id ;
                   foreach my $Value ( @{$FlowRef->{$Id}} ){
                      printf "%s,",unpack("H*",$Value) ;
                   }
                   print "\n" ;
               }else{
                   printf "  Id=%s Value=%s\n",$Id,unpack("H*",$FlowRef->{$Id}) ;
            }
        }
    }

=head2 EXAMPLE#2 - Convert Protocol from NetFlow v5 to NetFlow v9 -

The following script converts NetFlow protocol from NetFlow v5 to NetFlow v9 as converter. At first, it decodes NetFlow v5 datagram. After that, these flow records are encoded into NetFlow v9 according to the particular Template which include sampling interval and sampling mode. And they are sent to the next Collector.

    use strict;
    use Net::Flow qw(decode encode) ;
    use IO::Socket::INET ;

    my $receive_port = 9995 ;
    my $send_port    = 9996 ;

    my $packet = undef ;
    my $TemplateRef = undef ;
    my $MyTemplateRef={
        'SetId'        =>0,
        'TemplateId'   =>300,
        'Template'=>[
                     { 'Length' => 4, 'Id' => 8  }, # SRC_ADDR
                     { 'Length' => 4, 'Id' => 12 }, # DST_ADDR
                     { 'Length' => 4, 'Id' => 2  }, # PKTS
                     { 'Length' => 4, 'Id' => 1  }, # BYTES
                     { 'Length' => 2, 'Id' => 7  }, # SRC_PORT
                     { 'Length' => 2, 'Id' => 11 }, # DST_PORT
                     { 'Length' => 1, 'Id' => 4  }, # PROT
                     { 'Length' => 1, 'Id' => 5  }, # TOS
                     { 'Length' => 4, 'Id' => 34 }, # SAMPLING_INT
                     { 'Length' => 4, 'Id' => 35 }, # SAMPLING_ALG
                     ],
        } ;

    my @MyTemplates = ( $MyTemplateRef ) ;

    my $EncodeHeaderHashRef = {
        'SourceId'    => 0,
        'VersionNum'  => 9,
        'SequenceNum' => 0,
        } ;

    my $r_sock = IO::Socket::INET->new( LocalPort => $receive_port,
                                        Proto => 'udp') ;

    my $s_sock = IO::Socket::INET->new( PeerAddr => '127.0.0.1',
                                       PeerPort =>  $send_port,
                                        Proto => 'udp' ) ;

    while ( $r_sock->recv($packet,1548) ) {

        my $PktsArrayRef = undef ;

        my ( $HeaderHashRef,
            undef,
            $FlowArrayRef,
            $ErrorsArrayRef )
            = Net::Flow::decode(
                                \$packet,
                                undef
                                ) ;

        grep{ print "$_\n" }@{$ErrorsArrayRef} if( @{$ErrorsArrayRef} ) ;

        foreach my $HashRef ( @{$FlowArrayRef} ){
            $HashRef->{"SetId"} = 300 ;
            $HashRef->{"34"} = pack("N",$HeaderHashRef->{"SamplingInterval"})
                if defined $HeaderHashRef->{"SamplingInterval"} ;
            $HashRef->{"35"} = pack("N",$HeaderHashRef->{"SamplingMode"})
                if defined $HeaderHashRef->{"SamplingMode"} ;
        }

        $EncodeHeaderHashRef->{"SysUpTime"}    = $HeaderHashRef->{"SysUpTime"} ;
        $EncodeHeaderHashRef->{"UnixSecs"}     = $HeaderHashRef->{"UnixSecs"} ;

        ( $EncodeHeaderHashRef,
         $PktsArrayRef,
         $ErrorsArrayRef)
            = Net::Flow::encode(
                                $EncodeHeaderHashRef,
                                \@MyTemplates,
                                $FlowArrayRef,
                                1400
                                ) ;

       grep{ print "$_\n" }@{$ErrorsArrayRef} if( @{$ErrorsArrayRef} ) ;

       foreach my $Ref (@{$PktsArrayRef}){
           $s_sock->send($$Ref) ;
       }

   }

=head1 DESCRIPTION

The Flow module provides the decoding function for NetFlow version 5,9 and IPFIX, and the encoding function for NetFlow version 9 and IPFIX. It supports NetFlow version 9 (RFC3945) and NetFlow version 5 (http://www.cisco.com/) and IPFIX(RFC5101). You can easily make the Flow Proxy, Protocol Converter and Flow Concentrator by using the combination of both function, just like Flow Mediator(draft-kobayashi-ipfix-mediator-model-02.txt). The Mediator would have multiple functions by utilizing intermediate process. And also, you can make the flexible Collector which can receive any Templates by using the Storable perl module.

=head1 FUNCTIONS

=head2 decode method

    ( $HeaderHashRef,
     $TemplateArrayRef,
     $FlowArrayRef,
     $ErrorsArrayRef ) =
      Net::Flow::decode(
                           \$Packets,
                           $InputTemplateArrayRef
                          ) ;

It returns a HASH reference containing the NetFlow/IPFIX Header information as $HeaderHashRef. And it returns ARRAY references with the Template and Flow Record (each ARRAY element contains a HASH reference for one Template or Flow Record) as $TemplateArrayRef or $FlowArrayRef. In case of an error a reference to an ARRAY containing the error messages is returned as $ErrorsArrayRef. The returned $TemplateArrayRef can be input on the next received packet which doesn't contain Template to decode it.

=head3 Return Values

=over 4

=item I<$HeaderHashRef>

A HASH reference containing information in case of IPFIX header, with the following keys:

  "VersionNum"
  "Length"
  "UnixSecs"
  "SequenceNum"
  "ObservationDomainId"

A HASH reference containing information in case of NetFlow v9 header, with the following keys:

  "VersionNum"
  "Count"
  "SysUpTime"
  "UnixSecs"
  "SequenceNum"
  "SourceId"

A HASH reference containing information in case of NetFlow v5 header, with the following keys:

  "VersionNum"
  "Count"
  "SysUpTime"
  "UnixSecs"
  "UnixNsecs"
  "FlowSequenceNum"
  "EngineType"
  "EngineId"
  "SamplingMode"
  "SamplingInterval"

All values of above keys are shown as decimal.

=item I<$TemplateArrayRef>

This ARRAY reference contains several Templates which are contained input NetFlow/IPFIX packet and $InputTemplateArrayRef. Each Template is given HASH references. This HASH reference provides Data Template and Option Template, as follows. 
A HASH reference containing information in case of Data Template, with the following keys:
  
  "SetId"
  "TemplateId"
  "FieldCount"
  "Template"

A HASH reference containing information in case of Option Template, with the following keys:

  "SetId"
  "TemplateId"
  "OptionScopeLength"
  "OptionLength"
  "FieldCount"
  "ScopeCount"
  "Template"

In case of IPFIX, "OptionScopeLength" and "OptionLength" are omitted.

In case of IPFIX, 0 value  of "FieldCount" has a particular meaning. if TemplateWithdrawMessage is received, "FieldCount" of corresponding Template would become value of 0. A HASH reference containing information in case of WithdrawTemplateMessage, with the following keys:

  "SetId"
  "FieldCount"
  "TemplateId"

All values for above keys other than "Template" are shown as decimal. The value for "Template" is a ARRAY references. Each ARRAY element contains a HASH reference for one pair of "Id" and "Length". This pair of "Id" and "Length" are shown as Field type. The order of this ARRAY means the order of this Template to decode data. A HASH reference containing information for each field type, with the following keys:  

  "Id"
  "Length"

If Enterprise Number is given in the IPFIX packets, the value of "Id" is presented by concatenating string between the value of Enterprise Number and the value of Information Element Id. For example, if Enterprise Number is "3000" and Information Element Id is "100", the value of "Id" becomes "3000.100". In case of IPFIX, 65535 value  of "Length" has a particular meaning. if "Length" is 65535, this field type means valiable length field. The length of field in each Flow Record is different.

The values for "Length","TemplateId","FieldCount" are shown as decimal.

=item I<$FlowArrayRef>

This ARRAY reference contains several HASH references for each Flow Record. This HASH reference provides Flow Record for Data Template and Option Template, as follows. A HASH reference contains "SetId" and Ids of Field type, as HASH key. The value for "SetId" is shown as decimal which means decoded TemplateId. The "Id" number means Field type. The value for "SetId" is shown as decimal. The value for "Id" number is shown as binary data. The value of each field is directly extracted from NetFlow/IPFIX packets without modification.

  "SetId"
  "Id"
 
If one Flow Record has multiple Fields of same type, the value for Id number becomes a ARRAY references. Each ARRAY element is value shown as binary data. The order of this ARRAY means the order of multiple same Fields in one Flow Record. 

=back

=head2 encode method

    ( $HeaderHashRef,
     $PktsArrayRef,
     $ErrorsArrayRef) = 
      Net::Flow::encode(
                       $HeaderHashRef,
                       $TemplateArrayRef,
                       $FlowArrayRef,
                       $MaxSize
                       ) ;

Input parameters are same data structure returned from decode function. "$MaxSize" means maximum payload size. This function make several NetFlow payloads without exceeding the maximum size. 
These values for the input $HeaderHashRef, such as "UnixSecs", "SysUptime","SourceId" and "ObservationDomainId", are used in this method. The other values are ignored. These values for output $HeaderHashRef means header information of the latest IPFIX/NetFlow datagram. 

=head3 Return Values

=over 4

=item I<$PktsArrayRef>

This ARRAY reference contains several SCALAR references for each NetFlow datagram which is shown binary. It can be used as UDP datagram.

=back

=head1 AUTHOR

Atsushi Kobayashi <akoba@nttv6.net>
http://www3.plala.or.jp/akoba/

Let me know your flow-based measurement system using Net::Flow. 

=head1 ACKNOWLEDGMENTS

This perl module was supported by the Ministry of Internal Affairs and Communications of Japan. 
In the considerations of variable length fields, I have received support from Philip Gladstone. 

=head1 COPYRIGHT

Copyright (c) 2007-2008 NTT Information Sharing Platform Laboratories

This package is free software and is provided "as is" without express or implied warranty.  It may be used, redistributed and/or modified under the terms of the Perl Artistic License (see http://www.perl.com/perl/misc/Artistic.html)

=cut
