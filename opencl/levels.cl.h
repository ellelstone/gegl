static const char* levels_cl_source =
"__kernel void kernel_levels(__global const float4     *in,                    \n"
"                            __global       float4     *out,                   \n"
"                            float in_offset,                                  \n"
"                            float out_offset,                                 \n"
"                            float scale)                                      \n"
"{                                                                             \n"
"  int gid = get_global_id(0);                                                 \n"
"  float4 in_v  = in[gid];                                                     \n"
"  float4 out_v;                                                               \n"
"  out_v.xyz = (in_v.xyz - in_offset) * scale + out_offset;                    \n"
"  out_v.w   =  in_v.w;                                                        \n"
"  out[gid]  =  out_v;                                                         \n"
"}                                                                             \n"
;