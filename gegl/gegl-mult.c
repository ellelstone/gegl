#include "gegl-mult.h"
#include "gegl-scanline-processor.h"
#include "gegl-image-iterator.h"
#include "gegl-scalar-data.h"
#include "gegl-value-types.h"
#include "gegl-param-specs.h"
#include "gegl-utils.h"

enum
{
  PROP_0, 
  PROP_MULT0,
  PROP_MULT1,
  PROP_MULT2,
  PROP_MULT3,
  PROP_LAST 
};

static void class_init (GeglMultClass * klass);
static void init (GeglMult * self, GeglMultClass * klass);
static void get_property (GObject *gobject, guint prop_id, GValue *value, GParamSpec *pspec);
static void set_property (GObject *gobject, guint prop_id, const GValue *value, GParamSpec *pspec);

static GeglScanlineFunc get_scanline_func(GeglUnary * unary, GeglColorSpaceType space, GeglChannelSpaceType type);

static void mult_float (GeglFilter * filter, GeglImageIterator ** iters, gint width);
static void mult_uint8 (GeglFilter * filter, GeglImageIterator ** iters, gint width);

static gpointer parent_class = NULL;

GType
gegl_mult_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo typeInfo =
      {
        sizeof (GeglMultClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) class_init,
        (GClassFinalizeFunc) NULL,
        NULL,
        sizeof (GeglMult),
        0,
        (GInstanceInitFunc) init,
      };

      type = g_type_register_static (GEGL_TYPE_UNARY, 
                                     "GeglMult", 
                                     &typeInfo, 
                                     0);
    }
    return type;
}

