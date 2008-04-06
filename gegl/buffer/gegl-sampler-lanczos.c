/* This file is part of GEGL
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
 * License along with GEGL; if not, see <http://www.gnu.org/licenses/>.
 *
 */

/* XXX WARNING: This code compiles, but is functionally broken, and
 * currently not used by the rest of GeglBuffer */

#include "gegl-sampler-lanczos.h"
#include "gegl-buffer-private.h" /* XXX */
#include <string.h>
#include <math.h>

enum
{
  PROP_0,
  PROP_LANCZOS_WIDTH,
  PROP_LANCZOS_SAMPLES,
  PROP_LAST
};

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GeglSamplerLanczos *self         = GEGL_SAMPLER_LANCZOS (object);

  switch (prop_id)
    {
      case PROP_LANCZOS_WIDTH:
        g_value_set_int (value, self->lanczos_width);
        break;

      case PROP_LANCZOS_SAMPLES:
        g_value_set_int (value, self->lanczos_spp);
        break;

      default:
        break;
    }
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GeglSamplerLanczos *self         = GEGL_SAMPLER_LANCZOS (object);

  switch (prop_id)
    {
      case PROP_LANCZOS_WIDTH:
        self->lanczos_width = g_value_get_int (value);
        break;

      case PROP_LANCZOS_SAMPLES:
        self->lanczos_spp = g_value_get_int (value);
        break;

      default:
        break;
    }
}

static void    gegl_sampler_lanczos_get (GeglSampler *self,
                                              gdouble           x,
                                              gdouble           y,
                                              void             *output);

static void    gegl_sampler_lanczos_prepare (GeglSampler *self);

static inline gdouble sinc (gdouble x);
static void           lanczos_lookup (GeglSampler *sampler);

G_DEFINE_TYPE (GeglSamplerLanczos, gegl_sampler_lanczos, GEGL_TYPE_SAMPLER)

static void
finalize (GObject *object)
{
  GeglSamplerLanczos *self         = GEGL_SAMPLER_LANCZOS (object);
  GeglSampler        *sampler = GEGL_SAMPLER (object);

  g_free (self->lanczos_lookup);
  g_free (sampler->cache_buffer);
  G_OBJECT_CLASS (gegl_sampler_lanczos_parent_class)->finalize (object);
}

