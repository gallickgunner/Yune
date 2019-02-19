#define PI              3.14159265359f
#define INV_PI          0.31830988618f
#define EPSILON         0.0001f
#define RR_THRESHOLD    3
#define WALL_SIZE       6
#define SPHERE_SIZE     2
#define LIGHT_SIZE      2

typedef struct Mat4x4{
    float4 r1;
    float4 r2;
    float4 r3;
    float4 r4;
} Mat4x4;

typedef struct Ray{

    float4 origin;
    float4 dir;
    float length;
    bool is_shadow_ray;
} Ray;

typedef struct HitInfo{

    int sphere_ID, quad_ID, light_ID;
    float4 hit_point;  // point of intersection.
    float4 normal;

} HitInfo;

typedef struct Sphere{

    float4 pos;
    float4 emissive_col;
    float4 diffuse_col;
    float4 specular_col;
    float radius;
    float phong_exponent;

} Sphere;

typedef struct Quad{

    float4 pos;
    float4 norm;
    float4 emissive_col;
    float4 diffuse_col;
    float4 specular_col;
    float4 edge_l;
    float4 edge_w;
    float phong_exponent;

} Quad;

typedef struct Triangle{

    float4 v1;
    float4 v2;
    float4 v3;
    float4 vn1;
    float4 vn2;
    float4 vn3;
    int matID;       // total size till here = 100 bytes
    float pad[3];    // padding 12 bytes - to make it 112 bytes (next multiple of 16
} Triangle;

//For use with Triangle geometry. For geometric shapes material data is present with shape structure.
typedef struct Material{

    float4 emissive;
    float4 diff;
    float4 spec;
    float ior;
    float alpha_x;
    float alpha_y;
    short is_specular;
    short is_transmissive;    // total 64 bytes.
} Material;

typedef struct Camera{
    Mat4x4 view_mat;
    float view_plane_dist;  // total 68 bytes
    float pad[3];           // 12 bytes padding to reach 80 (next multiple of 16)
} Camera;

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                           CLK_ADDRESS_CLAMP_TO_EDGE   |
                           CLK_FILTER_NEAREST;

__constant Sphere spheres[2] = {  {  (float4)(2.f, -2.f, -6.f, 1.f),
                                               (float4)(0.f, 0.f, 0.f, 0.f),
                                               (float4)(0.f, 0.f, 0.f, 0.f),
                                               (float4)(0.75f, 0.75f, 0.75f, 0.f),
                                                1.5f, 10000.0f
                                            },

                                             { (float4)(-4.f, -2.f, -6.f, 1.f),
                                               (float4)(0.f, 0.f, 0.f, 0.f),
                                               (float4)(0.6f, 0.6f, 0.6f, 0),
                                               (float4)(0.2f, 0.2f, 0.2f, 0),
                                               1.0f, 10.0f
                                             }
                                           };

__constant Quad light_sources[LIGHT_SIZE] = { {     (float4)(-2.0f, 5.f, -6.5f, 1.f),   //pos
                                                    (float4)(0.f, -1.f, 0.f, 0.f),      //norm
                                                    (float4)(14.f, 14.f, 14.f, 0.f),    //emissive col
                                                    (float4)(0.f, 0.f, 0.f, 0.f),       //diffuse col
                                                    (float4)(0.f, 0.f, 0.f, 0.f),       //specular col
                                                    (float4)(4.f, 0.f, 0.f, 0.f),       //edge_l
                                                    (float4)(0.f, 0.f, 1.f, 0.f),       //edge_w
                                                    0.f                                 //phong exponent
                                               },

                                              {     (float4)(-6.0f, 0.5f, -4.0f, 1.f),
                                                    (float4)(1.f, 0.f, 0.f, 0.f),
                                                    (float4)(14.f, 14.f, 14.f, 0.f),
                                                    (float4)(0.f, 0.f, 0.f, 0.f),
                                                    (float4)(0.f, 0.f, 0.f, 0.f),
                                                    (float4)(0.f, 2.f, 0.f, 0.f),
                                                    (float4)(0.f, 0.f, -2.f, 0.f),
                                                    0.f
                                              }
                                            };