static void 
class_init (GeglMultClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GeglOpClass *op_class = GEGL_OP_CLASS(klass);
  GeglUnaryClass *unary_class = GEGL_UNARY_CLASS(klass);

  parent_class = g_type_class_peek_parent(klass);

  unary_class->get_scanline_func = get_scanline_func;

  gobject_class->set_property = set_property;
  gobject_class->get_property = get_property;

  /* op properties */
  gegl_op_class_install_data_input_property (op_class,
                                        g_param_spec_float ("mult0",
                                                      "Mult0",
                                                      "The multiplier of channel 0",
                                                      -G_MAXFLOAT, G_MAXFLOAT,
                                                      1.0,
                                                      G_PARAM_PRIVATE));

  gegl_op_class_install_data_input_property (op_class,
                                        g_param_spec_float ("mult1",
                                                       "Mult1",
                                                       "The multiplier of channel 1",
                                                       -G_MAXFLOAT, G_MAXFLOAT,
                                                       1.0,
                                                       G_PARAM_PRIVATE));

  gegl_op_class_install_data_input_property (op_class,
                                        g_param_spec_float ("mult2",
                                                       "Mult2",
                                                       "The multiplier of channel 2",
                                                       -G_MAXFLOAT, G_MAXFLOAT,
                                                       1.0,
                                                       G_PARAM_PRIVATE));

  gegl_op_class_install_data_input_property (op_class,
                                        g_param_spec_float ("mult3",
                                                       "Mult3",
                                                       "The multiplier of channel 3",
                                                       -G_MAXFLOAT, G_MAXFLOAT,
                                                       1.0,
                                                       G_PARAM_PRIVATE));

  /* gobject properties */
  g_object_class_install_property (gobject_class, PROP_MULT0,
                                   g_param_spec_float ("mult0",
                                                       "Mult0",
                                                       "The multiplier of channel 0",
                                                       -G_MAXFLOAT, G_MAXFLOAT,
                                                       1.0,
                                                       G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MULT1,
                                   g_param_spec_float ("mult1",
                                                       "Mult1",
                                                       "The multiplier of channel 1",
                                                       -G_MAXFLOAT, G_MAXFLOAT,
                                                       1.0,
                                                       G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MULT2,
                                   g_param_spec_float ("mult2",
                                                       "Mult2",
                                                       "The multiplier of channel 2",
                                                       -G_MAXFLOAT, G_MAXFLOAT,
                                                       1.0,
                                                       G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MULT3,
                                   g_param_spec_float ("mult3",
                                                       "Mult3",
                                                       "The multiplier of channel 3",
                                                       -G_MAXFLOAT, G_MAXFLOAT,
                                                       1.0,
                                                       G_PARAM_READWRITE));
}

static void 
init (GeglMult * self, 
      GeglMultClass * klass)
{
  /* Add the multiplier inputs */ 
  gegl_op_append_input(GEGL_OP(self), GEGL_TYPE_SCALAR_DATA, "mult0");
  gegl_op_append_input(GEGL_OP(self), GEGL_TYPE_SCALAR_DATA, "mult1");
  gegl_op_append_input(GEGL_OP(self), GEGL_TYPE_SCALAR_DATA, "mult2");
  gegl_op_append_input(GEGL_OP(self), GEGL_TYPE_SCALAR_DATA, "mult3");

  self->mult[0] = 1.0; 
  self->mult[1] = 1.0; 
  self->mult[2] = 1.0; 
  self->mult[3] = 1.0; 
}

static void
get_property (GObject      *gobject,
              guint         prop_id,
              GValue       *value,
              GParamSpec   *pspec)
{
  GeglMult *self = GEGL_MULT(gobject);
  switch (prop_id)
  {
    case PROP_MULT0:
      g_value_set_float(value, self->mult[0]);
      break;
    case PROP_MULT1:
      g_value_set_float(value, self->mult[1]);
      break;
    case PROP_MULT2:
      g_value_set_float(value, self->mult[2]);
      break;
    case PROP_MULT3:
      g_value_set_float(value, self->mult[3]);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
  }
}

static void
set_property (GObject      *gobject,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GeglMult *self = GEGL_MULT(gobject);
  switch (prop_id)
  {
    case PROP_MULT0:
      self->mult[0] = g_value_get_float(value);
      break;
    case PROP_MULT1:
      self->mult[1] = g_value_get_float(value);
      break;
    case PROP_MULT2:
      self->mult[2] = g_value_get_float(value);
      break;
    case PROP_MULT3:
      self->mult[3]= g_value_get_float(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
  }
}

/* scanline_funcs[data type] */
static GeglScanlineFunc scanline_funcs[] = 
{ 
  NULL, 
  mult_uint8, 
  mult_float, 
  NULL 
};

static GeglScanlineFunc
get_scanline_func(GeglUnary * unary,
                  GeglColorSpaceType space,
                  GeglChannelSpaceType type)
{
  return scanline_funcs[type];
}

static void                                                            
mult_float (GeglFilter * filter,              
                  GeglImageIterator ** iters,        
                  gint width)                       
{                                                                       
  GeglMult * self = GEGL_MULT(filter);

  gfloat mult[4] = { self->mult[0], self->mult[1], self->mult[2], self->mult[3] };

  gfloat **d = (gfloat**)gegl_image_iterator_color_channels(iters[0]);
  gfloat *da = (gfloat*)gegl_image_iterator_alpha_channel(iters[0]);
  gint d_color_chans = gegl_image_iterator_get_num_colors(iters[0]);

  gfloat **a = (gfloat**)gegl_image_iterator_color_channels(iters[1]);
  gfloat *aa = (gfloat*)gegl_image_iterator_alpha_channel(iters[1]);
  gint a_color_chans = gegl_image_iterator_get_num_colors(iters[1]);

  gint alpha_mask = 0x0;

  if(aa)
    alpha_mask |= GEGL_A_ALPHA; 

  {
    gfloat *d0 = (d_color_chans > 0) ? d[0]: NULL;   
    gfloat *d1 = (d_color_chans > 1) ? d[1]: NULL;
    gfloat *d2 = (d_color_chans > 2) ? d[2]: NULL;

    gfloat *a0 = (a_color_chans > 0) ? a[0]: NULL;   
    gfloat *a1 = (a_color_chans > 1) ? a[1]: NULL;
    gfloat *a2 = (a_color_chans > 2) ? a[2]: NULL;

    /* This needs to actually match multipliers to channels returned */

    while(width--)                                                        
      {                                                                   
        switch(d_color_chans)
          {
            case 3: *d2++ = mult[2] * *a2++;
            case 2: *d1++ = mult[1] * *a1++;
            case 1: *d0++ = mult[0] * *a0++;
            case 0:        
          }

        if(alpha_mask == GEGL_A_ALPHA)
          {
              *da++ = mult[3] * *aa++;
          }
      }
  }

  g_free(d);
  g_free(a);
}                                                                       

static void                                                            
mult_uint8 (GeglFilter * filter,              
                  GeglImageIterator ** iters,        
                  gint width)                       
{                                                                       
  GeglMult * self = GEGL_MULT(filter);

  gfloat mult[4] = {self->mult[0], self->mult[1], self->mult[2], self->mult[3]};

  guint8 **d = (guint8**)gegl_image_iterator_color_channels(iters[0]);
  guint8 *da = (guint8*)gegl_image_iterator_alpha_channel(iters[0]);
  gint d_color_chans = gegl_image_iterator_get_num_colors(iters[0]);

  guint8 **a = (guint8**)gegl_image_iterator_color_channels(iters[1]);
  guint8 *aa = (guint8*)gegl_image_iterator_alpha_channel(iters[1]);
  gint a_color_chans = gegl_image_iterator_get_num_colors(iters[1]);

  gint alpha_mask = 0x0;

  if(aa)
    alpha_mask |= GEGL_A_ALPHA; 

  {
    guint8 *d0 = (d_color_chans > 0) ? d[0]: NULL;   
    guint8 *d1 = (d_color_chans > 1) ? d[1]: NULL;
    guint8 *d2 = (d_color_chans > 2) ? d[2]: NULL;

    guint8 *a0 = (a_color_chans > 0) ? a[0]: NULL;   
    guint8 *a1 = (a_color_chans > 1) ? a[1]: NULL;
    guint8 *a2 = (a_color_chans > 2) ? a[2]: NULL;

    /* This needs to actually match multipliers to channels returned */

    while(width--)                                                        
      {                                                                   
        switch(d_color_chans)
          {
            case 3: *d2++ = CLAMP((gint)(mult[2] * *a2 + .5), 0, 255);
                    a2++;
            case 2: *d1++ = CLAMP((gint)(mult[1] * *a1 + .5), 0, 255);
                    a1++;
            case 1: *d0++ = CLAMP((gint)(mult[0] * *a0 + .5), 0, 255);
                    a0++;
            case 0:        
          }

        if(alpha_mask == GEGL_A_ALPHA)
          {
              *da++ = CLAMP((gint)(mult[3] * *aa + .5), 0, 255);
              aa++;
          }
      }
  }

  g_free(d);
  g_free(a);
}                                                                       
