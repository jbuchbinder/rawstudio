plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-g3

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I$(top_srcdir)/librawstudio/ \
	-I$(top_srcdir)/

lib_LTLIBRARIES = colorspace_transform.la

libdir = $(datadir)/rawstudio/plugins/

colorspace_transform_la_LIBADD = @PACKAGE_LIBS@ colorspace_transform_sse2.lo rs-cmm.lo
colorspace_transform_la_LDFLAGS = -module -avoid-version

colorspace_transform.lo: colorspace_transform.c
	$(LTCOMPILE) -DEXIT_CODE=0 -c colorspace_transform.c

rs-cmm.lo: rs-cmm.c rs-cmm.h
	$(LTCOMPILE) -DEXIT_CODE=2 -c rs-cmm.c

colorspace_transform_sse2.lo: colorspace_transform_sse2.c
if CAN_COMPILE_SSE2
SSE_FLAG=-msse2
else
SSE_FLAG=
endif
	$(LTCOMPILE) $(SSE_FLAG) -DEXIT_CODE=1 -c colorspace_transform_sse2.c
