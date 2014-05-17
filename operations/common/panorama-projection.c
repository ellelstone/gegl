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
 * License along with GEGL; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2014 Øyvind Kolås <pippin@gimp.org>
 */

#include <math.h>

#ifdef GEGL_PROPERTIES

gegl_property_double (pan, "nick", _("Pan"),
    "blurb", _("Horizontal camera panning"),
    "min",  -180.0, "max", 360.0,
    "unit", "degree",
    NULL)

gegl_property_double (tilt, "nick", _("Tilt"),
    "blurb", _("Vertical angle"),
    "min", -180.0, "max",  180.0,
    "unit", "degree",
    NULL)

gegl_property_double (spin, "nick", _("Spin"),
    "blurb", _("Spin angle around camera axis"),
    "min",  -360.0, "max",   360.0,
    NULL)

gegl_property_double (zoom, "nick", _("Zoom"),
    "blurb",   _("Zoom level"),
    "default", 100.0, "min", 0.01, "max", 1000.0,
    NULL)

gegl_property_int (width, "nick", _("Width"),
    "blurb",   _("output/rendering width in pixels, -1 for input width"),
    "default", -1, "min", -1, "max", 10000,
    "role", "output-extent",
    "axis", "x",
    NULL)

gegl_property_int (height, "nick", _("Height"),
    "blurb",   _("output/rendering height in pixels, -1 for input height"),
    "default", -1, "min", -1, "max", 10000,
    "role", "output-extent",
    "axis", "y",
    NULL)

gegl_property_boolean (little_planet, "nick", _("Little planet"),
    "blurb",  _("use the pan/tilt location as center for a stereographic/little planet projection."),
    NULL)

gegl_property_enum (sampler_type, GeglSamplerType, gegl_sampler_type,
    "nick",    _("Resampling method"),
    "blurb",   _("Image resampling method to use"),
    "default", GEGL_SAMPLER_NEAREST,
    NULL)

#else

#define GEGL_OP_FILTER
#define GEGL_OP_C_FILE "panorama-projection.c"

#include "config.h"
#include <glib/gi18n-lib.h>
#include "gegl-op.h"

/*#include "speedmath.inc"*/

typedef struct _Transform Transform;
struct _Transform
{
  float pan;
  float tilt;
  float sin_tilt;
  float cos_tilt;
  float sin_spin;
  float cos_spin;
  float sin_negspin;
  float cos_negspin;
  float zoom;
  float spin;
  float xoffset;
  float width;
  float height;
  void (*xy2ll) (Transform *transform, float x, float  y, float *lon, float *lat);
  void (*ll2xy) (Transform *transform, float lon, float  lat, float *x, float *y);
  int   do_spin;
  int   do_zoom;
};

/* formulas from:
 * http://mathworld.wolfram.com/GnomonicProjection.html
 */
static void inline
gnomonic_xy2ll (Transform *transform, float x, float y,
                float *lon, float *lat)
{
  float p, c;
  float longtitude, latitude;
  float sin_c, cos_c;

  if (transform->do_spin)
  {
    float tx = x, ty = y;
    x = tx * transform->cos_spin - ty * transform->sin_spin;
    y = ty * transform->cos_spin + tx * transform->sin_spin;
  }

  if (transform->do_zoom)
  {
    x /= transform->zoom;
    y /= transform->zoom;
  }

  p = sqrtf (x*x+y*y);
  c = atan2f (p, 1);

  sin_c = sinf(c);
  cos_c = cosf(c);

  latitude = asinf (cos_c * transform->sin_tilt + ( y * sin_c * transform->cos_tilt) / p);
  longtitude = transform->pan + atan2f (x * sin_c, p * transform->cos_tilt * cos_c - y * transform->sin_tilt * sin_c);

  if (longtitude < 0)
    longtitude += M_PI * 2;

  *lon = (longtitude / (M_PI * 2));
  *lat = ((latitude + M_PI/2) / M_PI);
}

static void inline
gnomonic_ll2xy (Transform *transform,
                float lon, float lat,
                float *x, float *y)
{
  float cos_c;
  float sin_lon = sinf (lon);
  float cos_lon = cosf (lon);
  float cos_lat_minus_pan = cosf (lat - transform->pan);

  cos_c = (transform->sin_tilt * sin_lon +
           transform->cos_tilt * cos (lon) *
            cos_lat_minus_pan);

  *x = ((cos_lon * sin (lat - transform->pan)) / cos_c);
  *y = ((transform->cos_tilt * sin_lon -
         transform->sin_tilt * cos_lon * cos_lat_minus_pan) / cos_c);

  if (transform->do_zoom)
  {
    *x *= transform->zoom;
    *y *= transform->zoom;
  }
  if (transform->do_spin)
  {
    float tx = *x, ty = *y;
    *x = tx * transform->cos_negspin - ty * transform->sin_negspin;
    *y = ty * transform->cos_negspin + tx * transform->sin_negspin;
  }
}

