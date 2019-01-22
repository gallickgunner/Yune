#define PI              3.14159265359f
#define INV_PI          0.31830988618f
#define RAYBUMP_EPSILON 0.01f
#define PLANE_SIZE      5
#define SPHERE_SIZE     2
#define LIGHT_SIZE      2

//Provide your own absolute paths to "kernel-include" folder here.
#include "....\kernel-include\typedefs.h"
#include "....\kernel-include\helper.h"
#include "....\kernel-include\SampleHemisphere.h"

void createRay(float pixel_x, float pixel_y, int img_width, int img_height, Ray* eye_ray, constant Camera* main_cam);
bool traceRay(Ray* ray, HitInfo* hit_info, int exclude_lightID);
float4 shading(Ray ray, int GI_BOUNCES, int GI_CHECK, uint* seed);
float4 evaluateDirectLighting(Ray ray, HitInfo hit_info, uint* seed);
float4 evaluateBRDF(float4 w_i, float4 w_o, HitInfo hit_info);
int sampleLights(HitInfo hit_info, float* geometry_term, float* light_pdf, float4* w_i, uint* seed);

__kernel void pathtracer(__write_only image2d_t outputImage, __read_only image2d_t inputImage,
                                    __constant Camera* main_cam, int RR_THRESHOLD, int GI_CHECK, int reset, uint rand)
{
    int img_width = get_image_width(outputImage);
    int img_height = get_image_height(outputImage);
    
    int2 pixel = (int2)(get_global_id(0), get_global_id(1));
    
    if (pixel.x >= img_width || pixel.y >= img_height)
        return;
        
    
    //create a camera ray 
    Ray eye_ray;
    float r1, r2;
    uint seed = pixel.y* img_width + pixel.x;
    seed =  rand * seed;
    
    //Since wang_hash(61) returns 0 and Xor Shift cant handle 0.
    if(seed == 61)
        seed =  0;
    wang_hash(seed);
    float4 color = (float4) (0.f, 0.f, 0.f, 1.f);
    
    seed = xor_shift(seed);
    r1 = seed / (float) UINT_MAX;
    seed =  xor_shift(seed);
    r2 =  seed / (float) UINT_MAX;
    
    createRay(pixel.x + r1, pixel.y + r2, img_width, img_height, &eye_ray, main_cam);           
    color = shading(eye_ray, RR_THRESHOLD, GI_CHECK, &seed);
    
    if(any(isnan(color)))
            color = PINK;
        
    if ( reset == 1 )
    {   
        color.w = 1;
        write_imagef(outputImage, pixel, color);
    }
    else
    {
        float4 prev_color = read_imagef(inputImage, sampler, pixel);
        int num_passes = prev_color.w;

        color += (prev_color * num_passes);
        color /= (num_passes + 1);
        color.w = num_passes + 1;
        write_imagef(outputImage, pixel, color);    
    }
    
        
}

void createRay(float pixel_x, float pixel_y, int img_width, int img_height, Ray* eye_ray, constant Camera* main_cam)
{

    float aspect_ratio;
    aspect_ratio = (img_width*1.0)/img_height;

    eye_ray->dir.x = aspect_ratio *((2.0 * pixel_x/img_width) - 1);
    eye_ray->dir.y = (2.0 * pixel_y/img_height) -1 ;
    eye_ray->dir.z = -main_cam->view_plane_dist;
    eye_ray->dir.w = 0;
    
    eye_ray->dir.x = dot(main_cam->view_mat.r1, eye_ray->dir);
    eye_ray->dir.y = dot(main_cam->view_mat.r2, eye_ray->dir);
    eye_ray->dir.z = dot(main_cam->view_mat.r3, eye_ray->dir);
    eye_ray->dir.w = dot(main_cam->view_mat.r4, eye_ray->dir);
    
    eye_ray->dir = normalize(eye_ray->dir);
    
    eye_ray->origin = (float4) (main_cam->view_mat.r1.w,
                                main_cam->view_mat.r2.w,
                                main_cam->view_mat.r3.w,
                                main_cam->view_mat.r4.w);
                                
    eye_ray->is_shadow_ray = false;
    eye_ray->length = INFINITY;
                             
}

