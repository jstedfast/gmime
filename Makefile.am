## Process this file with automake to produce Makefile.in

SUBDIRS = . tests doc

INCLUDES = -I@srcdir@ $(VERSION_FLAGS) @GLIB_CFLAGS@ -DG_LOG_DOMAIN=\"gmime\"

VERSION_FLAGS = -DGMIME_VERSION=\"@GMIME_VERSION@\"

noinst_PROGRAMS = gen-table test-mime test-parser

bin_SCRIPTS = gmime-config

lib_LTLIBRARIES = libgmime.la

libgmime_la_LDFLAGS = -version-info @GMIME_VERSION_INFO@

libgmime_la_SOURCES = 		\
	gmime-content-type.c	\
	gmime-message.c		\
	gmime-param.c		\
	gmime-parser.c		\
	gmime-part.c		\
	gmime-utils.c		\
	internet-address.c	\
	md5-utils.c

gmimeincdir = $(includedir)/gmime
gmimeinc_HEADERS = 		\
	gmime.h			\
	gmime-table-private.h	\
	gmime-content-type.h	\
	gmime-message.h		\
	gmime-param.h		\
	gmime-parser.h		\
	gmime-part.h		\
	gmime-utils.h		\
	internet-address.h	\
	md5-utils.h

DEPS = $(top_builddir)/libgmime.la
LDADDS = $(top_builddir)/libgmime.la @GLIB_LIBS@

test_mime_SOURCES = test-mime.c
test_mime_LDFLAGS = 
test_mime_DEPENDENCIES = $(DEPS)
test_mime_LDADD = $(LDADDS)

test_parser_SOURCES = test-parser.c
test_parser_LDFLAGS = 
test_parser_DEPENDENCIES = $(DEPS)
test_parser_LDADD = $(LDADDS)

gen_table_SOURCES = gen-table.c
gen_table_LDFLAGS = 
gen_table_DEPENDENCIES = 
gen_table_LDADD = 

check-local: tests

testall : tests

tests: MIMEtests PARSERtests

MIMEtests : test-mime
	@echo "##"
	@echo "## MIME regression tests"
	@echo "##"

PARSERtests : test-parser
	@echo "##"
	@echo "## MIME Parser regression tests"
	@echo "##"

dist-hook: libgmime.spec
	-cp libgmime.spec $(distdir)
	(cd $(srcdir) ; tar -cf -) | (cd $(distdir); tar xf -)

## We create gmimeConf.sh here and not from configure because we want
## to get the paths expanded correctly.  Macros like srcdir are given
## the value NONE in configure if the user doesn't specify them (this
## is an autoconf feature, not a bug).

confexecdir = $(libdir)
confexec_DATA = gmimeConf.sh

CLEANFILES = gmimeConf.sh

confexecdir = $(libdir)
confexec_DATA = gmimeConf.sh
EXTRA_DIST = gmimeConf.sh.in libgmime.spec.in libgmime.spec gmime.m4

gmimeConf.sh: gmimeConf.sh.in Makefile
## Use sed and then mv to avoid problems if the user interrupts.
	sed -e 's?\@GMIME_LIBDIR\@?$(GMIME_LIBDIR)?g' \
	    -e 's?\@GMIME_INCLUDEDIR\@?$(GMIME_INCLUDEDIR)?g' \
	    -e 's?\@GMIME_LIBS\@?$(GMIME_LIBS)?g' \
	    -e 's?\@VERSION\@?$(VERSION)?g' \
	      < $(srcdir)/gmimeConf.sh.in > gmimeConf.tmp \
	  && mv gmimeConf.tmp gmimeConf.sh

$(srcdir)/libgmime:
	-$(RM) $(srcdir)/libgmime
	ln -s $(srcdir)/. $(srcdir)/libgmime

$(libgmime_la_SOURCES): $(srcdir)/libgmime

install-data-local:
	-mkdir -p $(DESTDIR)$(datadir)/aclocal
	$(INSTALL_DATA) $(srcdir)/gmime.m4 $(DESTDIR)$(datadir)/aclocal/gmime.m4