static void inline
stereographic_ll2xy (Transform *transform,
                     float lon, float lat,
                     float *x,  float *y)
{
  float k;
  float sin_lon = sinf (lon);
  float cos_lon = cosf (lon);
  float cos_lat_minus_pan = cosf (lat - transform->pan);

  k = 2/(1 + transform->sin_tilt * sin_lon +
            transform->cos_tilt * cos (lon) *
            cos_lat_minus_pan);

  *x = k * ((cos_lon * sin (lat - transform->pan)));
  *y = k * ((transform->cos_tilt * sin_lon -
        transform->sin_tilt * cos_lon * cos_lat_minus_pan));

  if (transform->do_zoom)
  {
    *x *= transform->zoom;
    *y *= transform->zoom;
  }

  if (transform->do_spin)
  {
    float tx = *x, ty = *y;
    *x = tx * transform->cos_negspin - ty * transform->sin_negspin;
    *y = ty * transform->cos_negspin + tx * transform->sin_negspin;
  }
}

static void inline
stereographic_xy2ll (Transform *transform,
                     float x, float y,
                     float *lon, float *lat)
{
  float p, c;
  float longtitude, latitude;
  float sin_c, cos_c;

  if (transform->do_spin)
  {
    float tx = x, ty = y;
    x = tx * transform->cos_spin - ty * transform->sin_spin;
    y = ty * transform->cos_spin + tx * transform->sin_spin;
  }

  if (transform->do_zoom)
  {
    x /= transform->zoom;
    y /= transform->zoom;
  }

  p = sqrtf (x*x+y*y);
  c = 2 * atan2f (p / 2, 1);

  sin_c = sinf (c);
  cos_c = cosf (c);

  latitude = asinf (cos_c * transform->sin_tilt + ( y * sin_c * transform->cos_tilt) / p);
  longtitude = transform->pan + atan2f ( x * sin_c, p * transform->cos_tilt * cos_c - y * transform->sin_tilt * sin_c);

  if (longtitude < 0)
    longtitude += M_PI * 2;

  *lon = (longtitude / (M_PI * 2));
  *lat = ((latitude + M_PI/2) / M_PI);
}

static void prepare_transform (Transform *transform,
                               float pan, float spin, float zoom, float tilt,
                               int little_planet,
                               float width, float height,
                               float input_width, float input_height)
{
  float xoffset = 0.5;
  transform->xy2ll = gnomonic_xy2ll;
  transform->ll2xy = gnomonic_ll2xy;

  pan  = pan / 360 * M_PI * 2;
  spin = spin / 360 * M_PI * 2;
  zoom = little_planet?zoom / 1000.0:zoom / 100.0;
  tilt = tilt / 360 * M_PI * 2;

  while (pan > M_PI)
    pan -= 2 * M_PI;

  if (width <= 0 || height <= 0)
  {
    width = input_height; 
    height = width;
    xoffset = ((input_width - height)/height) / 2 + 0.5;
  }
  else
  {
    float orig_width = width;
    width = height;
    xoffset = ((orig_width - height)/height) / 2 + 0.5;
  }

  if (little_planet)
  {
    transform->xy2ll = stereographic_xy2ll;
    transform->ll2xy = stereographic_ll2xy;
  }

  transform->do_spin = fabs (spin) > 0.000001 ? 1 : 0;
  transform->do_zoom = fabs (zoom-1.0) > 0.000001 ? 1 : 0;

  transform->pan         = pan;
  transform->tilt        = tilt;
  transform->spin        = spin;
  transform->zoom        = zoom;
  transform->xoffset     = xoffset;
  transform->sin_tilt    = sinf (tilt);
  transform->cos_tilt    = cosf (tilt);
  transform->sin_spin    = sinf (spin);
  transform->cos_spin    = cosf (spin);
  transform->sin_negspin = sinf (-spin);
  transform->cos_negspin = cosf (-spin);
  transform->width       = width;
  transform->height      = height;
}

static void
prepare (GeglOperation *operation)
{
  const Babl *format = babl_format ("RaGaBaA float");

  gegl_operation_set_format (operation, "input", format);
  gegl_operation_set_format (operation, "output", format);
}

static GeglRectangle
get_bounding_box (GeglOperation *operation)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);
  GeglRectangle result = {0,0,0,0};

  if (o->width <= 0 || o->height <= 0)
  {
     GeglRectangle *in_rect;
     in_rect = gegl_operation_source_get_bounding_box (operation, "input");
     if (in_rect)
       {
          result = *in_rect;
       }
     else
     {
       result.width = 320;
       result.height = 200;
     }
  }
  else
  {
    result.width = o->width;
    result.height = o->height;
  }
  return result;
}


static void prepare_transform2 (Transform *transform,
                                GeglOperation *operation)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);
  GeglRectangle in_rect = *gegl_operation_source_get_bounding_box (operation, "input");

  prepare_transform (transform, 
                     o->pan, o->spin, o->zoom, o->tilt,
                     o->little_planet, o->width, o->height,
                     in_rect.width, in_rect.height);
}

