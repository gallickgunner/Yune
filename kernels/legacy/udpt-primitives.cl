#yune-preproc kernel-name pathtracer

#define PI              3.14159265359f
#define INV_PI          0.31830988618f
#define EPSILON         0.0001f
#define RR_THRESHOLD    6
#define WALL_SIZE       5
#define SPHERE_SIZE     2
#define LIGHT_SIZE      1
//#define MIS

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

    int sphereID, quadID, lightID;
    float4 hit_point;  // point of intersection.
    float4 normal;

} HitInfo;

typedef struct Sphere{

    float4 pos;
    float radius;
    int matID;

} Sphere;

typedef struct Quad{

    float4 pos;
    float4 normal;
    float4 edge_l;
    float4 edge_w;
    int matID;
} Quad;

typedef struct Light{

    float4 pos;
    float4 normal;
    float4 edge_l;
    float4 edge_w;
    float4 ke;
    float intensity;
} Light;

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

//For use with Triangle geometry.
typedef struct Material{

    float4 ke;
    float4 kd;
    float4 ks;
    float n;
    float k;
    float px;
    float py;
    float alpha_x;
    float alpha_y;
    int is_specular;
    int is_transmissive;    // total 80 bytes.
} Material;

typedef struct AABB{    
    float4 p_min;
    float4 p_max;
}AABB;

typedef struct BVHNodeGPU{
    AABB aabb;          //32 
    int vert_list[10]; //40
    int child_idx;      //4
    int vert_len;       //4 - total 80
} BVHNodeGPU;

typedef struct Camera{
    Mat4x4 view_mat;
    float view_plane_dist;  // total 68 bytes
    float pad[3];           // 12 bytes padding to reach 80 (next multiple of 16)
} Camera;

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                           CLK_ADDRESS_CLAMP_TO_EDGE   |
                           CLK_FILTER_NEAREST;

__constant Material materials[5] = 
{
    //Sphere 1
    {
        (float4)(0.f, 0.f, 0.f, 0.f),
        (float4)(0.711f, 0.501f, 0.3215f, 0.f),
        (float4)(0.2f, 0.2f, 0.2f, 0.f),
        1, 1, 20, 20, 20, 20, 0, 0
    },
    
    //Sphere 2
    {
        (float4)(0.f, 0.f, 0.f, 0.f),
        (float4)(0.9f, 0.9f, 0.9f, 0),
        (float4)(0.0f, 0.0f, 0.0f, 0),
        1, 1, 20, 20, 20, 20, 0, 0
    },
    
    //Grey walls
    {
        (float4)(0.f, 0.f, 0.f, 0.f),
        (float4)(0.9f, 0.9f, 0.9f, 0),
        (float4)(0.0f, 0.0f, 0.0f, 0),
        1, 1, 20, 50, 20, 20, 0, 0
    },
    
    //Red wall
    {
        (float4)(0.f, 0.f, 0.f, 0.f),
        (float4)(0.8f, 0.f, 0.f, 0),
        (float4)(0.0f, 0.0f, 0.0f, 0),
        1, 1, 20, 5, 20, 20, 0, 0
    },
    
    //Green wall
    {
        (float4)(0.f, 0.f, 0.f, 0.f),
        (float4)(0.0f, 0.8f, 0.f, 0),
        (float4)(0.0f, 0.0f, 0.0f, 0),
        1, 1, 20, 5, 20, 20, 0, 0
    }
};

__constant Sphere spheres[SPHERE_SIZE] = 
{  
    {  (float4)(1.f, -2.5f, -5.f, 1.f), 1.5, 0},
    {  (float4)(-5.f, -3.f, -8.f, 1.f), 1.0f, 1}
};

__constant Light light_sources[2] = 
{
    {  
        (float4)(-1.0f, 4.9f, -6.5f, 1.f),   //pos
        (float4)(0.f, -1.f, 0.f, 0.f),      //normal        
        (float4)(2.f, 0.f, 0.f, 0.f),       //edge_l
        (float4)(0.f, 0.f, 1.0f, 0.f),      //edge_w
        (float4)(1.0f, 1.0f, 1.0f, 0.f),    //emissive col
        28.f                                //intensity
    },

    {
        (float4)(-6.0f, 0.5f, -4.0f, 1.f),
        (float4)(1.f, 0.f, 0.f, 0.f),        
        (float4)(0.f, 2.f, 0.f, 0.f),
        (float4)(0.f, 0.f, -2.f, 0.f),
        (float4)(1.0f, 1.0f, 1.0f, 0.f),
        8.f
    }
};

