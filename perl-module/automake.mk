## Process this file with automake to produce Makefile.in  -*- makefile -*-

# PSPP

module_sources = \
 perl-module/Changes \
 perl-module/COPYING \
 perl-module/Examples.pod \
 perl-module/Makefile.PL \
 perl-module/MANIFEST \
 perl-module/ppport.h \
 perl-module/PSPP.xs \
 perl-module/README \
 perl-module/typemap \
 perl-module/lib/PSPP.pm \
 perl-module/t/Pspp.t

PERL_MAKEFLAGS = $(AM_MAKEFLAGS) LD_RUN_PATH=$(pkglibdir)

perl-module/pspp-module-config: Makefile
	(echo '%Locations = (';\
	 printf "  SourceDir => '";\
	 (cd $(top_srcdir) && echo `pwd`\', ) ;\
	 printf "  BuildDir => '";\
	 (cd $(top_builddir) && echo `pwd`\' );\
	 echo ');') > $(top_builddir)/perl-module/pspp-module-config

perl-module/Makefile: perl-module/Makefile.PL perl-module/pspp-module-config $(module_sources)
	cd perl-module && $(PERL) Makefile.PL PREFIX=$(prefix)

perl-module/PSPP-Perl-$(VERSION_FOR_PERL).tar.gz: $(module_sources) perl-module/Makefile
	rm -f $@
	cd perl-module && $(MAKE) $(PERL_MAKEFLAGS) tardist

PHONY += module-make
module-make: perl-module/Makefile
	cd perl-module && $(MAKE) $(PERL_MAKEFLAGS)

ALL_LOCAL += perl_module_tarball
perl_module_tarball: $(module_sources) src/libpspp-core.la
	@if test x"$(top_builddir)" != x"$(top_srcdir)" ; then \
	 for f in $(module_sources); do \
	  destdir=`dirname $$f` ;\
	  mkdir -p $$destdir ;\
	  if test ! -e "$(top_builddir)/$$f" || \
	     test "$(top_srcdir)/$$f" -nt "$(top_builddir)/$$f" ; then \
		 cp $(top_srcdir)/$$f $$destdir ; \
		 echo cp $(top_srcdir)/$$f $$destdir ; \
	  fi ; \
	 done \
	fi
	$(MAKE) $(PERL_MAKEFLAGS) module-make perl-module/PSPP-Perl-$(VERSION_FOR_PERL).tar.gz

CLEAN_LOCAL += perl_module_clean
perl_module_clean:
	cd perl-module && $(MAKE) $(AM_MAKEFLAGS) clean || true
	if test x"$(top_builddir)" != x"$(top_srcdir)" ; then \
	  rm -f $(module_sources) ; \
	fi
	rm -f perl-module/Makefile.old

CLEANFILES += \
        perl-module/PSPP-Perl-$(VERSION_FOR_PERL).tar.gz \
	perl-module/pspp-module-config \
	perl-module/const-c.inc \
	perl-module/const-xs.inc 

EXTRA_DIST +=  $(module_sources)