bool traceRay(Ray* ray, HitInfo* hit_info, int exclude_lightID)
{
     bool flag = false;
    
     for(int i = 0; i < SPHERE_SIZE; i++)
     {
         float4 normal = ray->origin - spheres[i].pos;

         float a = dot(ray->dir, ray->dir);
         float b = dot(normal, ray->dir);
         float c = dot(normal, normal) - spheres[i].radius*spheres[i].radius;
         float disc = b * b - c;

         float t = -1.0f;
         if(disc >= 0.0f)
            t = -b - sqrt(disc);     
            
         if (t >= 0.0 && t < ray->length)
         {
            ray->length = t;
            hit_info->obj_ID = i;
            hit_info->hit_point = ray->origin + (ray->dir * ray->length);
            hit_info->normal = normalize((hit_info->hit_point - spheres[i].pos));
            flag = true;
         }
         
        if(flag && ray->is_shadow_ray)
            break;
     }   
    
    for(int i = 0; i < PLANE_SIZE; i++)
    {       
        float DdotN = dot(ray->dir, planes[i].norm);
        if(fabs(DdotN) > 0.0001)
        {
            float t = dot(planes[i].norm, planes[i].point - ray->origin) / DdotN;
            if(t>=0.0 && t < ray->length)
            {
                hit_info->obj_ID = -1;
                hit_info->plane_ID = i;
                hit_info->normal = planes[i].norm;
                hit_info->hit_point = ray->origin + (ray->dir * t);
                ray->length = t;
                if(!ray->is_shadow_ray)
                    flag = true;
            }
        }       
    }
    if(hit_info->plane_ID == 1 || hit_info->plane_ID == 3 )
    {
        int j;
        if(hit_info->plane_ID == 1)
            j = 0;
        else
            j = 1;
        
        if(j == exclude_lightID)
            return flag;
        
        float proj1, proj2, la, lb;
        float4 temp;
        temp = hit_info->hit_point - light_sources[j].pos;

        proj1 = dot(temp, light_sources[j].edge_l);
        proj2 = dot(temp, light_sources[j].edge_w);             
        la = length(light_sources[j].edge_l);
        lb = length(light_sources[j].edge_w);
        
        // Projection of the vector from rectangle corner to hitpoint on the edges.
        proj1 /= la;    
        proj2 /= lb;
        
        if( (proj1 >= 0.0 && proj2 >= 0.0)  && (proj1 <= la && proj2 <= lb)  )
        {
            hit_info->plane_ID = -1;
            hit_info->light_ID = j;
            flag = true;                    
        }
    }
    return flag;
}


