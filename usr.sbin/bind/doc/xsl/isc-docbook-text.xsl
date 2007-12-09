<!--
 - Copyright (C) 2005  Internet Systems Consortium, Inc. ("ISC")
 -
 - Permission to use, copy, modify, and distribute this software for any
 - purpose with or without fee is hereby granted, provided that the above
 - copyright notice and this permission notice appear in all copies.
 -
 - THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 - REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 - AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 - INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 - LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 - OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 - PERFORMANCE OF THIS SOFTWARE.
-->

<!-- $ISC: isc-docbook-text.xsl,v 1.1.10.1 2005/09/05 03:01:47 marka Exp $ -->

<!-- Tweaks to Docbook-XSL HTML for producing flat ASCII text. --> 

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0"
		xmlns:l="http://docbook.sourceforge.net/xmlns/l10n/1.0">

  <!-- Import our Docbook HTML stuff -->
  <xsl:import href="isc-docbook-html.xsl"/>

  <!-- Disable tables of contents (for now - tweak as needed) -->
  <xsl:param name="generate.toc"/>

  <!-- Voodoo to read i18n/l10n overrides directly from this stylesheet -->
  <xsl:param name="local.l10n.xml" select="document('')"/>

  <!-- Customize Docbook-XSL i18n/l10n mappings. -->
  <l:i18n>
    <l:l10n language="en" english-language-name="English">

      <!-- Please use plain old ASCII quotes -->
      <l:dingbat key="startquote" text='&quot;'/>
      <l:dingbat key="endquote"   text='&quot;'/>

    </l:l10n>
  </l:i18n>

</xsl:stylesheet>

<!-- 
  - Local variables:
  - mode: sgml
  - End:
 -->