__constant Quad walls[WALL_SIZE] = 
{ 
    {
        (float4)(-6.f, -4.f, -9.f, 1.f),     // bottom floor
        (float4)(0.f, 1.f, 0.f, 0.f),        
        (float4)(12.f, 0.f, 0.f, 0.f),
        (float4)(0.f, 0.f, 10.f, 0.f), 2
    },

    {
        (float4)(-6.f, 5.f, -9.f, 1.f),      // top wall
        (float4)(0.f, -1.f, 0.f, 0.f), 
        (float4)(12.f, 0.f, 0.f, 0.f),
        (float4)(0.f, 0.f, 10.f, 0.f), 2
    },

    {
        (float4)(6.f, 5.f, -9.f, 0.f),      //right wall
        (float4)(-1.f, 0.f, 0.f, 0.f), 
        (float4)(0.f, -9.f, 0.f, 0.f),
        (float4)(0.f, 0.f, 10.f, 0.f), 3
    },

    {
        (float4)(-6.f, 5.f, -9.f, 1.f),     //left wall
        (float4)(1.f, 0.f, 0.f, 0.f),     
        (float4)(0.f, -9.f, 0.f, 0.f),
        (float4)(0.f, 0.f, 10.f, 0.f), 4
    },

    {
        (float4)(-6.f, 5.f, -9.f, 0.f),     //back wall
        (float4)(0.f, 0.f, 1.f, 0.f),     
        (float4)(0.f, -9.f, 0.f, 0.f),
        (float4)(12.f, 0.f, 0.f, 0.f), 2
    }
};

__constant float4 SKY_COLOR =(float4) (0.588f, 0.88f, 1.0f, 1.0f);
__constant float4 BACKGROUND_COLOR =(float4) (0.4f, 0.4f, 0.4f, 1.0f);
__constant float4 PINK = (float4) (0.988f, 0.0588f, 0.7529f, 1.0f);

//Core Functions
void createRay(float pixel_x, float pixel_y, int img_width, int img_height, Ray* eye_ray, constant Camera* main_cam);
bool traceRay(Ray* ray, HitInfo* hit);
float4 shading(Ray ray, Ray light_ray, int GI_CHECK, uint* seed);
float4 evaluateDirectLighting(float4 w_o, HitInfo hit, uint* seed);
float4 evaluateBRDF(float4 w_i, float4 w_o, HitInfo hit_info, bool sample_glossy, float rr_prob);
float4 OrenNayarBRDF(float4 w_i, float4 w_o, HitInfo hit_info);
int sampleLights(HitInfo hit_info, float* light_pdf, float4* w_i, uint* seed);
float4 reflect(float4 w_i, HitInfo hit_info);
float4 refract(float4 w_i, HitInfo hit_info);
float evalFresnelReflectance(float4 w_i, HitInfo hit_info, float* ior_factor);

//Intersection Routiens
bool raySphereIntersection(Ray* ray, HitInfo* hit, int i);
bool rayQuadIntersection(Ray* ray, HitInfo* hit, int i);
bool rayLightIntersection(Ray* ray, HitInfo* hit, int i);

//Sampling Hemisphere Functions
void phongSampleHemisphere (Ray* ray, float* pdf, float4 w_i, HitInfo hit_info, uint* seed);
void uniformSampleHemisphere(Ray* ray, float* pdf, HitInfo hit_info, uint* seed);
void cosineWeightedHemisphere(Ray* ray, float* pdf, HitInfo hit_info, uint* seed);
void sampleFresnelIncidence(Ray* ray, HitInfo hit_info, float4 w_i, float* ior_factor, uint* seed);

//PDF functions to return PDF values provided a ray direction and for converting PDFs between solid angle/area
float calcPhongPDF(float4 w_i, float4 w_o, HitInfo hit_info);
float calcCosPDF(float4 w_i, float4 normal);
float convertPdfAngleToPdfArea(float pdf_angle, HitInfo curr, HitInfo next);

// Helper Functions
float4 inverseGammaCorrect(float4 color);
float4 toShadingSpace(float4 w, HitInfo hit_info);
float cosTheta(float4 w);
float cos2Theta(float4 w);
float sin2Theta(float4 w);
float sinTheta(float4 w);
float cosPhi(float4 w);
float sinPhi(float4 w);
float getYluminance(float4 color);
int getMatID(HitInfo hit);
uint wang_hash(uint seed);
uint xor_shift(uint seed);
void powerHeuristic(float* weight, float light_pdf, float brdf_pdf, int beta);
bool sampleGlossyPdf(HitInfo hit, uint* seed, float* prob);


/* Throught out the code, we use w_o as the inverse of the direction vector that hits the current surface. This points
 * towards the camera path. w_i represents the direction from the current surface to the next surface (towards the light source).
 * The only exception is for functions (reflect/refract, Sampling functions and fresnel reflectnace). Here the meaning is inverted. w_i represents the 
 * inverse of the "incident" direction which is actually w_o from the previous context.
 */
 
 
/***********************  KERNEL START  ************************/


