## Process this file with automake to produce Makefile.in

SUBDIRS = . tests doc

INCLUDES = -I@srcdir@ $(VERSION_FLAGS) @GLIB_CFLAGS@ -DG_LOG_DOMAIN=\"gmime\"

VERSION_FLAGS = -DGMIME_VERSION=\"@GMIME_VERSION@\"

noinst_PROGRAMS = gen-table gen-multibyte test-mime test-parser test-streams test-html test-iconv test-pgp test-pgpmime

bin_SCRIPTS = gmime-config

lib_LTLIBRARIES = libgmime.la

libgmime_la_LDFLAGS = -version-info @GMIME_VERSION_INFO@

libgmime_la_SOURCES = 		\
	alloca.c		\
	strlib.c		\
	memchunk.c		\
	gmime.c			\
	gmime-charset.c		\
	gmime-content-type.c	\
	gmime-data-wrapper.c	\
	gmime-disposition.c	\
	gmime-exception.c	\
	gmime-filter.c		\
	gmime-filter-basic.c	\
	gmime-filter-charset.c	\
	gmime-filter-chomp.c	\
	gmime-filter-crlf.c	\
	gmime-filter-from.c	\
	gmime-filter-html.c	\
	gmime-filter-yenc.c	\
	gmime-header.c		\
	gmime-iconv.c		\
	gmime-iconv-utils.c	\
	gmime-message.c		\
	gmime-object.c		\
	gmime-param.c		\
	gmime-parser.c		\
	gmime-part.c		\
	gmime-stream.c		\
	gmime-stream-buffer.c	\
	gmime-stream-file.c	\
	gmime-stream-filter.c	\
	gmime-stream-fs.c	\
	gmime-stream-mem.c	\
	gmime-stream-mmap.c	\
	gmime-stream-null.c	\
	gmime-utils.c		\
	internet-address.c	\
	md5-utils.c		\
	unicode.c		\
	gmime-cipher-context.c	\
	gmime-pgp-context.c	\
	gmime-pgp-mime.c

gmimeincludedir = $(includedir)/gmime
gmimeinclude_HEADERS = 		\
	gmime.h			\
	gmime-charset.h		\
	gmime-content-type.h	\
	gmime-data-wrapper.h	\
	gmime-disposition.h	\
	gmime-exception.h	\
	gmime-exception-list.def\
	gmime-filter.h		\
	gmime-filter-basic.h	\
	gmime-filter-charset.h	\
	gmime-filter-chomp.h	\
	gmime-filter-crlf.h	\
	gmime-filter-from.h	\
	gmime-filter-html.h	\
	gmime-filter-yenc.h	\
	gmime-header.h		\
	gmime-iconv.h		\
	gmime-iconv-utils.h	\
	gmime-message.h		\
	gmime-object.h		\
	gmime-param.h		\
	gmime-parser.h		\
	gmime-part.h		\
	gmime-stream.h		\
	gmime-stream-buffer.h	\
	gmime-stream-file.h	\
	gmime-stream-filter.h	\
	gmime-stream-fs.h	\
	gmime-stream-mem.h	\
	gmime-stream-mmap.h	\
	gmime-stream-null.h	\
	gmime-utils.h		\
	internet-address.h	\
	gmime-cipher-context.h	\
	gmime-pgp-context.h	\
	gmime-pgp-mime.h

noinst_HEADERS = 			\
	strlib.h			\
	memchunk.h			\
	gmime-table-private.h		\
	gmime-charset-map-private.h	\
	md5-utils.h			\
	unicode.h

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

test_streams_SOURCES = test-streams.c
test_streams_LDFLAGS = 
test_streams_DEPENDENCIES = $(DEPS)
test_streams_LDADD = $(LDADDS)

test_html_SOURCES = test-html.c
test_html_LDFLAGS = 
test_html_DEPENDENCIES = $(DEPS)
test_html_LDADD = $(LDADDS)

test_iconv_SOURCES = test-iconv.c
test_iconv_LDFLAGS = 
test_iconv_DEPENDENCIES = $(DEPS)
test_iconv_LDADD = $(LDADDS)

test_pgp_SOURCES = test-pgp.c
test_pgp_LDFLAGS = 
test_pgp_DEPENDENCIES = $(DEPS)
test_pgp_LDADD = $(LDADDS)

test_pgpmime_SOURCES = test-pgpmime.c
test_pgpmime_LDFLAGS = 
test_pgpmime_DEPENDENCIES = $(DEPS)
test_pgpmime_LDADD = $(LDADDS)

gen_table_SOURCES = gen-table.c
gen_table_LDFLAGS = 
gen_table_DEPENDENCIES = 
gen_table_LDADD = 

gen_multibyte_SOURCES = gen-multibyte.c
gen_multibyte_LDFLAGS = 
gen_multibyte_DEPENDENCIES = 
gen_multibyte_LDADD = 

check-local: tests

testall : tests

tests: MIMEtests PARSERtests STREAMtests FILTERtests ICONVtests PGPtests PGPMIMEtests

MIMEtests : test-mime
	@echo "##"
	@echo "## MIME regression tests"
	@echo "##"

PARSERtests : test-parser
	@echo "##"
	@echo "## MIME Parser regression tests"
	@echo "##"

STREAMtests : test-streams
	@echo "##"
	@echo "## Stream regression tests"
	@echo "##"

FILTERtests : test-html
	@echo "##"
	@echo "## Filter regression tests"
	@echo "##"

ICONVtests : test-iconv
	@echo "##"
	@echo "## iconv cache regression tests"
	@echo "##"

PGPtests : test-pgp
	@echo "##"
	@echo "## PGP tests"
	@echo "##"

PGPMIMEtests : test-pgpmime
	@echo "##"
	@echo "## PGP/MIME tests"
	@echo "##"

dist-hook: gmime.spec
	-cp gmime.spec $(distdir)
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
EXTRA_DIST = iconv-detect.c gmimeConf.sh.in gmime.spec.in gmime.spec gmime.m4 zentimer.h zenprofiler.h

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