float4 shading(Ray ray, int RR_THRESHOLD, int GI_CHECK, uint* seed)
{   
    HitInfo hit_info = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
    if(!traceRay(&ray, &hit_info, -1))
        return SKY_COLOR;
    
    if(hit_info.light_ID >= 0)
        return (float4) (1.f, 1.f, 1.f, 1.f);   
                    
    
    float4 direct_color, indirect_color, throughput;    
    throughput = (float4) (1.f, 1.f, 1.f, 1.f);
    direct_color = (float4) (0.f, 0.f, 0.f, 0.f);   
    indirect_color = direct_color;
    
    // Compute Direct Illumination at the first hitpoint.
    direct_color =  evaluateDirectLighting(ray, hit_info, seed);
    
    //Compute Indirect Illumination by storing all the seondary rays first.
    if(GI_CHECK)
    {   
        int x = 0;
      
        /* Currently bug on AMD i guess. While loop doesn't work properly and gives dark images. On NVIDIA while works as intended. 
         * Hence as a workaround for AMD, used for loop with a high limit. (RR would prolly break before 100).
         */
        for(int i = 0; i < 100; i++)
        //while(true)
        {
            HitInfo new_hitinfo = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
            Ray new_ray;
            float pdf = 1, cosine_falloff, spec_prob = 0.0f, r;
            
            *seed = xor_shift(*seed);
            r = *seed / (float) UINT_MAX;   
            
            if(hit_info.obj_ID >= 0)
            {
                spec_prob = getYluminance(spheres[hit_info.obj_ID].specular_col);
            }
            else if (hit_info.plane_ID >= 0)
                spec_prob = getYluminance(planes[hit_info.plane_ID].specular_col);
            
            if(r <= spec_prob)
                phongsampleHemisphere(&new_ray, &pdf, ray.dir, hit_info, seed);             
            else
                cosineWeightedHemisphere(&new_ray, &pdf, hit_info, seed);
            cosine_falloff = max(dot(new_ray.dir, hit_info.normal), 0.0f);
            if(cosine_falloff == 0.0f)
                continue;               
            
            // If GI_ray hits nothing, store sky color and break out of bounce loop.
            if(!traceRay(&new_ray, &new_hitinfo, -1))
                break;
            
            // Skip this iteration as ray hits light source.
            else if(new_hitinfo.light_ID >= 0)
                continue;
                
            //If it hits object, accumulate the brdf  and geometry terms.               
            throughput = throughput * evaluateBRDF(new_ray.dir, -ray.dir, hit_info) * cosine_falloff / pdf; 
        
            /* Russian Roulette: Use RR after few bounces. Terminate paths with low enough value of throughput. We use Y luminance as the value
             * that the path survives. The termination probability is (1 - Yluminance). If the path survives boost the energy by
             * (1/p)
             */
            
            if(x > RR_THRESHOLD)
            {       
                float p = min(getYluminance(throughput), 0.95f);
                *seed = xor_shift(*seed);
                r = *seed / (float) UINT_MAX;   
                
                if(r >= p)
                    break;              
                throughput *= 1/p;
            }
            
            indirect_color +=  throughput * evaluateDirectLighting(new_ray, new_hitinfo, seed);
            hit_info = new_hitinfo;
            ray = new_ray;
            x++;
            
        }
    }
    return direct_color + indirect_color;
}

float4 evaluateDirectLighting(Ray ray, HitInfo hit_info, uint* seed)
{   
    //Note that we for calculating light source contribution we use light_color * light_power/area to get the total intensity.
    float4 brdf_sample = (float4) (0.f, 0.f, 0.f, 0.f);
    float4 light_sample = (float4) (0.f, 0.f, 0.f, 0.f);
    float4 w_i;
    float light_pdf, brdf_pdf, geometry_term, mis_weight, cosine_falloff, spec_prob, r;
    
    //We use MIS with Power Heuristic.
    //Direct Light Sampling
    int j = sampleLights(hit_info, &geometry_term, &light_pdf, &w_i, seed);
    if(j == -1)
        return light_sample;
    
    Ray shadow_ray = {hit_info.hit_point + w_i * RAYBUMP_EPSILON, w_i, INFINITY, true};
    HitInfo shadow_hitinfo = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
    
    if(!traceRay(&shadow_ray, &shadow_hitinfo, j) )
    {   
        *seed = xor_shift(*seed);
        r = *seed / (float) UINT_MAX;
        brdf_pdf = max(dot(w_i, hit_info.normal), 0.0f) * INV_PI;
        
        mis_weight =  light_pdf;        
        powerHeuristic(&mis_weight, light_pdf, brdf_pdf, 2);
            
        light_sample =  evaluateBRDF(w_i, -ray.dir, hit_info) * light_sources[j].emissive_col * geometry_term;  
        //light_sample *=  mis_weight/light_pdf;
        light_sample *=  1/light_pdf;
        
    }
    
    return light_sample;
    
    // Currently Struggling with implementing MIS. MIS returns darker images and I don't know if it's intended or a bug.
    // BRDF Sampling
    Ray brdf_sample_ray;
        
    cosineWeightedHemisphere(&brdf_sample_ray, &brdf_pdf, hit_info, seed);
    cosine_falloff = max(dot(brdf_sample_ray.dir, hit_info.normal), 0.0f);
    if(cosine_falloff == 0.0f)
        return light_sample;                
    
    w_i = brdf_sample_ray.dir;  
    HitInfo new_hitinfo = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};   
    
    //If traceRay doesnt hit anything or if it does not hit the same light source return only light sample.
    if(!traceRay(&brdf_sample_ray, &new_hitinfo, -1))
       return light_sample;
    else if(new_hitinfo.light_ID != j)
        return light_sample;
        
    mis_weight = brdf_pdf;
    powerHeuristic(&mis_weight, light_pdf, brdf_pdf, 2);
    
    brdf_sample =  evaluateBRDF(w_i, -ray.dir, hit_info) * light_sources[j].emissive_col * light_sources[j].power * cosine_falloff / light_sources[j].area;
    brdf_sample *=  mis_weight / brdf_pdf;
    
    return (light_sample + brdf_sample);
}

