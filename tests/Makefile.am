## Process this file with automake to produce Makefile.in

SUBDIRS = .

INCLUDES = 				\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/util		\
	$(VERSION_FLAGS) 		\
	-DG_LOG_DOMAIN=\"gmime-tests\"	\
	-DG_DISABLE_DEPRECATED		\
	$(GLIB_CFLAGS)

VERSION_FLAGS = -DGMIME_VERSION=\"$(GMIME_VERSION)\" 		\
		-DGMIME_MAJOR_VERSION=$(GMIME_MAJOR_VERSION)	\
		-DGMIME_MINOR_VERSION=$(GMIME_MINOR_VERSION)	\
		-DGMIME_MICRO_VERSION=$(GMIME_MICRO_VERSION)

noinst_PROGRAMS = 	\
	test-best	\
	test-mime 	\
	test-parser 	\
	test-mbox 	\
	test-streams 	\
	test-cat	\
	test-html 	\
	test-iconv 	\
	test-partial	\
	test-pgp	\
	test-pgpmime

DEPS = $(top_builddir)/gmime/libgmime-2.0.la
LDADDS = $(top_builddir)/gmime/libgmime-2.0.la $(GLIB_LIBS)

test_best_SOURCES = test-best.c
test_best_LDFLAGS = 
test_best_DEPENDENCIES = $(DEPS)
test_best_LDADD = $(LDADDS)

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

test_cat_SOURCES = test-cat.c
test_cat_LDFLAGS = 
test_cat_DEPENDENCIES = $(DEPS)
test_cat_LDADD = $(LDADDS)

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

test_pgp_SOURCES = test-pgp.c
test_pgp_LDFLAGS = 
test_pgp_DEPENDENCIES = $(DEPS)
test_pgp_LDADD = $(LDADDS)

test_pgpmime_SOURCES = test-pgpmime.c
test_pgpmime_LDFLAGS = 
test_pgpmime_DEPENDENCIES = $(DEPS)
test_pgpmime_LDADD = $(LDADDS)

EXTRA_DIST = test1.eml test2.eml test3.eml

check-local: tests

testall : tests

tests: MIMEtests PARSERtests STREAMtests CATtests FILTERtests ICONVtests PGPtests

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

CATtests : test-cat
	@echo "##"
	@echo "## StreamCat regression tests"
	@echo "##"

FILTERtests : test-html
	@echo "##"
	@echo "## Filter regression tests"
	@echo "##"

ICONVtests : test-iconv
	@echo "##"
	@echo "## iconv cache regression tests"
	@echo "##"

PGPtests : test-pgp test-pgpmime
	@echo "##"
	@echo "## PGP tests"
	@echo "##"