__kernel void pathtracer(__write_only image2d_t outputImage, __read_only image2d_t inputImage, __constant Camera* main_cam, 
                         int scene_size, __global Triangle* vert_data, __global Material* mat_data, int bvh_size, __global BVHNodeGPU* bvh,
                         int GI_CHECK, int reset, uint rand, int block, int block_x, int block_y)
{
    int img_width = get_image_width(outputImage);
    int img_height = get_image_height(outputImage);
    int2 pixel = (int2)(get_global_id(0), get_global_id(1));
    
    pixel.x += ceil((float)img_width / block_x) * (block % block_x);
    pixel.y += ceil((float)img_height / block_y) * (block / block_x);
    
    if (pixel.x >= img_width || pixel.y >= img_height)
        return;
    
    //create a camera ray and light ray
    Ray eye_ray, light_ray;
    float r1, r2;
    uint seed = (pixel.y+1)* img_width + (pixel.x+1);
    seed =  rand * seed;
    seed = wang_hash(seed);
    //Since wang_hash can returns 0 and Xor Shift cant handle 0.    
    if(seed == 0)
        seed = wang_hash(seed);    
    
    float4 color = (float4) (0.f, 0.f, 0.f, 1.f);
    
    seed = xor_shift(seed);
    r1 = seed / (float) UINT_MAX;
    seed =  xor_shift(seed);
    r2 =  seed / (float) UINT_MAX;
    
    createRay(pixel.x + r1, pixel.y + r2, img_width, img_height, &eye_ray, main_cam);
    
    color = shading(eye_ray, light_ray, GI_CHECK, &seed);
    
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
    float4 dir;
    float aspect_ratio;
    aspect_ratio = (img_width*1.0)/img_height;

    dir.x = aspect_ratio *((2.0 * pixel_x/img_width) - 1);
    dir.y = (2.0 * pixel_y/img_height) -1 ;
    dir.z = -main_cam->view_plane_dist;
    dir.w = 0;    
    
    eye_ray->dir.x = dot(main_cam->view_mat.r1, dir);
    eye_ray->dir.y = dot(main_cam->view_mat.r2, dir);
    eye_ray->dir.z = dot(main_cam->view_mat.r3, dir);
    eye_ray->dir.w = dot(main_cam->view_mat.r4, dir);
    
    eye_ray->dir = normalize(eye_ray->dir);
    
    eye_ray->origin = (float4) (main_cam->view_mat.r1.w,
                                main_cam->view_mat.r2.w,
                                main_cam->view_mat.r3.w,
                                main_cam->view_mat.r4.w);
                                
    eye_ray->is_shadow_ray = false;
    eye_ray->length = INFINITY;                             
}

bool traceRay(Ray* ray, HitInfo* hit)
{
    bool flag = false;
    
    for(int i = 0; i < LIGHT_SIZE; i++)
    {       
        flag |= rayLightIntersection(ray, hit, i);
        if(flag && ray->is_shadow_ray)
            return flag;
    }
    
    for(int i = 0; i < SPHERE_SIZE; i++)
    {
        flag |= raySphereIntersection(ray, hit, i);
        if(flag && ray->is_shadow_ray)
            return flag;
    }   
    
    for(int i = 0; i < WALL_SIZE; i++)
    {       
        flag |= rayQuadIntersection(ray, hit, i);
        if(flag && ray->is_shadow_ray)
            return flag;
    }
    
    return flag;
}

bool raySphereIntersection(Ray* ray, HitInfo* hit, int i)
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
        hit->lightID = -1;
        hit->sphereID = i;
        hit->hit_point = ray->origin + (ray->dir * ray->length);
        hit->normal = normalize((hit->hit_point - spheres[i].pos));
        return true;
    }
    return false;
}

bool rayQuadIntersection(Ray* ray, HitInfo* hit, int i)
{
    float DdotN = dot(ray->dir, walls[i].normal);
    if(fabs(DdotN) > 0.0001)
    {
        float t = dot(walls[i].normal, walls[i].pos - ray->origin) / DdotN;
        if(t>0.0 && t < ray->length)
        {
            float proj1, proj2, la, lb;
            float4 temp;
            
            temp = ray->origin + (ray->dir * t);
            temp = temp - walls[i].pos;
            proj1 = dot(temp, walls[i].edge_l);
            proj2 = dot(temp, walls[i].edge_w);             
            la = length(walls[i].edge_l);
            lb = length(walls[i].edge_w);
            
            // Projection of the vector from rectangle corner to hitpoint on the edges.
            proj1 /= la;    
            proj2 /= lb;
            
            if( (proj1 >= 0.0 && proj2 >= 0.0)  && (proj1 <= la && proj2 <= lb)  )
            {
                hit->hit_point = ray->origin + (ray->dir * t);
                hit->sphereID = -1;
                hit->lightID = -1;
                hit->quadID = i;
                hit->normal = walls[i].normal;                
                ray->length = t;  
                return true;
            }
        }
    }
    return false;
}

