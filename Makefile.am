## Process this file with automake to produce Makefile.in

SUBDIRS = . tests doc

INCLUDES = -I@srcdir@ $(VERSION_FLAGS) @GLIB_CFLAGS@ -DG_LOG_DOMAIN=\"gmime\"

VERSION_FLAGS = -DGMIME_VERSION=\"@GMIME_VERSION@\"

noinst_PROGRAMS = gen-table test-mime test-parser test-mbox test-streams test-html test-iconv test-partial

bin_SCRIPTS = gmime-config

lib_LTLIBRARIES = libgmime-2.0.la

libgmime_2_0_la_LDFLAGS = -version-info @GMIME_VERSION_INFO@

libgmime_2_0_la_SOURCES = 	\
	alloca.c		\
	strlib.c		\
	memchunk.c		\
	gmime.c			\
	gmime-charset.c		\
	gmime-content-type.c	\
	gmime-data-wrapper.c	\
	gmime-disposition.c	\
	gmime-filter.c		\
	gmime-filter-basic.c	\
	gmime-filter-charset.c	\
	gmime-filter-crlf.c	\
	gmime-filter-from.c	\
	gmime-filter-html.c	\
	gmime-filter-yenc.c	\
	gmime-header.c		\
	gmime-iconv.c		\
	gmime-iconv-utils.c	\
	gmime-message.c		\
	gmime-message-part.c	\
	gmime-message-partial.c	\
	gmime-multipart.c	\
	gmime-object.c		\
	gmime-param.c		\
	gmime-parser.c		\
	gmime-part.c		\
	gmime-stream.c		\
	gmime-stream-buffer.c	\
	gmime-stream-cat.c	\
	gmime-stream-file.c	\
	gmime-stream-filter.c	\
	gmime-stream-fs.c	\
	gmime-stream-mem.c	\
	gmime-stream-mmap.c	\
	gmime-stream-null.c	\
	gmime-utils.c		\
	internet-address.c	\
	md5-utils.c

gmimeincludedir = $(includedir)/gmime-2.0
gmimeinclude_HEADERS = 		\
	gmime.h			\
	gmime-charset.h		\
	gmime-content-type.h	\
	gmime-data-wrapper.h	\
	gmime-disposition.h	\
	gmime-filter.h		\
	gmime-filter-basic.h	\
	gmime-filter-charset.h	\
	gmime-filter-crlf.h	\
	gmime-filter-from.h	\
	gmime-filter-html.h	\
	gmime-filter-yenc.h	\
	gmime-header.h		\
	gmime-iconv.h		\
	gmime-iconv-utils.h	\
	gmime-message.h		\
	gmime-message-part.h	\
	gmime-message-partial.h	\
	gmime-multipart.h	\
	gmime-object.h		\
	gmime-param.h		\
	gmime-parser.h		\
	gmime-part.h		\
	gmime-stream.h		\
	gmime-stream-buffer.h	\
	gmime-stream-cat.h	\
	gmime-stream-file.h	\
	gmime-stream-filter.h	\
	gmime-stream-fs.h	\
	gmime-stream-mem.h	\
	gmime-stream-mmap.h	\
	gmime-stream-null.h	\
	gmime-utils.h		\
	internet-address.h

noinst_HEADERS = 			\
	strlib.h			\
	memchunk.h			\
	gmime-table-private.h		\
	gmime-charset-map-private.h	\
	md5-utils.h

DEPS = $(top_builddir)/libgmime-2.0.la
LDADDS = $(top_builddir)/libgmime-2.0.la @GLIB_LIBS@

test_mime_SOURCES = test-mime.c
test_mime_LDFLAGS = 
test_mime_DEPENDENCIES = $(DEPS)
test_mime_LDADD = $(LDADDS)

test_parser_SOURCES = test-parser.c
test_parser_LDFLAGS = 
test_parser_DEPENDENCIES = $(DEPS)
test_parser_LDADD = $(LDADDS)

test_mbox_SOURCES = test-mbox.c
test_mbox_LDFLAGS = 
test_mbox_DEPENDENCIES = $(DEPS)
test_mbox_LDADD = $(LDADDS)

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

test_partial_SOURCES = test-partial.c
test_partial_LDFLAGS = 
test_partial_DEPENDENCIES = $(DEPS)
test_partial_LDADD = $(LDADDS)

gen_table_SOURCES = gen-table.c
gen_table_LDFLAGS = 
gen_table_DEPENDENCIES = 
gen_table_LDADD = 

check-local: tests

testall : tests

tests: MIMEtests PARSERtests STREAMtests FILTERtests ICONVtests

MIMEtests : test-mime
	@echo "##"
	@echo "## MIME regression tests"
	@echo "##"

PARSERtests : test-parser test-mbox test-partial
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

EXTRA_DIST = 			\
	gmimeConf.sh.in 	\
	gmime-2.0.pc.in		\
	gmime.spec.in 		\
	gmime.m4 		\
	zentimer.h 		\
	zenprofiler.h

BUILD_EXTRA_DIST = 		\
	gmime.spec

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gmime-2.0.pc

$(pkgconfig_DATA): config.status

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

$(libgmime_2_0_la_SOURCES): $(srcdir)/libgmime

install-data-local:
	-mkdir -p $(DESTDIR)$(datadir)/aclocal
	$(INSTALL_DATA) $(srcdir)/gmime.m4 $(DESTDIR)$(datadir)/aclocal/gmime.m4
