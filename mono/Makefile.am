API=gmime-api.xml
RAW_API = gmime-api.raw
METADATA = GMime.metadata
ASSEMBLY_NAME = gmime-sharp
references = $(GTK_SHARP_LIBS)
XML_SOURCES = sources.xml

PACKAGE_SHARP = $(PACKAGE)-sharp
TARGET = $(ASSEMBLY)
ASSEMBLY = $(ASSEMBLY_NAME).dll
gapidir = $(datadir)/gapi
noinst_DATA = $(ASSEMBLY) $(ASSEMBLY).config
gapi_DATA = $(API)
CLEANFILES = $(ASSEMBLY) generated-stamp generated/*.cs $(API)
DISTCLEANFILES = $(ASSEMBLY).config AssemblyInfo.cs

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gmime-sharp.pc

sources = 				\
	#

build_sources = $(addprefix $(srcdir)/, $(sources)) AssemblyInfo.cs

customs = 				\
	Global.custom			\
	Message.custom			\
	#

build_customs = $(addprefix $(srcdir)/, $(customs))

EXTRA_DIST =			\
	$(ASSEMBLY).config.in	\
	AssemblyInfo.cs.in	\
	$(RAW_API)		\
	$(METADATA)		\
	$(customs)		\
	$(sources)		\
	$(XML_SOURCES)		\
	gmime-sharp.pc.in

$(API): $(srcdir)/$(RAW_API) $(srcdir)/$(METADATA)
	cp $(srcdir)/$(RAW_API) $(API)
	chmod u+w $(API)
	$(GAPI_FIXUP) --api=$(API) --metadata=$(srcdir)/$(METADATA)

generated-stamp: $(API) $(build_customs)
	rm -f generated/* &&	\
	$(GAPI_CODEGEN) --generate $(API) --outdir=generated --customdir=$(srcdir) --assembly-name=$(ASSEMBLY_NAME) 	\
	&& touch generated-stamp

$(ASSEMBLY): $(build_sources) generated-stamp $(ASSEMBLY_NAME).pub
	$(CSC) --unsafe /out:$(ASSEMBLY) /target:library $(references) $(build_sources) generated/*.cs

install-data-local:
	@if test -n '$(TARGET)'; then									\
	  if test -n '$(DESTDIR)'; then								\
	    echo "$(GACUTIL) /i $(ASSEMBLY) /f /package $(PACKAGE_SHARP) /root $(DESTDIR)$(libdir)";		\
	    $(GACUTIL) /i $(ASSEMBLY) /f /package $(PACKAGE_SHARP) /root $(DESTDIR)$(libdir) || exit 1;	\
	  else												\
	    echo "$(GACUTIL) /i $(ASSEMBLY) /f /package $(PACKAGE_SHARP) /gacdir $(libdir)";			\
	    $(GACUTIL) /i $(ASSEMBLY) /f /package $(PACKAGE_SHARP) /gacdir $(libdir) || exit 1;		\
	  fi;												\
	fi

uninstall-local:
	@if test -n '$(TARGET)'; then									\
	  if test -n '$(DESTDIR)'; then								\
	    echo "$(GACUTIL) /u $(ASSEMBLY_NAME) /package $(PACKAGE_SHARP) /root $(DESTDIR)$(libdir)";	\
	    $(GACUTIL) /u $(ASSEMBLY_NAME) /package $(PACKAGE_SHARP) /root $(DESTDIR)$(libdir) || exit 1;  \
	  else												\
	    echo "$(GACUTIL) /u $(ASSEMBLY_NAME) /package $(PACKAGE_SHARP) /gacdir $(libdir)";	\
	    $(GACUTIL) /u $(ASSEMBLY_NAME) /package $(PACKAGE_SHARP) /gacdir $(libdir) || exit 1;	\
	  fi;												\
	fi

parse:
	gapi-parser $(XML_SOURCES)