__constant Quad walls[WALL_SIZE] = { {   (float4)(-6.f, -4.f, -9.f, 1.f),     // bottom floor
                                         (float4)(0.f, 1.f, 0.f, 0.f),
                                         (float4)(0.f, 0.f, 0.f, 0.f),
                                         (float4)(0.75f, 0.75f, 0.75f, 0.f),
                                         (float4)(0.f, 0.f, 0.f, 0.f),
                                         (float4)(12.f, 0.f, 0.f, 0.f),
                                         (float4)(0.f, 0.f, 10.f, 0.f), 60.f
                                       },

                                       { (float4)(-6.f, 5.f, -9.f, 1.f),      // top wall
                                         (float4)(0.f, -1.f, 0.f, 0.f),
                                         (float4)(0.f, 0.f, 0.f, 0.f),
                                         (float4)(0.75f, 0.75f, 0.75f, 0.f),
                                         (float4)(0.f, 0.f, 0.f, 0.f),
                                         (float4)(12.f, 0.f, 0.f, 0.f),
                                         (float4)(0.f, 0.f, 10.f, 0.f), 60.f
                                       },

                                       { (float4)(6.f, 5.f, -9.f, 0.f),      //right wall
                                         (float4)(-1.f, 0.f, 0.f, 0.f),
                                         (float4)(0.f, 0.f, 0.f, 0.f),
                                         (float4)(0.630f, 0.058f, 0.062f, 0.f),
                                         (float4)(0.f, 0.f, 0.f, 0.f),
                                         (float4)(0.f, -9.f, 0.f, 0.f),
                                         (float4)(0.f, 0.f, 10.f, 0.f), 60.f
                                       },

                                       { (float4)(-6.f, 5.f, -9.f, 1.f),     //left wall
                                         (float4)(1.f, 0.f, 0.f, 0.f),
                                         (float4)(0.f, 0.f, 0.f, 0.f),
                                         (float4)(0.132f, 0.406f, 0.061f, 0.f),
                                         (float4)(0.f, 0.f, 0.f, 0.f),
                                         (float4)(0.f, -9.f, 0.f, 0.f),
                                         (float4)(0.f, 0.f, 10.f, 0.f), 60.f
                                       },

                                       { (float4)(-6.f, 5.f, -9.f, 0.f),     //back wall
                                         (float4)(0.f, 0.f, 1.f, 0.f),
                                         (float4)(0.f, 0.f, 0.f, 0.f),
                                         (float4)(0.75f, 0.75f, 0.75f, 0.f),
                                         (float4)(0.f, 0.f, 0.f, 0.f),
                                         (float4)(0.f, -9.f, 0.f, 0.f),
                                         (float4)(12.f, 0.f, 0.f, 0.f), 260.f
                                       },

                                       { (float4)(-6.f, 5.f, 1.f, 0.f),     //front wall
                                         (float4)(0.f, 0.f, -1.f, 0.f),
                                         (float4)(0.f, 0.f, 0.f, 0.f),
                                         (float4)(0.75f, 0.75f, 0.75f, 0.f),
                                         (float4)(0.f, 0.f, 0.f, 0.f),
                                         (float4)(0.f, -9.f, 0.f, 0.f),
                                         (float4)(12.f, 0.f, 0.f, 0.f), 260.f
                                       }
                                    };

__constant float4 SKY_COLOR =(float4) (0.588f, 0.88f, 1.0f, 1.0f);
__constant float4 BACKGROUND_COLOR =(float4) (0.4f, 0.4f, 0.4f, 1.0f);
__constant float4 PINK = (float4) (0.988f, 0.0588f, 0.7529f, 1.0f);

//Core Functions
void createRay(float pixel_x, float pixel_y, int img_width, int img_height, Ray* eye_ray, constant Camera* main_cam);
bool traceRay(Ray* ray, HitInfo* hit_info, int exclude_lightID);
float4 shading(Ray ray, int GI_CHECK, uint* seed);
float4 evaluateDirectLighting(Ray ray, HitInfo hit_info, uint* seed);
float4 evaluateBRDF(float4 w_i, float4 w_o, HitInfo hit_info);
int sampleLights(HitInfo hit_info, float* light_pdf, float4* w_i, uint* seed);

//Sampling Hemisphere Functions
void phongSampleHemisphere (Ray* ray, float* pdf, float4 w_o, HitInfo hit_info, uint* seed);
void uniformSampleHemisphere(Ray* ray, float* pdf, HitInfo hit_info, uint* seed);
void cosineWeightedHemisphere(Ray* ray, float* pdf, HitInfo hit_info, uint* seed);