bool rayLightIntersection(Ray* ray, HitInfo* hit, int i)
{
    float DdotN = dot(ray->dir, light_sources[i].normal);
    if(fabs(DdotN) > 0.0001)
    {
        float t = dot(light_sources[i].normal, light_sources[i].pos - ray->origin) / DdotN;
        if(t>0.0 && t < ray->length)
        {
            float proj1, proj2, la, lb;
            float4 temp;
            
            temp = ray->origin + (ray->dir * t);
            temp = temp - light_sources[i].pos;
            proj1 = dot(temp, light_sources[i].edge_l);
            proj2 = dot(temp, light_sources[i].edge_w);             
            la = length(light_sources[i].edge_l);
            lb = length(light_sources[i].edge_w);
            
            // Projection of the vector from rectangle corner to hitpoint on the edges.
            proj1 /= la;    
            proj2 /= lb;
            
            if( (proj1 >= 0.0 && proj2 >= 0.0)  && (proj1 <= la && proj2 <= lb)  )
            {
                hit->hit_point = ray->origin + (ray->dir * t);
                hit->sphereID = -1;                    
                hit->quadID = -1;
                hit->lightID = i;
                hit->normal = light_sources[i].normal;                
                ray->length = t;  
                return true;
            }
        }
    }
    return false;
}

float4 shading(Ray ray, Ray light_ray, int GI_CHECK, uint* seed)
{   
    HitInfo hit_info = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};

    if(!traceRay(&ray, &hit_info))
        return BACKGROUND_COLOR;
    
    if(hit_info.lightID >= 0)
    {
        //if(dot(ray.dir, light_sources[hit_info.lightID].normal) < 0)
            return (float4)(1,1,1,1);
        //else
        //    return (float4)(0.1,.1,.1,1);
    }
    
    float4 direct_color, indirect_color, throughput;    
    throughput = (float4) (1.f, 1.f, 1.f, 1.f);
    direct_color = (float4) (0.f, 0.f, 0.f, 0.f);   
    indirect_color = direct_color;
    
    // Compute Direct Illumination at the first hitpoint.
    int matID =  getMatID(hit_info);
    if(!materials[matID].is_specular)
        direct_color =  evaluateDirectLighting(-ray.dir, hit_info, seed);
    
    //Compute Indirect Illumination bouncing rays around.
    if(GI_CHECK)
    { 
        // Bug on AMD platforms. While(true) and breaking explicitly yields darker images.
        //while(true)
        float r = 0.0f;
        for(int i = 0; i < 100000; i++)        
        {
            HitInfo new_hitinfo = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
            Ray new_ray;
            float pdf = 1, cosine_falloff, brdf_prob = 0.0f, ior_factor = 1.0f;
            bool is_refract = false, is_glossy = false;
            if(materials[matID].is_specular)
                sampleFresnelIncidence(&new_ray, hit_info, -ray.dir, &ior_factor, seed);
            else
            {
                is_glossy = sampleGlossyPdf(hit_info, seed, &brdf_prob);
            
                if(brdf_prob == 0.0f)
                    break;
                
                if(is_glossy)
                    phongSampleHemisphere(&new_ray, &pdf, -ray.dir, hit_info, seed);             
                else
                    cosineWeightedHemisphere(&new_ray, &pdf, hit_info, seed);               
            }            
            
            // If GI_ray hits nothing or pdf is zero or if ray hits light source return.
            // Note that in normal path tracing we would have skipped an iteration upon hitting a light source. But in Progressive path tracing
            // we return i.e. we ignore the sample completely since we are taking samples continuously.
            // Also Account for the light source if the object hit is specular.
            if(pdf <= 0.0f || !traceRay(&new_ray, &new_hitinfo) || new_hitinfo.lightID >= 0)
            {
                if(materials[matID].is_specular && new_hitinfo.lightID >= 0)
                {
                    indirect_color += throughput * light_sources[new_hitinfo.lightID].ke;
                }                
                break;
            }
            
            //Add emission term of the new object hit.     
            if(hit_info.quadID >= 0)
                indirect_color += throughput * materials[walls[new_hitinfo.quadID].matID].ke;
            else
                indirect_color += throughput * materials[spheres[new_hitinfo.sphereID].matID].ke;
            
            
            //If it hits object, accumulate the brdf  and geometry terms.  
            
            /* If material is specular or transmissive multiply by IOR factor (which contains non-1 values for refraction only case). The fresenel term gets cancelled out
             * when divided by the same probability of following a reflection or refraction path. The cos(theta) term gets cancelled due to the specular/transmissive bsdf.
             * The delta term gets cancelled by the pdf.
             */
            if(materials[matID].is_specular)          
                throughput = throughput * ior_factor;
            else
                throughput = throughput * evaluateBRDF(new_ray.dir, -ray.dir, hit_info, is_glossy, brdf_prob) * fmax(dot(new_ray.dir, hit_info.normal), 0.0f) / pdf; 
            
            /* Russian Roulette: Use RR after few bounces. Terminate paths with low enough value of throughput. We use Y luminance as the value
             * that the path survives. The termination probability is (1 - Yluminance). If the path survives boost the energy by
             * (1/p)
             */
             
            if(i > RR_THRESHOLD)
            {       
                float p = min(getYluminance(throughput), 0.95f);
                *seed = xor_shift(*seed);
                r = *seed / (float) UINT_MAX;   
                
                if(r >= p)
                    break;              
                throughput *= 1/p;
            }
            
            matID = getMatID(hit_info);            
            if(!materials[matID].is_specular)
                indirect_color +=  throughput * evaluateDirectLighting(-new_ray.dir, new_hitinfo, seed);
            hit_info = new_hitinfo;
            ray = new_ray;            
        }
    }
    return direct_color + indirect_color;
}

