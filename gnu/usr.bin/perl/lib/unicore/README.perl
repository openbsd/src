The *.txt files were copied from

	http://www.unicode.org/Public/UNIDATA/

as of Unicode 4.0.0 (April 2003).

The two big files, NormalizationTest.txt (2.0MB) and Unihan.txt
(25.7MB) were not included due to space considerations.  Also NOT
included were any *.html files and the Derived* files

    DerivedAge.txt
    DerivedCoreProperties.txt
    DerivedNormalizationProps.txt

To be 8.3-friendly, the lib/unicore/PropertyValueAliases.txt was
renamed to be lib/unicore/PropValueAliases.txt, since otherwise
it would have conflicted with lib/unicore/PropertyAliases.txt.

The *.pl files are generated from these files by the mktables script.

-- 
jhi@iki.fi