float4 evaluateBRDF(float4 w_i, float4 w_o, HitInfo hit_info)
{
    float4 diffuse, specular, refl_vec;
    float mirror_config;
    
    refl_vec = (2*dot(w_i, hit_info.normal)) *hit_info.normal - w_i;
    refl_vec = normalize(refl_vec);
    
    if(hit_info.plane_ID >= 0)
    {
        mirror_config = pow(max(dot(w_o, refl_vec), 0.0f), planes[hit_info.plane_ID].phong_exponent);       
        diffuse = planes[hit_info.plane_ID].diffuse_col * INV_PI;
        specular = planes[hit_info.plane_ID].specular_col * mirror_config * (planes[hit_info.plane_ID].phong_exponent+2) * INV_PI * 0.5f;
    }
    else
    {       
        mirror_config = pow(max(dot(w_o, refl_vec), 0.0f), spheres[hit_info.obj_ID].phong_exponent);        
        diffuse = spheres[hit_info.obj_ID].diffuse_col * INV_PI;
        specular = spheres[hit_info.obj_ID].specular_col * mirror_config * (spheres[hit_info.obj_ID].phong_exponent+2) * INV_PI * 0.5f;
    }
    return (diffuse + specular);
    
}

int sampleLights(HitInfo hit_info, float* geometry_term, float* light_pdf, float4* w_i, uint* seed)
{
    float sum_gt = 0, cosine_falloff = 0, max_weight = -1, r1, r2, distance; // sum_gt = sum of geometry terms.
    float gt_arr[LIGHT_SIZE]; // array of geometry terms for each light source. Store this to return information about the light source picked.
    float4 w_is[LIGHT_SIZE]; // Store every light direction. Store this to return information about the light source picked.
    int max_idx = -1;
    for(int i = 0; i < LIGHT_SIZE; i++)
    {
        float4 temp_wi;
        
        *seed = xor_shift(*seed);
        r1 = *seed / (float) UINT_MAX;      
        *seed = xor_shift(*seed);   
        r2 = *seed / (float) UINT_MAX;
        
        temp_wi = (light_sources[i].pos + r1*light_sources[i].edge_l + r2*light_sources[i].edge_w) - hit_info.hit_point;    
        distance = dot(temp_wi, temp_wi);
        temp_wi = normalize(temp_wi);
        w_is[i] = temp_wi;  
        
        cosine_falloff = max(dot(temp_wi, hit_info.normal), 0.0f) * max(dot(-(temp_wi), light_sources[i].norm), 0.0f);
        
        if(cosine_falloff <= 0.0)
        {
            gt_arr[i] = 0;
            continue;
        }
        
        gt_arr[i] = light_sources[i].power * cosine_falloff / (light_sources[i].area * distance);
        if(max_weight < gt_arr[i])
        {
            max_idx = i;
            max_weight = gt_arr[i];
        }
        sum_gt += gt_arr[i]; 
    }
    
    // If no lights get hit, return -1
    if(max_idx == -1)
        return max_idx;
    
    //Pick a Uniform random number and return the light w.r.t to it's probability.
    *seed = xor_shift(*seed);
    r1 = *seed / (float) UINT_MAX;
    
    float cumulative_weight = 0;
    for(int i = 0; i < LIGHT_SIZE; i++)
    {       
        float weight = gt_arr[i]/sum_gt;
        
        if(r1 >= cumulative_weight && r1 < (cumulative_weight + weight ) )
        {
            *geometry_term = gt_arr[i];
            *light_pdf = weight/(light_sources[i].area);
            *w_i = w_is[i];
            return i;
        }
        cumulative_weight += weight;
    }
}