float4 evaluateDirectLighting(float4 w_o, HitInfo hit, uint* seed)
{
    int matID = getMatID(hit);    
    float4 emission = materials[matID].ke;
    float4 light_sample = (float4) (0.f, 0.f, 0.f, 0.f);
    float4 w_i;
    float light_pdf, brdf_prob = 0.0f; 
    bool sample_glossy = false;
    
    int j = sampleLights(hit, &light_pdf, &w_i, seed);
	if(j == -1 || light_pdf <= 0.0f)
		return emission;
	
	float len = length(w_i);
    len -= EPSILON*1.2f;
    w_i = normalize(w_i);
    
	Ray shadow_ray = {hit.hit_point + w_i * EPSILON, w_i, INFINITY, true};
	HitInfo shadow_hitinfo = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
    shadow_ray.length = len; 
    
    //Direct Light Sampling     
	//If ray doesn't hit anything (exclude light source j while intersection check). This means light source j visible.
    if(!traceRay(&shadow_ray, &shadow_hitinfo))
    {
        sample_glossy = sampleGlossyPdf(hit, seed, &brdf_prob);
        if(brdf_prob == 0.0f)
            return emission;
		light_sample = evaluateBRDF(w_i, w_o, hit, sample_glossy, brdf_prob) * light_sources[j].ke * light_sources[j].intensity * fmax(dot(w_i, hit.normal), 0.0f);
		light_sample *= 1/light_pdf;
	}
	
    #ifndef MIS    
	return light_sample + emission;
    
    //We use MIS with Power Heuristic. The exponent can be set to 1 for balance heuristic.   
	#else
    float4 brdf_sample = (float4) (0.f, 0.f, 0.f, 0.f);
    float brdf_pdf, mis_weight;
    
    //Compute MIS weighted Light sample
    if(sample_glossy)
        brdf_pdf = calcPhongPDF(w_i, w_o, hit);
    else
        brdf_pdf = calcCosPDF(w_i, hit.normal);
    
    mis_weight =  light_pdf;     
    powerHeuristic(&mis_weight, light_pdf, brdf_pdf, 2);
    light_sample *= mis_weight;
    
    // BRDF Sampling
    Ray brdf_sample_ray;
    
    if(sample_glossy)
        phongSampleHemisphere(&brdf_sample_ray, &brdf_pdf, w_o, hit, seed);
    else
        cosineWeightedHemisphere(&brdf_sample_ray, &brdf_pdf, hit, seed);
    
    if(brdf_pdf <= 0.0f)
        return light_sample + emission;                
    
    w_i = brdf_sample_ray.dir;  
    HitInfo new_hitinfo = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};   
    
    //If traceRay doesnt hit anything or if it does not hit the same light source return only light sample.
    if(!traceRay(&brdf_sample_ray, &new_hitinfo) || new_hitinfo.lightID != j)
       return light_sample + emission;
    
    mis_weight = brdf_pdf;
    powerHeuristic(&mis_weight, light_pdf, brdf_pdf, 2);
    brdf_sample =  evaluateBRDF(w_i, w_o, hit, sample_glossy, brdf_prob) * light_sources[j].ke * light_sources[j].intensity * fmax(dot(w_i, hit.normal), 0.0f);
    brdf_sample *=  mis_weight / brdf_pdf;
    
    return (light_sample + brdf_sample + emission);
    #endif
}

