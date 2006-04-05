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

<!-- $ISC: pre-latex.xsl,v 1.2.10.3 2005/09/15 02:28:26 marka Exp $ -->

<!--
  - Whack &mdash; into something that won't choke LaTeX.
  - There's probably a better way to do this, but this will work for now.
  --> 

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">

  <xsl:variable name="mdash" select="'&#8212;'"/>

  <xsl:template name="fix-mdash" match="text()[contains(., '&#8212;')]">
    <xsl:param name="s" select="."/>
    <xsl:choose>
      <xsl:when test="contains($s, $mdash)">
        <xsl:value-of select="substring-before($s, $mdash)"/>
	<xsl:text>---</xsl:text>
        <xsl:call-template name="fix-mdash">
	  <xsl:with-param name="s" select="substring-after($s, $mdash)"/>
	</xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
        <xsl:value-of select="$s"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="@*|node()">
    <xsl:copy>
      <xsl:copy-of select="@*"/>
      <xsl:apply-templates/>
    </xsl:copy>
  </xsl:template>

  <xsl:template match="/">
    <xsl:apply-templates/>
  </xsl:template>

</xsl:stylesheet>
