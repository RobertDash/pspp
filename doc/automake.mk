## Process this file with automake to produce Makefile.in  -*- makefile -*-

info_TEXINFOS = doc/pspp.texinfo doc/pspp-dev.texinfo

doc_pspp_TEXINFOS = doc/version.texi \
	doc/bugs.texi \
	doc/command-index.texi \
	doc/concept-index.texi \
	doc/data-io.texi \
	doc/data-selection.texi \
	doc/expressions.texi \
	doc/files.texi \
	doc/combining.texi \
	doc/flow-control.texi \
	doc/function-index.texi \
	doc/installing.texi \
	doc/introduction.texi \
	doc/invoking.texi \
	doc/language.texi \
	doc/license.texi \
	doc/ni.texi \
	doc/not-implemented.texi \
	doc/statistics.texi \
	doc/transformation.texi \
	doc/tutorial.texi \
	doc/tut.texi \
	doc/regression.texi \
	doc/utilities.texi \
	doc/variables.texi \
	doc/fdl.texi 

doc_pspp_dev_TEXINFOS = doc/version-dev.texi \
	doc/dev/intro.texi \
	doc/dev/concepts.texi \
	doc/dev/syntax.texi \
	doc/dev/data.texi \
	doc/dev/i18n.texi \
	doc/dev/output.texi \
	doc/dev/system-file-format.texi \
	doc/dev/portable-file-format.texi \
	doc/dev/q2c.texi

EXTRA_DIST += doc/pspp.man \
	doc/get-commands.pl

doc/ni.texi: $(top_srcdir)/src/language/command.def doc/get-commands.pl
	@$(MKDIR_P)  doc
	$(PERL) $(top_srcdir)/doc/get-commands.pl $(top_srcdir)/src/language/command.def > $@

doc/tut.texi:
	@$(MKDIR_P) doc
	echo "@set example-dir $(examplesdir)" > $@


doc/pspp.xml: doc/pspp.texinfo $(doc_pspp_TEXINFOS)
	@$(MKDIR_P)  doc
	$(MAKEINFO) $(AM_MAKEINFOFLAGS) --docbook -I $(top_srcdir) \
		$(top_srcdir)/doc/pspp.texinfo -o - \
		| $(SED) 's/Time-&-Date/Time-\&amp;-Date/g' > $@
	$(XMLLINT) --output /dev/null $@ 2>&1 2> /dev/null || ( $(RM) $@ && false )

docbookdir = $(docdir)
docbook_DATA = doc/pspp.xml
EXTRA_DIST += doc/pspp.xml

EXTRA_DIST += doc/OChangeLog
CLEANFILES += pspp-dev.dvi $(docbook_DATA)
