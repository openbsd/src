#!/bin/sh
# $LynxId: indent.sh,v 1.3 2007/05/13 16:26:51 tom Exp $
# Indent LYNX files.
NOOP=no
OPTS='
--blank-lines-after-declarations
--blank-lines-after-procedures
--braces-on-if-line
--continue-at-parentheses
--cuddle-else
--dont-break-procedure-type
--indent-level4
--leave-preprocessor-space
--line-length80
--no-space-after-function-call-names
--parameter-indentation4
--space-after-cast
--space-special-semicolon
--swallow-optional-blank-lines
-T AddressDefList 
-T BOOL
-T BOOLEAN
-T CSOfield_info 
-T DIR
-T DocAddress 
-T DocInfo 
-T DocObj
-T EntryInfo 
-T EditFieldData 
-T FILE
-T GCC_NORETURN 
-T GCC_UNUSED 
-T GLOBALREF 
-T GroupDef 
-T GroupDefList 
-T HTAAFailReasonType 
-T HTAAProt 
-T HTAARealm 
-T HTAAServer 
-T HTAssoc 
-T HTAssocList 
-T HTAtom 
-T HTBTElement 
-T HTBTree 
-T HTChildAnchor 
-T HTChunk 
-T HTConverter 
-T HTFormat 
-T HTLine 
-T HTLinkType 
-T HTList 
-T HTParentAnchor 
-T HTParentAnchor0 
-T HTPresentation 
-T HTStream
-T HTStyle 
-T HTStyleChange 
-T HTStyleSheet 
-T HText 
-T HyperDoc 
-T InitResponseAPDU 
-T Item 
-T ItemList 
-T LYNX_ADDRINFO
-T LYNX_HOSTENT
-T LYUCcharset 
-T LexItem 
-T ProgramPaths 
-T STable_cellinfo 
-T STable_info 
-T STable_rowinfo 
-T STable_states 
-T SearchAPDU 
-T SearchResponseAPDU 
-T TextAnchor 
-T UCode_t 
-T UserDefList 
-T WAISDocumentCodes 
-T WAISDocumentHeader 
-T WAISDocumentHeadlines 
-T WAISDocumentLongHeader 
-T WAISDocumentShortHeader 
-T WAISDocumentText 
-T WAISInitResponse 
-T WAISSearch 
-T _cdecl
-T any 
-T bit_map 
-T boolean 
-T bstring 
-T data_tag
-T eServerType 
-T lynx_list_item_type 
-T pdu_type
-T query_term 
-nbacc
'
for name in $*
do
	case $name in
	-n)	NOOP=yes
		OPTS="$OPTS -v"
		;;
	-*)
		OPTS="$OPTS $name"
		;;
	*.[ch])
		save="${name}".a$$
		test="${name}".b$$
		rm -f "$save" "$test"
		mv "$name" "$save"
		sed \
			-e '/MODULE_ID(/s/)$/);/' \
			-e 's,)[ 	]*\<GCC_PRINTFLIKE,);//GCC_PRINTFLIKE,' \
			-e 's,[ 	]*\<GCC_NORETURN;,;//GCC_NORETURN;,' \
			-e 's,[ 	]*\<GCC_UNUSED;,;//GCC_UNUSED;,' \
			"$save" >"$test"
		cp "$test" "$name"
		chmod u+w "$name"
		${INDENT_PROG-indent} -npro $OPTS "$name"
		sed \
			-e '/MODULE_ID(/s/);$/)/' \
			-e 's,;[ 	]*//GCC_UNUSED;, GCC_UNUSED;,' \
			-e 's,;[ 	]*//GCC_NORETURN;, GCC_NORETURN;,' \
			-e 's,);[ 	]*//GCC_PRINTFLIKE,) GCC_PRINTFLIKE,' \
			"$name" >"$test"
		mv "$test" "$name"
		rm -f "${name}~"
		if test $NOOP = yes ; then
			if ! ( cmp -s "$name" $save )
			then
				diff -u $save "$name"
			fi
			mv "$save" "$name"
			rm -f "${name}~"
		else
			if ( cmp -s "$name" "$save" )
			then
				echo "** unchanged $name"
				rm -f "${name}" "${name}~"
				mv "$save" "$name"
			else
				echo "** updated $name"
				rm -f "$save"
			fi
		fi
		;;
	*)
		echo "** ignored:   $name"
		;;
	esac
done