float4 evaluateBRDF(float4 w_i, float4 w_o, HitInfo hit_info, bool sample_glossy, float rr_prob)
{
    float4 color, refl_vec;
    
    refl_vec = reflect(w_i, hit_info);
    refl_vec = normalize(refl_vec);
    
    int matID = getMatID(hit_info);    
    if(!sample_glossy)
    {
        if(rr_prob == 1.0f)
            color = OrenNayarBRDF(w_i, w_o, hit_info);
        else
            color = materials[matID].kd * INV_PI / rr_prob;
    }
    else
    {     
        int phong_exp = materials[matID].px * materials[matID].py;
        color = materials[matID].ks *  pow(fmax(dot(w_o, refl_vec), 0.0f), phong_exp) * (phong_exp + 2) * INV_PI * 0.5f / rr_prob;
    }        
    return color;     
}

float4 OrenNayarBRDF(float4 w_i, float4 w_o, HitInfo hit_info)
{
    w_i = toShadingSpace(w_i, hit_info);
    w_o = toShadingSpace(w_o, hit_info);
    float sigma_sq, A, B;
    int matID = getMatID(hit_info);
    
    float costerm = 0.0f;
    if(sinTheta(w_i) >= EPSILON && sinTheta(w_o) >= EPSILON)
        costerm = fmax(0.0f, ((cosPhi(w_i) * cosPhi(w_o)) + (sinPhi(w_i) * sinPhi(w_o))));
    float sin_alpha, tan_beta;
    if(fabs(cosTheta(w_i)) < fabs(cosTheta(w_o)))
    {
        sin_alpha = sinTheta(w_i);
        tan_beta = sinTheta(w_o)/fabs(cosTheta(w_o));
    }
    else
    {
        sin_alpha = sinTheta(w_o);
        tan_beta = sinTheta(w_i)/fabs(cosTheta(w_i));
    }
    
    /* Note although alpha_x and alpha_y are parameters in Microfacet Distribution Function where microfacets are considered perfectly specular/transmissive (RMS Slope), 
     * we abuse the term to store "sigma" which is the standard deviation of the angles of microfacet used in ON Model so we don't have to introduce new variable.
     */
    sigma_sq = materials[matID].alpha_x;
    A =  1 - (sigma_sq/(2*(sigma_sq + 0.33)));
    B = 0.45* sigma_sq / (sigma_sq + 0.09);        
    return materials[matID].kd * INV_PI * (A + B * (costerm * sin_alpha * tan_beta));    
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
        
        cosine_falloff = max(dot(temp_wi, hit_info.normal), 0.0f) * max(dot(-temp_wi, light_sources[i].normal), 0.0f);
        
        if(cosine_falloff <= 0.0)
        {
            weights[i] = 0;
            continue;
        }
        area = length(light_sources[i].edge_l) * length(light_sources[i].edge_w);
        
        if(LIGHT_SIZE == 1)
        {
            *w_i = w_is[i];
            *light_pdf = 1/area;
            *light_pdf *= distance / fmax(dot(-temp_wi, light_sources[i].normal), 0.0f);
            return i;
        }
        
        weights[i] = length(light_sources[i].ke) * cosine_falloff * area/ distance;
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
            *w_i = w_is[i];
            *light_pdf = (weight/area);
            
            //Convert PDF w.r.t area measure to PDF w.r.t solid angle. MIS needs everything to be in the same domain (either dA or dw)
            *light_pdf *=  (dot(*w_i, *w_i) / (fmax(dot(-normalize(*w_i), light_sources[i].normal), 0.0f) ));
            return i;
        }
        cumulative_weight += weight;
    }
}