//PDF functions to return PDF values provided a ray direction
float calcPhongPDF(float4 w_i, float4 w_o, HitInfo hit_info);
float calcCosPDF(float4 w_i, HitInfo hit_info);

// Helper Functions
float getYluminance(float4 color);
uint wang_hash(uint seed);
uint xor_shift(uint seed);
void powerHeuristic(float* weight, float light_pdf, float brdf_pdf, int beta);

__kernel void pathtracer(__write_only image2d_t outputImage, __read_only image2d_t inputImage, __constant Camera* main_cam, 
                         __global Triangle* scene_data, __global Material* mat_data, int GI_CHECK, int reset, uint rand)
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
    color = shading(eye_ray, GI_CHECK, &seed);
    
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
            hit_info->sphere_ID = i;
            hit_info->hit_point = ray->origin + (ray->dir * ray->length);
            hit_info->normal = normalize((hit_info->hit_point - spheres[i].pos));
            flag = true;
         }
         
        if(flag && ray->is_shadow_ray)
            break;
     }   
    
    for(int i = 0; i < WALL_SIZE; i++)
    {       
        float DdotN = dot(ray->dir, walls[i].norm);
        if(fabs(DdotN) > 0.0001)
        {
            float t = dot(walls[i].norm, walls[i].pos - ray->origin) / DdotN;
            if(t>=0.0 && t < ray->length)
            {
                float proj1, proj2, la, lb;
                float4 temp;
                
                hit_info->hit_point = ray->origin + (ray->dir * t);
                temp = hit_info->hit_point - walls[i].pos;
                proj1 = dot(temp, walls[i].edge_l);
                proj2 = dot(temp, walls[i].edge_w);             
                la = length(walls[i].edge_l);
                lb = length(walls[i].edge_w);
                
                // Projection of the vector from rectangle corner to hitpoint on the edges.
                proj1 /= la;    
                proj2 /= lb;
                
                if( (proj1 >= 0.0 && proj2 >= 0.0)  && (proj1 <= la && proj2 <= lb)  )
                {
                    hit_info->sphere_ID = -1;
                    hit_info->light_ID = -1;
                    hit_info->quad_ID = i;
                    hit_info->normal = walls[i].norm;
                    ray->length = t;
                    if(!ray->is_shadow_ray)
                        flag = true;                    
                }
                
                //Check intersection with light sources.
                if(i == 1 || i == 3)
                {
                    int j = 0;
                    
                    if( i == 1)
                        j = 0;
                    else
                        j = 1;
                    
                    if(j == exclude_lightID)
                        continue;
                        
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
                        hit_info->quad_ID = -1;
                        hit_info->light_ID = j;
                        flag = true;                    
                    }
                }                
                if(flag && ray->is_shadow_ray)
                    break;
            }
        }       
    }
    return flag;
}


