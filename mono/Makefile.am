API = gmime-api.xml
SNK = gmime-sharp.snk
RAW_API = gmime-api.raw
METADATA = GMime.metadata
ASSEMBLY_NAME = gmime-sharp
references = $(GLIB_SHARP_LIBS)
SOURCES_XML = sources.xml

PACKAGE_SHARP = $(PACKAGE)-sharp-$(GMIME_API_VERSION)
ASSEMBLY = $(ASSEMBLY_NAME).dll
TARGET = $(ASSEMBLY) $(ASSEMBLY).config
TARGET_API = $(API)
noinst_DATA = $(API) $(ASSEMBLY) $(ASSEMBLY).config
CLEANFILES = $(ASSEMBLY) generated-stamp generated/*.cs $(API)
DISTCLEANFILES = $(ASSEMBLY).config AssemblyInfo.cs gmime-sharp-$(GMIME_API_VERSION).pc
GACUTIL_DESTDIR_FLAGS = -package $(PACKAGE_SHARP) -root $(DESTDIR)$(gacdir)
GACUTIL_FLAGS = -package $(PACKAGE_SHARP) -gacdir $(gacdir)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(PACKAGE_SHARP).pc
gapidir = $(datadir)/gapi-2.0
gapi_DATA = $(TARGET_API)

sources = 					\
	StreamWrapper.cs			\
	HeaderEnumerator.cs			\
	Header.cs

build_sources =  				\
	$(srcdir)/StreamWrapper.cs		\
	$(srcdir)/HeaderEnumerator.cs		\
	$(srcdir)/Header.cs			\
	AssemblyInfo.cs

customs = 					\
	CertificateCollection.custom		\
	DataWrapper.custom			\
	Global.custom				\
	HeaderList.custom			\
	InternetAddress.custom			\
	InternetAddressList.custom		\
	Message.custom				\
	Multipart.custom			\
	Object.custom				\
	SignatureCollection.custom		\
	Stream.custom

build_customs = 				\
	$(srcdir)/CertificateCollection.custom	\
	$(srcdir)/DataWrapper.custom		\
	$(srcdir)/Global.custom			\
	$(srcdir)/HeaderList.custom		\
	$(srcdir)/InternetAddress.custom	\
	$(srcdir)/InternetAddressList.custom	\
	$(srcdir)/Message.custom		\
	$(srcdir)/Multipart.custom		\
	$(srcdir)/Object.custom			\
	$(srcdir)/SignatureCollection.custom	\
	$(srcdir)/Stream.custom

EXTRA_DIST =					\
	$(ASSEMBLY).config.in			\
	AssemblyInfo.cs.in			\
	$(SNK)					\
	$(METADATA)				\
	$(RAW_API)				\
	$(customs)				\
	$(sources)				\
	$(SOURCES_XML)				\
	gmime-sharp.pc.in

$(PACKAGE_SHARP).pc: gmime-sharp.pc
	-cp gmime-sharp.pc $(PACKAGE_SHARP).pc

$(API): $(srcdir)/$(RAW_API) $(srcdir)/$(METADATA)
	cp $(srcdir)/$(RAW_API) $(API)
	chmod u+w $(API)
	$(GAPI_FIXUP) --api=$(API) --metadata=$(srcdir)/$(METADATA)

generated-stamp: $(API) $(build_customs)
	rm -f generated/* &&	\
	$(GAPI_CODEGEN) --generate $(API) --outdir=generated --customdir=$(srcdir) --assembly-name=$(ASSEMBLY_NAME) \
	&& touch generated-stamp

if PLATFORM_WIN32
GAPI_CDECL_INSERT=$(top_srcdir)/gapi-cdecl-insert --keyfile=$(SNK) $(ASSEMBLY)
else
GAPI_CDECL_INSERT=
endif

$(ASSEMBLY): $(build_sources) generated-stamp $(srcdir)/$(ASSEMBLY_NAME).snk
	$(CSC) -unsafe /out:$(ASSEMBLY) /target:library $(references) \
	-keyfile:$(srcdir)/$(ASSEMBLY_NAME).snk $(build_sources) generated/*.cs
	$(GAPI_CDECL_INSERT)
	sn -q -R $(ASSEMBLY) $(srcdir)/$(SNK)

install-data-local:
	@if test -n '$(TARGET)'; then						\
	  if test -n '$(DESTDIR)'; then						\
	    echo "$(GACUTIL) -i $(ASSEMBLY) -f $(GACUTIL_DESTDIR_FLAGS)";	\
	    $(GACUTIL) -i $(ASSEMBLY) -f $(GACUTIL_DESTDIR_FLAGS) || exit 1;	\
	  else									\
	    echo "$(GACUTIL) -i $(ASSEMBLY) -f $(GACUTIL_FLAGS)";		\
	    $(GACUTIL) -i $(ASSEMBLY) -f $(GACUTIL_FLAGS) || exit 1;		\
	  fi;									\
	fi

uninstall-local:
	@if test -n '$(TARGET)'; then						\
	  if test -n '$(DESTDIR)'; then						\
	    echo "$(GACUTIL) -u $(ASSEMBLY_NAME) $(GACUTIL_DESTDIR_FLAGS)";	\
	    $(GACUTIL) -u $(ASSEMBLY_NAME) $(GACUTIL_DESTDIR_FLAGS) || exit 1;	\
	  else									\
	    echo "$(GACUTIL) -u $(ASSEMBLY_NAME) $(GACUTIL_FLAGS)";		\
	    $(GACUTIL) -u $(ASSEMBLY_NAME) $(GACUTIL_FLAGS) || exit 1;		\
	  fi;									\
	fi

parse:
	$(GAPI_PARSER) $(SOURCES_XML)
