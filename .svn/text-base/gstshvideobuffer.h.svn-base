#ifndef GSTSHVIDEOBUFFER_H
#define GSTSHVIDEOBUFFER_H

#include <gst/gst.h>

#define GST_TYPE_SHVIDEOBUFFER (gst_shvideobuffer_get_type())
#define GST_IS_SHVIDEOBUFFER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_SHVIDEOBUFFER))
#define GST_SHVIDEOBUFFER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_SHVIDEOBUFFER, Gstshvideobuffer))
#define GST_SHVIDEOBUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_SHVIDEOBUFFER, Gstshvideobufferclass))
#define GST_SHVIDEOBUFFER_CAST(obj)  ((Gstshvideobuffer *)(obj))

typedef struct _Gstshvideobuffer Gstshvideobuffer;
typedef struct _Gstshvideobufferclass Gstshvideobufferclass;

/**
 * Gstshvideobuffer:
 * @y_data: HW data pointer to the y -data
 * @y_size: size of the Y -data
 * @c_data: HW data pointer to the U/V -data
 * @c_size: size of the U/V -data
 *
 * Subclass of #GstBuffer containing additional 
 * data pointers and sizes for Y and U/V -image data.
 */
struct _Gstshvideobuffer {
  GstBuffer buffer;

  guint8   *y_data;
  guint    y_size;
  guint8   *c_data;
  guint    c_size;
};

struct _Gstshvideobufferclass
{
  GstBufferClass parent;
};

/**
 * GST_SHVIDEOBUFFER_Y_DATA:
 * @buf: a #Gstshvideobuffer.
 *
 * A pointer to the y_data element of this buffer.
 */
#define GST_SHVIDEOBUFFER_Y_DATA(buf)  (GST_SHVIDEOBUFFER_CAST(buf)->y_data)
/**
 * GST_SHVIDEOBUFFER_Y_SIZE:
 * @buf: a #Gstshvideobuffer.
 *
 * The size in bytes of the y_data in this buffer.
 */
#define GST_SHVIDEOBUFFER_Y_SIZE(buf)  (GST_SHVIDEOBUFFER_CAST(buf)->y_size)
/**
 * GST_SHVIDEOBUFFER_C_DATA:
 * @buf: a #Gstshvideobuffer.
 *
 * A pointer to the c_data element of this buffer.
 */
#define GST_SHVIDEOBUFFER_C_DATA(buf)  (GST_SHVIDEOBUFFER_CAST(buf)->c_data)
/**
 * GST_SHVIDEOBUFFER_C_SIZE:
 * @buf: a #Gstshvideobuffer.
 *
 * The size in bytes of the c_data in this buffer.
 */
#define GST_SHVIDEOBUFFER_C_SIZE(buf)  (GST_SHVIDEOBUFFER_CAST(buf)->c_size)

GType gst_shvideobuffer_get_type (void);

#endif //GSTSHVIDEOBUFFER_H