float4 shading(Ray ray, int GI_CHECK, uint* seed)
{   
    HitInfo hit_info = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
    if(!traceRay(&ray, &hit_info, -1))
        return BACKGROUND_COLOR;
    
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
        for(int i = 0; i < 100000; i++)
        //while(true)
        {
            HitInfo new_hitinfo = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
            Ray new_ray;
            float pdf = 1, cosine_falloff, spec_prob = 0.0f, r;
            
            *seed = xor_shift(*seed);
            r = *seed / (float) UINT_MAX;   
            
            if(hit_info.sphere_ID >= 0)
            {
                spec_prob = length(spheres[hit_info.sphere_ID].specular_col);
            }
            else if (hit_info.quad_ID >= 0)
                spec_prob = length(walls[hit_info.quad_ID].specular_col);
            
            if(r <= spec_prob)
                phongSampleHemisphere(&new_ray, &pdf, -ray.dir, hit_info, seed);             
            else
                cosineWeightedHemisphere(&new_ray, &pdf, hit_info, seed);            
            
            // If GI_ray hits nothing, accumulate background color.
            if(!traceRay(&new_ray, &new_hitinfo, -1) || pdf == 0.0f)
                break;
            
            // Skip this iteration as ray hits light source.
            if(new_hitinfo.light_ID >= 0)
                continue;
                
            //If it hits object, accumulate the brdf  and geometry terms.
            throughput = throughput * evaluateBRDF(new_ray.dir, -ray.dir, hit_info) * max(dot(new_ray.dir, hit_info.normal), 0.0f) / pdf; 
        
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
    float4 brdf_sample = (float4) (0.f, 0.f, 0.f, 0.f);
    float4 light_sample = (float4) (0.f, 0.f, 0.f, 0.f);
    float4 w_i;
    float light_pdf, brdf_pdf, distance, mis_weight, cosine_falloff, spec_prob, r;
    
    //We use MIS with Power Heuristic. Currently Struggling with implementing MIS. MIS returns darker images and I don't know if it's intended or a bug.
    //Direct Light Sampling
    int j = sampleLights(hit_info, &light_pdf, &w_i, seed);
    if(j == -1)
        return light_sample;
    distance = dot(w_i, w_i);
    w_i = normalize(w_i);
    
    Ray shadow_ray = {hit_info.hit_point + w_i * EPSILON, w_i, INFINITY, true};
    HitInfo shadow_hitinfo = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
    
    //If ray doesn't hit anything (exclude light source j while intersection check). This means light source j visible.
    if(!traceRay(&shadow_ray, &shadow_hitinfo, j) )
    {
        *seed = xor_shift(*seed);
        r = *seed / (float) UINT_MAX;   
        
        //Use probability to pick either diffuse or glossy pdf
        
        if(hit_info.sphere_ID >= 0)
            spec_prob = length(spheres[hit_info.sphere_ID].specular_col);        
        else if (hit_info.quad_ID >= 0)
            spec_prob = length(walls[hit_info.quad_ID].specular_col);
        
        if(r <= spec_prob)
            brdf_pdf = calcPhongPDF(w_i, -ray.dir, hit_info);
        else
            brdf_pdf = calcCosPDF(w_i, hit_info);
        
        mis_weight =  light_pdf;        
        powerHeuristic(&mis_weight, light_pdf, brdf_pdf, 2);
        cosine_falloff = max(dot(w_i, hit_info.normal), 0.0f) * max(dot(-(w_i), light_sources[j].norm), 0.0f);
        
        light_sample =  evaluateBRDF(w_i, -ray.dir, hit_info) * light_sources[j].emissive_col * cosine_falloff / distance;          
        //light_sample *=  mis_weight/light_pdf;
        light_sample *=  1/light_pdf;
        
    }
    
    return light_sample;   
    
    // BRDF Sampling
    Ray brdf_sample_ray;
    
    if(r <= spec_prob)
        phongSampleHemisphere(&brdf_sample_ray, &brdf_pdf, -ray.dir, hit_info, seed);
    else
        cosineWeightedHemisphere(&brdf_sample_ray, &brdf_pdf, hit_info, seed);
    
    if(brdf_pdf == 0.0f)
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
    
    cosine_falloff = max(dot(brdf_sample_ray.dir, hit_info.normal), 0.0f);
    brdf_sample =  evaluateBRDF(w_i, -ray.dir, hit_info) * light_sources[j].emissive_col * cosine_falloff;
    brdf_sample *=  mis_weight / brdf_pdf;
    
    return (light_sample + brdf_sample);
}

float4 evaluateBRDF(float4 w_i, float4 w_o, HitInfo hit_info)
{
    float4 diffuse, specular, refl_vec;
    float mirror_config;
    
    refl_vec = (2*dot(w_i, hit_info.normal)) *hit_info.normal - w_i;
    refl_vec = normalize(refl_vec);
    
    if(hit_info.quad_ID >= 0)
    {
        mirror_config = pow(max(dot(w_o, refl_vec), 0.0f), walls[hit_info.quad_ID].phong_exponent);       
        diffuse = walls[hit_info.quad_ID].diffuse_col * INV_PI;
        specular = walls[hit_info.quad_ID].specular_col * mirror_config * (walls[hit_info.quad_ID].phong_exponent+2) * INV_PI * 0.5f;
    }
    else
    {       
        mirror_config = pow(max(dot(w_o, refl_vec), 0.0f), spheres[hit_info.sphere_ID].phong_exponent);        
        diffuse = spheres[hit_info.sphere_ID].diffuse_col * INV_PI;
        specular = spheres[hit_info.sphere_ID].specular_col * mirror_config * (spheres[hit_info.sphere_ID].phong_exponent+2) * INV_PI * 0.5f;
    }
    return (diffuse + specular);
    
}

int sampleLights(HitInfo hit_info, float* light_pdf, float4* w_i, uint* seed)
{
    float sum = 0, cosine_falloff = 0, r1, r2, distance, area;  // sum = sum of geometry terms.
    float weights[LIGHT_SIZE];                                  // array of weights for each light source.
    float4 w_is[LIGHT_SIZE];                                    // Store every light direction. Store this to return information about the light source picked.
    for(int i = 0; i < LIGHT_SIZE; i++)
    {
        float4 temp_wi;
        
        *seed = xor_shift(*seed);
        r1 = *seed / (float) UINT_MAX;      
        *seed = xor_shift(*seed);   
        r2 = *seed / (float) UINT_MAX;
        
        temp_wi = (light_sources[i].pos + r1*light_sources[i].edge_l + r2*light_sources[i].edge_w) - hit_info.hit_point;    
        distance = dot(temp_wi, temp_wi);
        w_is[i] = temp_wi;  
        temp_wi = normalize(temp_wi);        
        
        cosine_falloff = max(dot(temp_wi, hit_info.normal), 0.0f) * max(dot(-(temp_wi), light_sources[i].norm), 0.0f);
        
        if(cosine_falloff <= 0.0)
        {
            weights[i] = 0;
            continue;
        }
        area = length(light_sources[i].edge_l) * length(light_sources[i].edge_w);
        weights[i] = length(light_sources[i].emissive_col) * cosine_falloff * area / distance;
        sum += weights[i]; 
    }
    
    // If no lights get hit, return -1
    if(sum == 0)
        return -1;
    
    //Pick a Uniform random number and return the light w.r.t to it's probability.
    *seed = xor_shift(*seed);
    r1 = *seed / (float) UINT_MAX;
    
    float cumulative_weight = 0;
    for(int i = 0; i < LIGHT_SIZE; i++)
    {       
        float weight = weights[i]/sum;
        
        if(r1 >= cumulative_weight && r1 < (cumulative_weight + weight ) )
        {
            area = length(light_sources[i].edge_l) * length(light_sources[i].edge_w);
            *light_pdf = weight/area;
            *w_i = w_is[i];
            return i;
        }
        cumulative_weight += weight;
    }
}

void phongSampleHemisphere (Ray* ray, float* pdf, float4 w_o, HitInfo hit_info, uint* seed)
{
    /* Create a new coordinate system for Normal Space where Z aligns with Reflection Direction. */
    Mat4x4 normal_to_world;
    float4 Ny, Nx, Nz;

    //Note that w_o points outwards from the surface.
    Nz = 2*(dot(w_o, hit_info.normal)) * hit_info.normal - w_o;
    Nz = normalize(Nz);

    if ( fabs(Nz.y) > fabs(Nz.z) )
        Nx = (float4) (Nz.y, -Nz.x, 0, 0.f);
    else
        Nx = (float4) (Nz.z, 0, -Nz.x, 0.f);

    Nx = normalize(Nx);
    Ny = normalize(cross(Nz, Nx));

    normal_to_world.r1 = (float4) (Nx.x, Ny.x, Nz.x, hit_info.hit_point.x);
    normal_to_world.r2 = (float4) (Nx.y, Ny.y, Nz.y, hit_info.hit_point.y);
    normal_to_world.r3 = (float4) (Nx.z, Ny.z, Nz.z, hit_info.hit_point.z);
    normal_to_world.r4 = (float4) (Nx.w, Ny.w, Nz.w, hit_info.hit_point.w);

    float x, y, z, r1, r2;

    *seed = xor_shift(*seed);
    r1 = *seed / (float) UINT_MAX;
    *seed = xor_shift(*seed);
    r2 = *seed / (float) UINT_MAX;

    /*
    theta = inclination (from Y), phi = azimuth. Need theta in [0, pi/2] and phi in [0, 2pi]
    => X = r sin(theta) cos(phi)
    => Y = r sin(theta) sin(phi)
    => Z = r cos(theta)

    Phong PDF is  (n+1) * cos^n(alpha)/2pi  where alpha is the angle between reflection dir and w_o(the sample taken).
    Since reflection direction aligned with Z axis, alpha equals theta. Formula is,

    (alpha, phi) = (acos(r1^(1/(n+1))), 2pi*r2)
    */
    int phong_exponent;
    if(hit_info.sphere_ID > 0)
        phong_exponent = spheres[hit_info.sphere_ID].phong_exponent;
    else
        phong_exponent = walls[hit_info.quad_ID].phong_exponent;

    float phi = 2*PI * r2;
    float costheta = pow(r1, 1.0f/(phong_exponent+1));
    float sintheta = 1 - pow(r1, 2.0f/(phong_exponent+1));
    sintheta = sqrt(sintheta);

    x = sintheta * cos(phi);  // r * sin(theta) cos(phi)
    y = sintheta * sin(phi);  // r * sin(theta) sin(phi)
    z = costheta;             // r * cos(theta)  r = 1

    float4 ray_dir = (float4) (x, y, z, 0);

    ray->dir.x = dot(normal_to_world.r1, ray_dir);
    ray->dir.y = dot(normal_to_world.r2, ray_dir);
    ray->dir.z = dot(normal_to_world.r3, ray_dir);
    ray->dir.w = 0;

    ray->dir = normalize(ray->dir);
    ray->origin = hit_info.hit_point + ray->dir * EPSILON;
    ray->is_shadow_ray = false;
    ray->length = INFINITY;
    *pdf = (phong_exponent+1) * 0.5 * INV_PI * pow(costheta, phong_exponent);
}

void uniformSampleHemisphere(Ray* ray, float* pdf, HitInfo hit_info, uint* seed)
{

    /* Create a new coordinate system for Normal Space where Z aligns with normal. */
    Mat4x4 normal_to_world;
    float4 Ny, Nx, Nz;
    Nz = hit_info.normal;

    if ( fabs(Nz.y) > fabs(Nz.z) )
        Nx = (float4) (Nz.y, -Nz.x, 0, 0.f);
    else
        Nx = (float4) (Nz.z, 0, -Nz.x, 0.f);

    Nx = normalize(Nx);
    Ny = normalize(cross(Nz, Nx));

    normal_to_world.r1 = (float4) (Nx.x, Ny.x, Nz.x, hit_info.hit_point.x);
    normal_to_world.r2 = (float4) (Nx.y, Ny.y, Nz.y, hit_info.hit_point.y);
    normal_to_world.r3 = (float4) (Nx.z, Ny.z, Nz.z, hit_info.hit_point.z);
    normal_to_world.r4 = (float4) (Nx.w, Ny.w, Nz.w, hit_info.hit_point.w);

    float x, y, z, r1, r2;
    /* Sample hemisphere according to area.  */
    *seed = xor_shift(*seed);
    r1 = *seed / (float) UINT_MAX;
    *seed = xor_shift(*seed);
    r2 = *seed / (float) UINT_MAX;

    /*
    theta = inclination (from Y), phi = azimuth. Need theta in [0, pi/2] and phi in [0, 2pi]
    => X = r sin(theta) cos(phi)
    => Y = r sin(theta) sin(phi)
    => Z = r cos(theta)

    For uniformly sampling w.r.t area, we have to take into account
    theta since Area varies according to it. Remeber dA = r^2 sin(theta) d(theta) d(phi)
    The PDF is constant and equals 1/2pi

    According to that we have the CDF for theta as
    => cos(theta) = 1 - r1    ----- where "r1" is a uniform random number in range [0,1]

    We can also simplify if we want.....
    => cos(theta) = r1     -------- since r1 is uniform in the range [0,1], (r1) and (1-r1) have the same probability.

    From there we can directly find sin(theta) instead of using inverse transform sampling to avoid expensive trigonometric functions.
    => sin(theta) = sqrt[ 1-cos^2(theta) ]
    => sin(theta) = sqrt[ 1 - r1^2 ]

    */

    float phi = 2*PI * r2;
    float sinTheta = sqrt(1 - r1*r1);   // uniform
    x = sinTheta * cos(phi);  // r * sin(theta) cos(phi)
    y = sinTheta * sin(phi);  // r * sin(theta) sin(phi)
    z = r1;             // r * cos(theta)  r = 1

    float4 ray_dir = (float4) (x, y, z, 0);
    ray->dir.x = dot(normal_to_world.r1, ray_dir);
    ray->dir.y = dot(normal_to_world.r2, ray_dir);
    ray->dir.z = dot(normal_to_world.r3, ray_dir);
    ray->dir.w = 0;

    ray->dir = normalize(ray->dir);
    ray->origin = hit_info.hit_point + ray->dir * EPSILON;
    ray->is_shadow_ray = false;
    ray->length = INFINITY;

    *pdf = 2* PI;
}

void cosineWeightedHemisphere(Ray* ray, float* pdf, HitInfo hit_info, uint* seed)
{
    /* Create a new coordinate system for Normal Space where Z aligns with normal. */
    Mat4x4 normal_to_world;
    float4 Ny, Nx, Nz;
    Nz = hit_info.normal;

    if ( fabs(Nz.y) > fabs(Nz.z) )
        Nx = (float4) (Nz.y, -Nz.x, 0, 0.f);
    else
        Nx = (float4) (Nz.z, 0, -Nz.x, 0.f);

    Nx = normalize(Nx);
    Ny = normalize(cross(Nz, Nx));

    normal_to_world.r1 = (float4) (Nx.x, Ny.x, Nz.x, hit_info.hit_point.x);
    normal_to_world.r2 = (float4) (Nx.y, Ny.y, Nz.y, hit_info.hit_point.y);
    normal_to_world.r3 = (float4) (Nx.z, Ny.z, Nz.z, hit_info.hit_point.z);
    normal_to_world.r4 = (float4) (Nx.w, Ny.w, Nz.w, hit_info.hit_point.w);

    float x, y, z, r1, r2;

    *seed = xor_shift(*seed);
    r1 = *seed / (float) UINT_MAX;
    *seed = xor_shift(*seed);
    r2 = *seed / (float) UINT_MAX;

    /*
    theta = inclination (from Y), phi = azimuth. Need theta in [0, pi/2] and phi in [0, 2pi]
    => X = r sin(theta) cos(phi)
    => Y = r sin(theta) sin(phi)
    => Z = r cos(theta)

    For cosine weighted sampling we have the PDF
    PDF = cos(theta)/pi
    Our equation then becomes,
    1/pi {double integral}{cos(theta) sin(theta) d(theta) d(phi)} = 1

    According to that we have the CDF for theta as
    => sin^2(theta) = r1    ----- where "r1" is a uniform random number in range [0,1]
    => sin(theta) = root(r1)
    => cos(theta) = root(1-r1)

    The CDF for phi is given as,
    => phi/2pi = r2
    => phi = 2 * pi * r2
    */

    float phi = 2 * PI * r2;
    float sinTheta = sqrt(r1);
    x = sinTheta * cos(phi);  // r * sin(theta) cos(phi)
    y = sinTheta * sin(phi);  // r * sin(theta) sin(phi)
    z = sqrt(1-r1);             // r * cos(theta),  r = 1

    float4 ray_dir = (float4) (x, y, z, 0);
    ray->dir.x = dot(normal_to_world.r1, ray_dir);
    ray->dir.y = dot(normal_to_world.r2, ray_dir);
    ray->dir.z = dot(normal_to_world.r3, ray_dir);
    ray->dir.w = 0;

    ray->dir = normalize(ray->dir);
    ray->origin = hit_info.hit_point + ray->dir * EPSILON;
    ray->is_shadow_ray = false;
    ray->length = INFINITY;

    *pdf = z * INV_PI;
}

float calcPhongPDF(float4 w_i, float4 w_o, HitInfo hit_info)
{
    float4 refl_dir;
    refl_dir = 2*(dot(w_o, hit_info.normal)) * hit_info.normal - w_o;
    refl_dir = normalize(refl_dir);
    
    float costheta = cos(dot(refl_dir, w_i));
    float phong_exponent;
    
    if(hit_info.sphere_ID > 0)
        phong_exponent = spheres[hit_info.sphere_ID].phong_exponent;
    else
        phong_exponent = walls[hit_info.quad_ID].phong_exponent;
    return (phong_exponent+1) * 0.5 * INV_PI * pow(costheta, phong_exponent);
    
    
}

float calcCosPDF(float4 w_i, HitInfo hit_info)
{
    return max(dot(w_i, hit_info.normal), 0.0f) * INV_PI;
}

float getYluminance(float4 color)
{
    return 0.212671f*color.x + 0.715160f*color.y + 0.072169f*color.z;
}

uint wang_hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

uint xor_shift(uint seed)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

void powerHeuristic(float* weight, float light_pdf, float brdf_pdf, int beta)
{
    *weight = (pown(*weight, beta)) / (pown(light_pdf, beta) + pown(brdf_pdf, beta) );  
}


