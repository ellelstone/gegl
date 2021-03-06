/* This file is an image processing operation for GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 */

#include "config.h"
#include <glib/gi18n-lib.h>


#ifdef GEGL_PROPERTIES

property_double (dampness, _("Dampness"), 0.95)
    description (_("The value represents the contribution of the past to the new frame."))
    value_range (0.0, 1.0)

#else

#define GEGL_OP_FILTER
#define GEGL_OP_NAME     mblur
#define GEGL_OP_C_SOURCE mblur.c

#include "gegl-op.h"

typedef struct
{
  GeglBuffer *acc;
} Priv;


static void
init (GeglProperties *o, const Babl *format)
{
  Priv         *priv = (Priv*)o->user_data;
  GeglRectangle extent = {0,0,1024,1024};

  g_assert (priv == NULL);

  priv = g_new0 (Priv, 1);
  o->user_data = (void*) priv;

  priv->acc = gegl_buffer_new (&extent, format);
}

static void prepare (GeglOperation *operation)
{
  const Babl *space = gegl_operation_get_source_space (operation, "input");
  gegl_operation_set_format (operation, "output", babl_format_with_space ("RGBA float", space));
}

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result,
         gint                 level)
{
  GeglProperties *o;
  Priv           *p;
  const Babl *format = gegl_operation_get_format (operation, "output");

  o = GEGL_PROPERTIES (operation);
  p = (Priv*)o->user_data;
  if (p == NULL)
    init (o, format);
  p = (Priv*)o->user_data;

    {
      GeglBuffer *temp_in;

      if (gegl_rectangle_equal (result, gegl_buffer_get_extent (input)))
        temp_in = g_object_ref (input);
      else
        temp_in = gegl_buffer_create_sub_buffer (input, result);

      {
        gint pixels = result->width * result->height;
        gfloat *buf = g_new (gfloat, pixels * 4);
        gfloat *acc = g_new (gfloat, pixels * 4);
        gfloat dampness;
        gint i;
        gegl_buffer_get (p->acc, result, 1.0, format, acc, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
        gegl_buffer_get (temp_in, result, 1.0, format, buf, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
        dampness = o->dampness;
        for (i=0;i<pixels;i++)
          {
            gint c;
            for (c=0;c<4;c++)
              acc[i*4+c]=acc[i*4+c]*dampness + buf[i*4+c]*(1.0-dampness);
          }
        gegl_buffer_set (p->acc, result, 0, format, acc, GEGL_AUTO_ROWSTRIDE);
        gegl_buffer_set (output, result, 0, format, acc, GEGL_AUTO_ROWSTRIDE);
        g_free (buf);
        g_free (acc);
      }
      g_object_unref (temp_in);
    }

  return  TRUE;
}

static void
finalize (GObject *object)
{
  GeglProperties *o = GEGL_PROPERTIES (object);

  if (o->user_data)
    {
      Priv *p = (Priv*)o->user_data;

      g_object_unref (p->acc);
      g_clear_pointer (&o->user_data, g_free);
    }

  G_OBJECT_CLASS (gegl_op_parent_class)->finalize (object);
}


static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  G_OBJECT_CLASS (klass)->finalize = finalize;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  filter_class->process = process;
  operation_class->prepare = prepare;
  operation_class->threaded = FALSE;

  gegl_operation_class_set_keys (operation_class,
    "name",        "gegl:mblur",
    "title",       _("Temporal blur"),
    "categories" , "blur:video",
    "description", _("Accumulating motion blur using a kalman filter, for use with video sequences of frames."),
    NULL);
}

#endif