void phongSampleHemisphere (Ray* ray, float* pdf, float4 w_i, HitInfo hit_info, uint* seed)
{
    /* Create a new coordinate system for Normal Space where Z aligns with Reflection Direction. */
    Mat4x4 normal_to_world;
    float4 Ny, Nx, Nz;
    
    // w_i is the inverse of the direction to the current surface.
    Nz = reflect(w_i, hit_info);
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
    theta = inclination (from Z), phi = azimuth. Need theta in [0, pi/2] and phi in [0, 2pi]
    => X = r sin(theta) cos(phi)
    => Y = r sin(theta) sin(phi)
    => Z = r cos(theta)

    Phong PDF is  (n+1) * cos^n(alpha)/2pi  where alpha is the angle between reflection dir and w_o(the new ray being sampled).
    Since reflection direction aligned with Z axis, alpha equals theta. Formula is,

    (alpha, phi) = (acos(r1^(1/(n+1))), 2pi*r2)
    */
    int phong_exponent;
    int matID = getMatID(hit_info);
    phong_exponent = materials[matID].px * materials[matID].py;

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
    
    //If a ray was sampled in the lower hemisphere, set pdf = 0
    if(dot(ray->dir, hit_info.normal) < 0)
        *pdf = 0;
    else
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
    theta = inclination (from Z), phi = azimuth. Need theta in [0, pi/2] and phi in [0, 2pi]
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
    theta = inclination (from Z), phi = azimuth. Need theta in [0, pi/2] and phi in [0, 2pi]
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

void sampleFresnelIncidence(Ray* ray, HitInfo hit_info, float4 w_i, float* ior_factor, uint* seed)
{
    int matID = getMatID(hit_info);
    float r, pdf;
    //If material is refractive as well, calc Fresnel Reflectance.
    if(materials[matID].is_transmissive)
    {
        pdf = evalFresnelReflectance(w_i, hit_info, ior_factor);        
        if(pdf == 1.0)
        {
            ray->dir = reflect(w_i, hit_info);
            *ior_factor = 1.0f;
        }
        else
        {
            *seed = xor_shift(*seed);
            r = *seed / (float) UINT_MAX;
            if(r < pdf)
            {
                ray->dir = reflect(w_i, hit_info);
                *ior_factor = 1.0f;
            }
            else
                ray->dir = refract(w_i, hit_info);
        }
    }
    // Else material is perfect mirror.
    else
    {
        *ior_factor = 1.0f;
        ray->dir = reflect(w_i, hit_info);
    }
    ray->length = INFINITY;
    ray->is_shadow_ray = false;
    ray->origin = hit_info.hit_point + ray->dir * EPSILON;
}

float4 reflect(float4 w_i, HitInfo hit_info)
{
    //Note w_i here means the incident direction (reversed so it originates from the surface).     
    float4 refl_vec;
    float4 normal = hit_info.normal;
    if(dot(w_i, normal) < 0)
        normal *= -1.0f;
    
    // w_i is the inverse of the direction to the current surface.
    refl_vec = 2*(dot(w_i, normal)) * normal - w_i;
    refl_vec = normalize(refl_vec);
    return refl_vec;
}

float4 refract(float4 w_i, HitInfo hit_info)
{
    //Note w_i here means the incident direction (reversed so it originates from the surface). In traditional pathtracing
    // this is actually the w_o since it goes from the surface towards the camera.
    
    float4 refr_vec;
    float4 normal = hit_info.normal;
    float n1, n2;  // n1 = IOR of incident medium, n2 = other
    int matID = getMatID(hit_info);
    
    //If theta < 0 this means we are already inside the object. Hence n1 should equal ior of the object rather than air.
    if(dot(w_i, normal) < 0)
    {
        n1 = materials[matID].n;
        n2 = 1;
        normal *= -1.0f;
    }
    else
    {
        n1 = 1;
        n2 = materials[matID].n;
    }
    
    float4 wt_perp = n1/n2 * (dot(w_i, normal) * normal - w_i);
    float4 wt_parallel = sqrt(1 - length(wt_perp) * length(wt_perp)) * -normal;
    refr_vec = normalize(wt_perp + wt_parallel);  
    return refr_vec;
}

float evalFresnelReflectance(float4 w_i, HitInfo hit_info, float* ior_factor)
{
    //Note w_i here means the incident direction (reversed so it originates from the surface). In traditional pathtracing
    // this is actually the w_o since it goes from the surface towards the camera.
    
    float4 normal = hit_info.normal;
    float n1, n2;  // n1 = IOR of incident medium, n2 = other
    int matID = getMatID(hit_info);
    
    //If theta < 0 this means we are already inside the object. Hence n1 should equal ior of the object rather than air.    
    if(dot(w_i, normal) < 0)
    {
        n1 = materials[matID].n;
        n2 = 1;
        normal *= -1.0f;
    }
    else
    {
        n1 = 1;
        n2 = materials[matID].n;
    }
    
    float cosThetaI = dot(w_i, normal);
    
    float sinThetaI = sqrt(1 - cosThetaI*cosThetaI);
    float sinThetaT = n1 * sinThetaI / n2;
    float cosThetaT =  sqrt(1 - sinThetaT*sinThetaT);
    
    if(sinThetaT >= 1.0f && n1 > n2)
        return 1.0f;
    
    float4 ks = materials[matID].ks;
    float r0 =  ks.x + ks.y + ks.z;
    r0 /= 3.0f;
    *ior_factor = (n2*n2) / (n1*n1);
    if(n1 > n2)
        return (r0 + (1-r0) * (1- pow(cosThetaI,5.0f)));
    else
        return (r0 + (1-r0) * (1- pow(cosThetaT,5.0f)));
}

float calcPhongPDF(float4 w_i, float4 w_o, HitInfo hit_info)
{
    float4 refl_dir;
    refl_dir = 2*(dot(w_o, hit_info.normal)) * hit_info.normal - w_o;
    refl_dir = normalize(refl_dir);
    
    float costheta = fmax(0.0f, cos(dot(refl_dir, w_i)));
    float phong_exponent;
    int matID = getMatID(hit_info);
    phong_exponent = materials[matID].px + materials[matID].py;
    
    return (phong_exponent+1) * 0.5 * INV_PI * pow(costheta, phong_exponent);    
}

float calcCosPDF(float4 w_i, float4 normal)
{
    return fmax(dot(w_i, normal), 0.0f) * INV_PI;
}

float4 toShadingSpace(float4 w, HitInfo hit_info)
{
    /* Remember we need World to normal. FOr that we need to invert normal to world which is easily obtainable
     * This is done be decomposing normal-to-world matrix into  Translation * Rotation matrix.  
     *  Inv(NtoW) = Inv(T*R)
     *            = Inv(R) * Inv(T)
     *            =  Transpose(R) * -T
     */
     
    Mat4x4 world_to_normal; 
    float4 Ny, Nx, Nz;
    Nz = hit_info.normal;

    if ( fabs(Nz.y) > fabs(Nz.z) )
        Nx = (float4) (Nz.y, -Nz.x, 0, 0.f);
    else
        Nx = (float4) (Nz.z, 0, -Nz.x, 0.f);

    Nx = normalize(Nx);
    Ny = normalize(cross(Nz, Nx));
    
    world_to_normal.r1 = (float4) (Nx.x, Nx.y, Nx.z, dot(hit_info.hit_point, Nx));
    world_to_normal.r2 = (float4) (Ny.x, Ny.y, Ny.z, dot(hit_info.hit_point, Ny));
    world_to_normal.r3 = (float4) (Nz.x, Nz.y, Nz.z, dot(hit_info.hit_point, Nz));
    world_to_normal.r4 = (float4) (0.f, 0.f, 0.f, 1.0f);
    
    return (float4)(dot(world_to_normal.r1, w), dot(world_to_normal.r2, w), dot(world_to_normal.r3, w), dot(world_to_normal.r4, w));
}

float cosTheta(float4 w)
{
    return w.z;
}

float cos2Theta(float4 w)
{
    return w.z * w.z;
}

float sin2Theta(float4 w)
{
    return (1 - cos2Theta(w));
}

float sinTheta(float4 w)
{
    return sqrt(sin2Theta(w));
}

float cosPhi(float4 w)
{
    float sintheta = sinTheta(w);
    if(sintheta <= EPSILON && sintheta >= -EPSILON)
        return 0;
    return clamp(w.x/sintheta, -1.0f, 1.0f);
}

float sinPhi(float4 w)
{
    float sintheta = sinTheta(w);
    if(sintheta <= EPSILON && sintheta >= -EPSILON)
        return 0;
    return clamp(w.y/sintheta, -1.0f, 1.0f);
}


float convertPdfAngleToPdfArea(float pdf_angle, HitInfo curr, HitInfo next)
{
    float pdf_area;
    float4 dir = next.hit_point - curr.hit_point;    
    float dist = length(dir) * length(dir);
    dir = normalize(dir);
    pdf_area = pdf_angle * fmax(0.0f, dot(next.normal, -dir)) / dist ;
    return pdf_area;    
}

bool sampleGlossyPdf(HitInfo hit, uint* seed, float* prob)
{
    *seed = xor_shift(*seed);
    float r = *seed / (float) UINT_MAX;  
    
    float4 ks, kd, sum;
    float pd, ps; 
    int matID = getMatID(hit);
    ks = materials[matID].ks;
    kd = materials[matID].kd;
    
    if(length(ks.xyz) == 0.0f)
    {
        *prob = 1.0f;
        return false;        
    }
    else if(length(kd.xyz) == 0.0f || kd.x + kd.y + kd.z == 0.0f)
    {
        *prob = 1.0f;
        return true;        
    }
    
    sum = ks + kd;
    float max_val = max(sum.x, max(sum.y, sum.z));
    
    if(max_val == sum.x)
    {
        pd = kd.x;
        ps = ks.x;
    }
    else if(max_val == sum.y)
    {
        pd = kd.y;
        ps = ks.y;
    }
    else
    {
        pd = kd.z;
        ps = ks.z;
    }
    
    
    if(r < pd)
    {
        *prob = pd;
        return false;
    }
    else if ( r < pd + ps && r >= pd)
    {
        *prob = ps;
        return true;        
    }
    else
    {
        *prob = 0;
        return false;
    }
}

int getMatID(HitInfo hit)
{
    if(hit.quadID >= 0)
        return walls[hit.quadID].matID;
    else
        return spheres[hit.sphereID].matID;
}

float getYluminance(float4 color)
{
    return 0.212671f*color.x + 0.715160f*color.y + 0.072169f*color.z;
}

float4 inverseGammaCorrect(float4 color)
{
    return pow(color, (float4) 2.2f);
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

void powerHeuristic(float* weight, float pdf1, float pdf2, int beta)
{
    *weight = (pown(*weight, beta)) / (pown(pdf1, beta) + pown(pdf2, beta) );  
}