static GeglRectangle
get_required_for_output (GeglOperation       *operation,
                         const gchar         *input_pad,
                         const GeglRectangle *region)
{
  GeglRectangle result = *gegl_operation_source_get_bounding_box (operation, "input");
  return result;
}


static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *result,
         gint                 level)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);
  Transform           transform;
  const Babl         *format_io;
  GeglSampler        *sampler;
  GeglBufferIterator *it;
  gint  index_in, index_out;
  GeglRectangle in_rect = *gegl_operation_source_get_bounding_box (operation, "input");
  GeglMatrix2  scale_matrix;
  GeglMatrix2 *scale = NULL;

  prepare_transform2 (&transform, operation);

  format_io = babl_format ("RaGaBaA float");
  sampler = gegl_buffer_sampler_new (input, format_io, o->sampler_type);

  if (o->sampler_type == GEGL_SAMPLER_NOHALO ||
      o->sampler_type == GEGL_SAMPLER_LOHALO)
    scale = &scale_matrix;

    {
      float   ud = ((1.0/transform.width));
      float   vd = ((1.0/transform.height));
      it = gegl_buffer_iterator_new (output, result, level, format_io, GEGL_BUFFER_WRITE, GEGL_ABYSS_NONE);
      index_out = 0;

      while (gegl_buffer_iterator_next (it))
        {
          gint    i;
          gint    n_pixels = it->length;
          gint    x = it->roi->x; /* initial x                   */
          gint    y = it->roi->y; /*           and y coordinates */

          float   u0 = ((x/transform.width) - transform.xoffset);
          float   u, v;

          float *in = it->data[index_in];
          float *out = it->data[index_out];

          u = u0;
          v = ((y/transform.height) - 0.5); 

          if (scale)
            {
              for (i=0; i<n_pixels; i++)
                {
                  float cx, cy;
#define gegl_unmap(xx,yy,ud,vd) { \
                  float rx, ry;\
                  transform.xy2ll (&transform, xx, yy, &rx, &ry);\
                  ud = rx;vd = ry;}
                  gegl_sampler_compute_scale (scale_matrix, u, v);
                  gegl_unmap(u,v, cx, cy);
#undef gegl_unmap

                  gegl_sampler_get (sampler,
                                    cx * in_rect.width, cy * in_rect.height,
                                    scale, out, GEGL_ABYSS_LOOP);
                  in  += 4;
                  out += 4;

                  /* update x, y and u,v coordinates */
                  x++;
                  u+=ud;
                  if (x >= (it->roi->x + it->roi->width))
                    {
                      x = it->roi->x;
                      y++;
                      u = u0;
                      v += vd;
                    }
                }
              }
            else
              {
                for (i=0; i<n_pixels; i++)
                  {
                    float cx, cy;

                    transform.xy2ll (&transform, u, v, &cx, &cy);

                    gegl_sampler_get (sampler,
                                      cx * in_rect.width, cy * in_rect.height,
                                      scale, out, GEGL_ABYSS_LOOP);
                    in  += 4;
                    out += 4;

                    /* update x, y and u,v coordinates */
                    x++;
                    u+=ud;
                    if (x >= (it->roi->x + it->roi->width))
                      {
                        x = it->roi->x;
                        u = u0;
                        y++;
                        v += vd;
                      }
                  }
              }
        }
    }
  g_object_unref (sampler);

#if 0
  {
    float t;
    float lat0  = 0;
    float lon0 = 0;
    float lat1  = 0.5;
    float lon1 = 0.5;
    int i = 0;
    guchar pixel[4] = {255,0,0,255};

    for (t = 0; t < 1.0; t+=0.01, i++)
    {
      float lat = lat0 * (1.0 - t) + lat1 * t;
      float lon = lon0 * (1.0 - t) + lon1 * t;
      float x, y;
      float xx, yy;
      GeglRectangle prect = {0,0,1,1};

      ll2xy (&transform, lon, lat, &x, &y);

      x += xoffset;
      y += 0.5;

      x *= width;
      y *= height;

      prect.x = floor (x);
      prect.y = floor (y);
      prect.width = 1;
      prect.height = 1;

      gegl_buffer_set (output, &prect, 0, babl_format ("R'G'B' u8"), pixel, 8);
    }
  }
#endif

  return TRUE;
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass         *operation_class;
  GeglOperationFilterClass *filter_class;
  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class  = GEGL_OPERATION_FILTER_CLASS (klass);

  filter_class->process = process;
  operation_class->prepare = prepare;
  operation_class->get_bounding_box = get_bounding_box;
  operation_class->get_required_for_output = get_required_for_output;

  gegl_operation_class_set_keys (operation_class,
    "name"       , "gegl:panorama-projection",
    "categories" , "misc",
    "description", _("Perform a equlinear/gnomonic or little planet/stereographic projection of a equirectangular input image."),
    NULL);
}
#endif
