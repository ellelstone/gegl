#include <glib-object.h>
#include "gegl.h"
#include "ctest.h"
#include "csuite.h"
#include "testutils.h"
#include <string.h>

#define MULTIPLIER .5 

#define R0 80
#define G0 160
#define B0 240

#define IMAGE_OP_WIDTH 1 
#define IMAGE_OP_HEIGHT 1 

static GeglOp * source;

static void
test_fade_g_object_new(Test *test)
{
  {
    GeglFade * fade = g_object_new (GEGL_TYPE_FADE, NULL);  

    ct_test(test, fade != NULL);
    ct_test(test, GEGL_IS_FADE(fade));
    ct_test(test, g_type_parent(GEGL_TYPE_FADE) == GEGL_TYPE_UNARY);
    ct_test(test, !strcmp("GeglFade", g_type_name(GEGL_TYPE_FADE)));

    g_object_unref(fade);
  }
}

static void
test_fade_g_object_properties(Test *test)
{
  {
    GeglFade * fade = g_object_new (GEGL_TYPE_FADE, 
                                    "multiplier", MULTIPLIER, 
                                    "input", 0, source,
                                     NULL);  

    ct_test(test, 2 == gegl_node_get_num_inputs(GEGL_NODE(fade)));
    ct_test(test, source == (GeglOp*)gegl_node_get_source(GEGL_NODE(fade), 0));

    g_object_unref(fade);
  }

  {
    GeglFade * fade = g_object_new (GEGL_TYPE_FADE, NULL);  

    g_object_set(fade, "multiplier", MULTIPLIER, NULL);

    g_object_unref(fade);
  }

  {
    gfloat multiplier;
    GeglFade * fade = g_object_new (GEGL_TYPE_FADE, 
                                    "multiplier", MULTIPLIER, 
                                    NULL);  

    g_object_get(fade, "multiplier", &multiplier, NULL);

    ct_test(test, MULTIPLIER == multiplier);

    g_object_unref(fade);
  }
}

static void
test_fade_apply(Test *test)
{
  {
    guint8 r, g, b;
    GeglOp *fade = g_object_new(GEGL_TYPE_FADE,
                                "input", 0, source,
                                "multiplier", MULTIPLIER,
                                NULL);

    gegl_op_apply(fade); 

   
    r = CLAMP((gint)(R0 * MULTIPLIER + .5), 0, 255); 
    g = CLAMP((gint)(G0 * MULTIPLIER + .5), 0, 255); 
    b = CLAMP((gint)(B0 * MULTIPLIER + .5), 0, 255); 

    ct_test(test, testutils_check_rgb_uint8(GEGL_IMAGE_OP(fade), r, g, b)); 
    g_object_unref(fade);
  }

  {
    guint8 r, g, b;
    GeglOp *fade1 = g_object_new(GEGL_TYPE_FADE,
                                 "multiplier", MULTIPLIER,
                                 "input", 0, source,
                                 NULL);

    GeglOp *fade2 = g_object_new(GEGL_TYPE_FADE,
                                 "multiplier", MULTIPLIER,
                                 "input", 0, fade1,
                                 NULL);

    gegl_op_apply(fade2); 

    r = CLAMP((gint)(R0 * MULTIPLIER + .5), 0, 255); 
    g = CLAMP((gint)(G0 * MULTIPLIER + .5), 0, 255); 
    b = CLAMP((gint)(B0 * MULTIPLIER + .5), 0, 255); 

    r = CLAMP((gint)(r * MULTIPLIER + .5), 0, 255); 
    g = CLAMP((gint)(g * MULTIPLIER + .5), 0, 255); 
    b = CLAMP((gint)(b * MULTIPLIER + .5), 0, 255); 

    ct_test(test, testutils_check_rgb_uint8(GEGL_IMAGE_OP(fade2), r, g, b)); 

    g_object_unref(fade1);
    g_object_unref(fade2);
  }
}

static void
fade_test_setup(Test *test)
{
  source = g_object_new(GEGL_TYPE_COLOR, 
                        "width", IMAGE_OP_WIDTH, 
                        "height", IMAGE_OP_HEIGHT, 
                        "pixel-rgb-uint8", R0, G0, B0, 
                        NULL); 

}

static void
fade_test_teardown(Test *test)
{
  g_object_unref(source);
}

Test *
create_fade_test_rgb_uint8()
{
  Test* t = ct_create("GeglFadeTestRgbUint8");

  g_assert(ct_addSetUp(t, fade_test_setup));
  g_assert(ct_addTearDown(t, fade_test_teardown));

#if 1 
  g_assert(ct_addTestFun(t, test_fade_g_object_new));
  g_assert(ct_addTestFun(t, test_fade_g_object_properties));
  g_assert(ct_addTestFun(t, test_fade_apply));
#endif

  return t; 
}
