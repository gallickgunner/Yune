#define SATURATION_EXP  1.0f
#define GAMMA           2.2f
#define EPSILON         0.001f
__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                               CLK_ADDRESS_CLAMP_TO_EDGE   |
                               CLK_FILTER_NEAREST;
                               
float getYluminance(float4 color);
float4 tonemapReinhard(float4 col, float lum_white);
float4 tonemapJohnHable(float4 col);

__kernel void tonemap(__read_only image2d_t current_frame, __read_only image2d_t prev_frame,
                      __write_only image2d_t outputImage, int reset)
{
    int img_width = get_image_width(outputImage);
    int img_height = get_image_height(outputImage);
    
    int2 pixel = (int2)(get_global_id(0), get_global_id(1));
    
    if (pixel.x >= img_width || pixel.y >= img_height)
        return;
        
    float4 hdr_color = read_imagef(current_frame, sampler, pixel);
    float4 ldr_color = hdr_color;
    
    ldr_color = tonemapReinhard(hdr_color, 1.0f);
    //ldr_color = tonemapJohnHable(hdr_color);

    //Apply Gamma correction
    clamp(ldr_color, (float4) 0.0f, (float4) 1.0f);
    ldr_color = pow(ldr_color, (float4) (1/GAMMA) );
    write_imagef(outputImage, pixel, ldr_color);
}

float getYluminance(float4 color)
{
    return 0.212671f*color.x + 0.715160f*color.y + 0.072169f*color.z + EPSILON;
}

float4 tonemapReinhard(float4 col, float lum_white)
{
    float lum_world = getYluminance(col);
    float lum_display = lum_world * (1 + lum_world/(lum_white * lum_white) ) / (1+lum_world);
    return lum_display * pow(col/lum_world, (float4) (SATURATION_EXP));
}

float4 tonemapJohnHable(float4 col)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    return ((col*(A*col+C*B)+D*E)/(col*(A*col+B)+D*F))-E/F;
}