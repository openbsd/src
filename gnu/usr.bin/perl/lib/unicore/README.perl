The *.txt files were copied 27 Mar 2002 from

	http://www.unicode.org/Public/UNIDATA/

The two big files, NormalizationTest.txt (2.0MB) and Unihan.txt
(25.7MB) were not included due to space considerations.  Also NOT
included were any *.html files and the derived files:

	DerivedAge.txt
	DerivedCoreProperties.txt
	DerivedNormalizationProps.txt
	DerivedProperties.txt

and the normalization-related files

	NormalizationCorrections.txt
	NormalizationTest.txt

To be 8.3-friendly, the lib/unicore/PropertyValueAliases.txt was
renamed to be lib/unicore/PropValueAliases.txt, since otherwise
it would have conflicted with lib/unicore/PropertyAliases.txt.

The *.pl files are generated from these files by the mktables script.

-- 
jhi@iki.fi
