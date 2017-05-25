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
 * Copyright 2012,2013 Felix Ulber <felix.ulber@gmx.de>
 *           2013 Øyvind Kolås <pippin@gimp.org>
 *           2017 Red Hat, Inc.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

property_double (black_level, _("Black level"), 0.0)
    description (_("Adjust the black level"))
    value_range (-0.1, 0.1)

property_double (exposure, _("Exposure"), 0.0)
    description (_("Relative brightness change in stops"))
    ui_range    (-10.0, 10.0)

#else

#define GEGL_OP_POINT_FILTER
#define GEGL_OP_NAME     exposure
#define GEGL_OP_C_SOURCE exposure.c

#include "gegl-op.h"

#include <math.h>
#ifdef _MSC_VER
#define exp2f (b) ((gfloat) pow (2.0, b))
#endif

static void
prepare (GeglOperation *operation)
{
  gegl_operation_set_format (operation, "input", babl_format ("RGBA float"));
  gegl_operation_set_format (operation, "output", babl_format ("RGBA float"));
}

/* GeglOperationPointFilter gives us a linear buffer to operate on
 * in our requested pixel format
 */
static gboolean
process (GeglOperation       *op,
         void                *in_buf,
         void                *out_buf,
         glong                n_pixels,
         const GeglRectangle *roi,
         gint                 level)
{
  GeglProperties *o = GEGL_PROPERTIES (op);
  gfloat     *in_pixel;
  gfloat     *out_pixel;
  gfloat      black_level = (gfloat) o->black_level;
  gfloat      diff;
  gfloat      exposure_negated = (gfloat) -o->exposure;
  gfloat      gain;
  gfloat      white;
  
  glong       i;

  in_pixel = in_buf;
  out_pixel = out_buf;
  
  white = exp2f (exposure_negated);
  diff = MAX (white - black_level, 0.01);
  gain = 1.0f / diff;

  for (i=0; i<n_pixels; i++)
    {
      out_pixel[0] = (in_pixel[0] - black_level) * gain;
      out_pixel[1] = (in_pixel[1] - black_level) * gain;
      out_pixel[2] = (in_pixel[2] - black_level) * gain;
      out_pixel[3] = in_pixel[3];
      
      out_pixel += 4;
      in_pixel  += 4;
    }
    
  return TRUE;
}

/*#include "opencl/gegl-cl.h"

static const char* kernel_source =
"__kernel void kernel_exposure(__global const float4 *in,          \n"
"                              __global       float4 *out,         \n"
"                              float                  black_level, \n"
"                              float                  gain)        \n"
"{                                                                 \n"
"  int gid = get_global_id(0);                                     \n"
"  float4 in_v  = in[gid];                                         \n"
"  float4 out_v;                                                   \n"
"  out_v.xyz =  ((in_v.xyz - black_level) * gain);                 \n"
"  out_v.w   =  in_v.w;                                            \n"
"  out[gid]  =  out_v;                                             \n"
"}                                                                 \n";

static GeglClRunData *cl_data = NULL;

// OpenCL processing function
static cl_int
cl_process (GeglOperation       *op,
            cl_mem               in_tex,
            cl_mem               out_tex,
            size_t               global_worksize,
            const GeglRectangle *roi,
            gint                 level)
{
  // Retrieve a pointer to GeglProperties structure which contains all the
  // chanted properties
  //

  GeglProperties *o = GEGL_PROPERTIES (op);

  gfloat      black_level = (gfloat) o->black_level;
  gfloat      diff;
  gfloat      exposure_negated = (gfloat) -o->exposure;
  gfloat      gain;
  gfloat      white;
  
  cl_int cl_err = 0;

  if (!cl_data)
    {
      const char *kernel_name[] = {"kernel_exposure", NULL};
      cl_data = gegl_cl_compile_and_build (kernel_source, kernel_name);
    }
  if (!cl_data) return 1;

  white = exp2f (exposure_negated);
  diff = MAX (white - black_level, 0.01);
  gain = 1.0f / diff;

  cl_err |= gegl_clSetKernelArg(cl_data->kernel[0], 0, sizeof(cl_mem),   (void*)&in_tex);
  cl_err |= gegl_clSetKernelArg(cl_data->kernel[0], 1, sizeof(cl_mem),   (void*)&out_tex);
  cl_err |= gegl_clSetKernelArg(cl_data->kernel[0], 2, sizeof(cl_float), (void*)&black_level);
  cl_err |= gegl_clSetKernelArg(cl_data->kernel[0], 3, sizeof(cl_float), (void*)&gain);
  if (cl_err != CL_SUCCESS) return cl_err;

  cl_err = gegl_clEnqueueNDRangeKernel(gegl_cl_get_command_queue (),
                                        cl_data->kernel[0], 1,
                                        NULL, &global_worksize, NULL,
                                        0, NULL, NULL);
  if (cl_err != CL_SUCCESS) return cl_err;

  return cl_err;
}
*/
static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass            *operation_class;
  GeglOperationPointFilterClass *point_filter_class;

  operation_class    = GEGL_OPERATION_CLASS (klass);
  point_filter_class = GEGL_OPERATION_POINT_FILTER_CLASS (klass);

//  operation_class->opencl_support = TRUE;
  operation_class->prepare        = prepare;

  point_filter_class->process    = process;
//  point_filter_class->cl_process = cl_process;

  gegl_operation_class_set_keys (operation_class,
    "name",        "gegl:exposure",
    "title",       _("Exposure"),
    "categories",  "color",
    "reference-chain", "load path=images/standard-input.png exposure exposure=1.5",
    "description", _("Changes Exposure of an image, allows stepping HDR and photographs up/down in stops. "),
    "op-version",  "1:0",
    NULL);
}

#endif
