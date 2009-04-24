## Process this file with automake to produce Makefile.in

SUBDIRS = .

INCLUDES = -I$(top_srcdir) $(GMIME_CFLAGS) $(GLIB_CFLAGS)

bin_PROGRAMS = uuencode uudecode

DEPS = 						\
	$(top_builddir)/util/libutil.la		\
	$(top_builddir)/gmime/libgmime-$(GMIME_API_VERSION).la

LDADDS = 					\
	$(top_builddir)/util/libutil.la		\
	$(top_builddir)/gmime/libgmime-$(GMIME_API_VERSION).la 	\
	$(GLIB_LIBS)

if SYSTEM_GETOPT
GETOPT_SOURCES =
else
GETOPT_SOURCES = getopt.c getopt.h getopt1.c
endif

uuencode_SOURCES = $(GETOPT_SOURCES) uuencode.c
uuencode_LDFLAGS = 
uuencode_DEPENDENCIES = $(DEPS)
uuencode_LDADD = $(LDADDS)

uudecode_SOURCES = $(GETOPT_SOURCES) uudecode.c
uudecode_LDFLAGS = 
uudecode_DEPENDENCIES = $(DEPS)
uudecode_LDADD = $(LDADDS)
