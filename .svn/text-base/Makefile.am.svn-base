plugin_LTLIBRARIES = libgstshvideo.la

EXTRA_DIST = \
	depcomp autogen.sh 

ACLOCAL_AMFLAGS = -I common/m4

libgstshvideo_la_SOURCES = gstshvideodec.c gstshvideoenc.c gstshvideosink.c \
	cntlfile/ControlFileUtil.c gstshvideoplugin.c gstshioutils.c gstshvideobuffer.c

libgstshvideo_la_CFLAGS = $(GST_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) \
	$(LIBSHCODECS_CFLAGS)
libgstshvideo_la_LIBADD = $(GST_BASE_LIBS) $(GST_PLUGINS_BASE_LIBS) \
        $(LIBSHCODECS_LIBS)
libgstshvideo_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -O2 -lrt \
	-lgstvideo-0.10 -lz -lstdc++ -lgstinterfaces-0.10 -lshcodecs
libgstshvideo_la_LIBTOOLFLAGS = --tag=disable-static

noinst_HEADERS = 

check-valgrind:
	@true

check-torture:
	@true
