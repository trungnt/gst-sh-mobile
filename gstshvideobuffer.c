#include "gstshvideobuffer.h"

static GstBufferClass *parent_class;

static void
gst_shvideobuffer_init (Gstshvideobuffer * shbuffer, gpointer g_class)
{
  GST_SHVIDEOBUFFER_Y_DATA(shbuffer) = NULL;
  GST_SHVIDEOBUFFER_Y_SIZE(shbuffer) = 0;
  GST_SHVIDEOBUFFER_C_DATA(shbuffer) = NULL;
  GST_SHVIDEOBUFFER_C_SIZE(shbuffer) = 0;
}

static void
gst_shvideobuffer_finalize (Gstshvideobuffer * shbuffer)
{
  /* Set malloc_data to NULL to prevent parent class finalize
     from trying to free allocated data. This buffer is used only in HW
     address space, where we don't allocate data but just map it. */
  GST_BUFFER_MALLOCDATA(shbuffer) = NULL;
  GST_BUFFER_DATA(shbuffer) = NULL;
  GST_SHVIDEOBUFFER_C_DATA(shbuffer) = NULL;
  GST_SHVIDEOBUFFER_C_SIZE(shbuffer) = 0;

  GST_MINI_OBJECT_CLASS (parent_class)->finalize (GST_MINI_OBJECT (shbuffer));
}

static void
gst_shvideobuffer_class_init (gpointer g_class, gpointer class_data)
{
  GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

  parent_class = g_type_class_peek_parent (g_class);

  mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
      gst_shvideobuffer_finalize;
}

GType
gst_shvideobuffer_get_type (void)
{
  static GType gst_shvideobuffer_type;

  if (G_UNLIKELY (gst_shvideobuffer_type == 0)) {
    static const GTypeInfo gst_shvideobuffer_info = {
      sizeof (GstBufferClass),
      NULL,
      NULL,
      gst_shvideobuffer_class_init,
      NULL,
      NULL,
      sizeof (Gstshvideobuffer),
      0,
      (GInstanceInitFunc) gst_shvideobuffer_init,
      NULL
    };
    gst_shvideobuffer_type = g_type_register_static (GST_TYPE_BUFFER,
        "Gstshvideobuffer", &gst_shvideobuffer_info, 0);
  }
  return gst_shvideobuffer_type;
}
