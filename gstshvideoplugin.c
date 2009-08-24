#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstshvideosink.h"
#include "gstshvideoenc.h"
#include "gstshvideodec.h"

gboolean
gst_shvideo_plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "gst-sh-mobile-sink", GST_RANK_NONE,
          GST_TYPE_SHVIDEOSINK))
    return FALSE;

  if (!gst_element_register (plugin, "gst-sh-mobile-dec", GST_RANK_NONE,
          GST_TYPE_SHVIDEODEC))
    return FALSE;

  if (!gst_element_register (plugin, "gst-sh-mobile-enc", GST_RANK_PRIMARY,
          GST_TYPE_SHVIDEOENC))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "gst-sh-mobile",
    "SH HW video elements",
    gst_shvideo_plugin_init,
    VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