static void
gegl_sampler_lanczos_class_init (GeglSamplerLanczosClass *klass)
{
  GObjectClass          *object_class       = G_OBJECT_CLASS (klass);
  GeglSamplerClass *sampler_class = GEGL_SAMPLER_CLASS (klass);

  object_class->finalize     = finalize;
  object_class->set_property = set_property;
  object_class->get_property = get_property;

  sampler_class->prepare = gegl_sampler_lanczos_prepare;
  sampler_class->get     = gegl_sampler_lanczos_get;

  g_object_class_install_property (object_class, PROP_LANCZOS_WIDTH,
                                   g_param_spec_int ("lanczos_width",
                                                     "lanczos_width",
                                                     "Width of the lanczos filter",
                                                     3,
                                                     21,
                                                     3,
                                                     G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_LANCZOS_SAMPLES,
                                   g_param_spec_int ("lanczos_spp",
                                                     "lanczos_spp",
                                                     "Sampels per pixels",
                                                     4000,
                                                     10000,
                                                     4000,
                                                     G_PARAM_CONSTRUCT | G_PARAM_READWRITE));

}

static void
gegl_sampler_lanczos_init (GeglSamplerLanczos *self)
{
}

void
gegl_sampler_lanczos_prepare (GeglSampler *sampler)
{
  /*GeglBuffer *input = GEGL_BUFFER (sampler->input);*/

  /* calculate lookup */
  lanczos_lookup (sampler);
}

void
gegl_sampler_lanczos_get (GeglSampler *sampler,
                          gdouble           x,
                          gdouble           y,
                          void             *output)
{
  GeglSamplerLanczos *self   = GEGL_SAMPLER_LANCZOS (sampler);
  GeglBuffer              *buffer  = sampler->buffer;
  gfloat                  *cache_buffer;
  gfloat                  *buf_ptr;

  gdouble                  x_sum, y_sum, arecip;
  gdouble                  newval[4];

  gfloat                   dst[4];
  gfloat                   abyss = 0.;
  gint                     i, j, pos, pu, pv;
  gint                     lanczos_spp    = self->lanczos_spp;
  gint                     lanczos_width  = self->lanczos_width;
  gint                     lanczos_width2 = lanczos_width * 2 + 1;

  gdouble                  x_kernel[lanczos_width2], /* 1-D kernels of Lanczos window coeffs */
                           y_kernel[lanczos_width2];
  
  gegl_sampler_fill_buffer (sampler, x, y);
  cache_buffer = sampler->cache_buffer;
  if (!cache_buffer)
    return;

  if (x >= 0 &&
      y >= 0 &&
      x < buffer->extent.width &&
      y < buffer->extent.height)
    {
      gint u = (gint) x;
      gint v = (gint) y;
      /* get weight for fractional error */
      gint su = (gint) ((x - u) * lanczos_spp + 0.5);
      gint sv = (gint) ((y - v) * lanczos_spp + 0.5);
      /* fill 1D kernels */
      for (x_sum = y_sum = 0.0, i = lanczos_width; i >= -lanczos_width; i--)
        {
          pos    = i * lanczos_spp;
          x_sum += x_kernel[lanczos_width + i] = self->lanczos_lookup[ABS (su - pos)];
          y_sum += y_kernel[lanczos_width + i] = self->lanczos_lookup[ABS (sv - pos)];
        }

      /* normalise the weighted arrays */
      for (i = 0; i < lanczos_width2; i++)
        {
          x_kernel[i] /= x_sum;
          y_kernel[i] /= y_sum;
        }

      newval[0] = newval[1] = newval[2] = newval[3] = 0.0;
      for (j = 0; j < lanczos_width2; j++)
        for (i = 0; i < lanczos_width2; i++)
          {
            pu         = CLAMP (u + i - lanczos_width, 0, buffer->extent.width - 1);
            pv         = CLAMP (v + j - lanczos_width, 0, buffer->extent.height - 1);
            buf_ptr    = cache_buffer + ((pv * buffer->extent.width + pu) * 4);
            newval[0] += y_kernel[j] * x_kernel[i] * buf_ptr[0] * buf_ptr[3];
            newval[1] += y_kernel[j] * x_kernel[i] * buf_ptr[1] * buf_ptr[3];
            newval[2] += y_kernel[j] * x_kernel[i] * buf_ptr[2] * buf_ptr[3];
            newval[3] += y_kernel[j] * x_kernel[i] * buf_ptr[3];
          }
      if (newval[3] <= 0.0)
        {
          arecip    = 0.0;
          newval[3] = 0;
        }
      else if (newval[3] > G_MAXDOUBLE)
        {
          arecip    = 1.0 / newval[3];
          newval[3] = G_MAXDOUBLE;
        }
      else
        {
          arecip = 1.0 / newval[3];
        }

      dst[0] = CLAMP (newval[0] * arecip, 0, G_MAXDOUBLE);
      dst[1] = CLAMP (newval[1] * arecip, 0, G_MAXDOUBLE);
      dst[2] = CLAMP (newval[2] * arecip, 0, G_MAXDOUBLE);
      dst[3] = CLAMP (newval[3], 0, G_MAXDOUBLE);
    }
  else
    {
      dst[0] = abyss;
      dst[1] = abyss;
      dst[2] = abyss;
      dst[3] = abyss;
    }
  babl_process (babl_fish (sampler->interpolate_format, sampler->format),
                dst, output, 1);
}



/* Internal lanczos */

static inline gdouble
sinc (gdouble x)
{
  gdouble y = x * G_PI;

  if (ABS (x) < 0.0001)
    return 1.0;

  return sin (y) / y;
}

static void
lanczos_lookup (GeglSampler *sampler)
{
  GeglSamplerLanczos *self = GEGL_SAMPLER_LANCZOS (sampler);

  const gint    lanczos_width = self->lanczos_width;
  const gint    samples       = (self->lanczos_spp * (lanczos_width + 1));
  const gdouble dx            = (gdouble) lanczos_width / (gdouble) (samples - 1);

  gdouble x = 0.0;
  gint    i;

  if (self->lanczos_lookup != NULL)
    g_free (self->lanczos_lookup);

  self->lanczos_lookup = g_new (gfloat, samples);

  for (i = 0; i < samples; i++)
    {
      self->lanczos_lookup[i] = ((ABS (x) < lanczos_width) ?
                                 (sinc (x) * sinc (x / lanczos_width)) : 0.0);
      x += dx;
    }
}
