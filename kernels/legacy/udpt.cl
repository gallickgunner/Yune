#define PI              3.14159265359f
#define INV_PI          0.31830988618f
#define EPSILON         0.0001f
#define RR_THRESHOLD    6
#define LIGHT_SIZE      1
#define HEAP_SIZE       1500
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

    int  triangle_ID, light_ID;
    float4 hit_point;  // point of intersection.
    float4 normal;

} HitInfo;

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

//For use with Triangle geometry.
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

__constant Quad light_sources[LIGHT_SIZE] = { {     (float4)(-0.1979f, 0.92f, -3.1972f, 1.f),
                                                    (float4)(0.f, -1.f, 0.f, 0.f),      //norm
                                                    (float4)(16.f, 16.f, 16.f, 0.f),    //emissive col
                                                    (float4)(0.f, 0.f, 0.f, 0.f),       //diffuse col
                                                    (float4)(0.f, 0.f, 0.f, 0.f),       //specular col
                                                    (float4)(0.4f, 0.f, 0.f, 0.f),       //edge_l
                                                    (float4)(0.f, 0.f, 0.4f, 0.f),       //edge_w
                                                    0.f                                  //phong exponent
                                               }
                                            };

__constant float4 SKY_COLOR =(float4) (0.588f, 0.88f, 1.0f, 1.0f);
__constant float4 BACKGROUND_COLOR =(float4) (0.4f, 0.4f, 0.4f, 1.0f);
__constant float4 PINK = (float4) (0.988f, 0.0588f, 0.7529f, 1.0f);

//Core Functions
void createRay(float pixel_x, float pixel_y, int img_width, int img_height, Ray* eye_ray, constant Camera* main_cam);
bool traceRay(Ray* ray, HitInfo* hit, int bvh_size, __global BVHNodeGPU* bvh, int scene_size, __global Triangle* scene_data);
float4 shading(int2 pixel, Ray ray, Ray light_ray, int GI_CHECK, uint* seed, int bvh_size, __global BVHNodeGPU* bvh, int scene_size, __global Triangle* scene_data,  __global Material* mat_data);
float4 evaluateDirectLighting(int2 pixel, float4 w_o, HitInfo hit, uint* seed, int bvh_size, __global BVHNodeGPU* bvh, int scene_size, __global Triangle* scene_data,  __global Material* mat_data);
float4 evaluateBRDF(float4 w_i, float4 w_o, HitInfo hit_info, bool sample_glossy, float rr_prob, __global Triangle* scene_data, __global Material* mat_data );
int sampleLights(HitInfo hit_info, float* light_pdf, float4* w_i, uint* seed);
float4 reflect(float4 w_i, HitInfo hit_info);
float4 refract(float4 w_i, HitInfo hit_info, __global Triangle* scene_data, __global Material* mat_data);
float evalFresnelReflectance(float4 w_i, HitInfo hit_info, float* ior_factor, __global Triangle* scene_data, __global Material* mat_data);

//Intersection Routiens
bool rayAabbIntersection(Ray* ray, AABB bb);
bool traverseBVH(Ray* ray, HitInfo* hit_info, int bvh_size, __global BVHNodeGPU* bvh, __global Triangle* scene_data);
bool rayTriangleIntersection(Ray* ray, HitInfo* hit, __global Triangle* scene_data, int idx);

//Sampling Hemisphere Functions
void phongSampleHemisphere (Ray* ray, float* pdf, float4 w_i, HitInfo hit_info, uint* seed,  __global Triangle* scene_data, __global Material* mat_data);
void uniformSampleHemisphere(Ray* ray, float* pdf, HitInfo hit_info, uint* seed);
void cosineWeightedHemisphere(Ray* ray, float* pdf, HitInfo hit_info, uint* seed);
void sampleFresnelIncidence(Ray* ray, HitInfo hit_info, float4 w_i, float* ior_factor, uint* seed, __global Triangle* scene_data, __global Material* mat_data);

//PDF functions to return PDF values provided a ray direction and for converting PDFs between solid angle/area
float calcPhongPDF(float4 w_i, float4 w_o, HitInfo hit_info, __global Triangle* scene_data, __global Material* mat_data);
float calcCosPDF(float4 w_i, float4 normal);
float convertPdfAngleToPdfArea(float pdf_angle, HitInfo curr, HitInfo next);

// Helper Functions
float4 inverseGammaCorrect(float4 color);
float getYluminance(float4 color);
uint wang_hash(uint seed);
uint xor_shift(uint seed);
void powerHeuristic(float* weight, float light_pdf, float brdf_pdf, int beta);
bool sampleGlossyPdf(HitInfo hit, __global Triangle* scene_data, __global Material* mat_data, uint* seed, float* prob);


/* Throught out the code, we use w_o as the inverse of the direction vector that hits the current surface. This points
 * towards the camera path. w_i represents the direction from the current surface to the next surface (towards the light source).
 * The only exception is for functions (reflect/refract, Sampling functions and fresnel reflectnace). Here the meaning is inverted. w_i represents the 
 * inverse of the "incident" direction which is actually w_o from the previous context.
 */
 
 
/***********************  KERNEL START  ************************/


__kernel void pathtracer(__write_only image2d_t outputImage, __read_only image2d_t inputImage, __constant Camera* main_cam, 
                         int scene_size, __global Triangle* scene_data, __global Material* mat_data, int bvh_size, __global BVHNodeGPU* bvh,
                         int GI_CHECK, int reset, uint rand, int block)
{
    int img_width = get_image_width(outputImage);
    int img_height = get_image_height(outputImage);
    int2 pixel = (int2)(get_global_id(0), get_global_id(1));
    
    if(block == 1)
    {            
        if (pixel.x >= img_width/2 || pixel.y >= img_height/2)
            return;
    }
    else if(block == 2)
    {
        pixel.x += img_width/2;
        if (pixel.x >= img_width || pixel.y >= img_height/2)
            return;
    }
    else if(block == 3)
    {   
        pixel.y += img_height/2;
        if (pixel.x >= img_width/2 || pixel.y >= img_height)
            return;
    }
    else if(block == 4)
    {
        pixel.x += img_width/2;
        pixel.y += img_height/2;
        if (pixel.x >= img_width || pixel.y >= img_height)
            return;
    }
    
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
    
    color = shading(pixel, eye_ray, light_ray, GI_CHECK ,&seed, bvh_size, bvh, scene_size, scene_data, mat_data);
   
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

bool traceRay(Ray* ray, HitInfo* hit, int bvh_size, __global BVHNodeGPU* bvh, int scene_size, __global Triangle* scene_data)
{
    bool flag = false;
    
    for(int i = 0; i < LIGHT_SIZE; i++)
    {       
        float DdotN = dot(ray->dir, light_sources[i].norm);
        if(fabs(DdotN) > 0.0001)
        {
            float t = dot(light_sources[i].norm, light_sources[i].pos - ray->origin) / DdotN;            
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
                    ray->length = t;
                    hit->hit_point = ray->origin + (ray->dir * t);
					hit->light_ID = i;
                    hit->triangle_ID = -1;
                    flag = true;
				}     
            }
        }       
    }
    //Traverse BVH if present, else brute force intersect all triangles...
    if(bvh_size > 0)
        flag |= traverseBVH(ray, hit, bvh_size, bvh, scene_data);
    else
    {
        for (int i =0 ; i < scene_size; i++)
            flag |= rayTriangleIntersection(ray, hit, scene_data, i);       
    }
    return flag;
}

bool traverseBVH(Ray* ray, HitInfo* hit, int bvh_size, __global BVHNodeGPU* bvh, __global Triangle* scene_data)
{
    int candidate_list[HEAP_SIZE];    
    candidate_list[0] = 0;
    int len = 1;
    bool intersect = false;
    
    if(!rayAabbIntersection(ray, bvh[0].aabb))
        return intersect;
    
    for(int i = 0; i < len && len < HEAP_SIZE; i++)
    {        
        float c_idx = bvh[candidate_list[i]].child_idx;
        if(c_idx == -1 && bvh[candidate_list[i]].vert_len > 0)
        {
            for(int j = 0; j < bvh[candidate_list[i]].vert_len; j++)
            {
                intersect |= rayTriangleIntersection(ray, hit, scene_data, bvh[candidate_list[i]].vert_list[j]);
                //If shadow ray don't need to compute further intersections...
                if(ray->is_shadow_ray && intersect)
                    return true;
            }
            continue;
        }
        
        for(int j = c_idx; j < c_idx + 2; j++)
        {
            AABB bb = {bvh[j].aabb.p_min, bvh[j].aabb.p_max};
            if((bvh[j].vert_len > 0 || bvh[j].child_idx > 0) && rayAabbIntersection(ray, bb))
            {               
                candidate_list[len] = j;
                len++;
            }
        }
    }
    return intersect;
}

bool rayTriangleIntersection(Ray* ray, HitInfo* hit, __global Triangle* scene_data, int idx)
{
    float4 v1v2 = scene_data[idx].v2 - scene_data[idx].v1; 
    float4 v1v3 = scene_data[idx].v3 - scene_data[idx].v1;
    
    float4 pvec = cross(ray->dir, v1v3);
    float det = dot(v1v2, pvec); 
    
    float inv_det = 1.0f/det;
    float4 dist = ray->origin - scene_data[idx].v1;
    float u = dot(pvec, dist) * inv_det;
    
    if(u < 0.0 || u > 1.0f)
        return false;
    
    float4 qvec = cross(dist, v1v2);
    float v = dot(qvec, ray->dir) * inv_det;
    
    if(v < 0.0 || u+v > 1.0)
        return false;
    
    float t = dot(v1v3, qvec) * inv_det;
    
    //BackFace Culling Algo
    /*
    if(det < EPSILON)
        continue;
    
    float4 dist = ray->origin - scene_data[idx].v1;
    float u = dot(pvec, dist);
    
    if(u < 0.0 || u > det)
        continue;
    
    float4 qvec = cross(dist, v1v2);
    float v = dot(qvec, ray->dir);
    
    if(v < 0.0 || u+v > det)
        continue;
    
    float t = dot(v1v3, qvec);
    
    float inv_det = 1.0f/det;
    t *= inv_det;
    u *= inv_det;
    v *= inv_det;*/
    
    if ( t > 0 && t < ray->length ) 
    {
        ray->length = t;                            
        
        float4 N1 = normalize(scene_data[idx].vn1);
        float4 N2 = normalize(scene_data[idx].vn2);
        float4 N3 = normalize(scene_data[idx].vn3);
        
        float w = 1 - u - v;        
        hit->hit_point = ray->origin + ray->dir * t;
        hit->normal = normalize(N1*w + N2*u + N3*v);
        
        hit->triangle_ID = idx;
        hit->light_ID = -1;
        return true;
    }     
    return false;
}

bool rayAabbIntersection(Ray* ray, AABB bb)
{
    float t_max = INFINITY, t_min = -INFINITY;
    float3 dir_inv = 1 / ray->dir.xyz;
    
    float3 min_diff = (bb.p_min - ray->origin).xyz * dir_inv;
    float3 max_diff = (bb.p_max - ray->origin).xyz * dir_inv;
    
    if(!isnan(min_diff.x))
    {
        t_min = fmax(min(min_diff.x, max_diff.x), t_min);
        t_max = min(fmax(min_diff.x, max_diff.x), t_max);
    }
    
    if(!isnan(min_diff.y))
    {
        t_min = fmax(min(min_diff.y, max_diff.y), t_min);
        t_max = min(fmax(min_diff.y, max_diff.y), t_max);
    }
    if(t_max < t_min)
        return false;
    
    if(!isnan(min_diff.z))
    {
        t_min = fmax(min(min_diff.z, max_diff.z), t_min);
        t_max = min(fmax(min_diff.z, max_diff.z), t_max);
    }
    
    /*
    t_min = fmax(t_min, min(min(min_diff.x, max_diff.x), t_max));
    t_max = min(t_max, fmax(fmax(min_diff.x, max_diff.x), t_min));

    t_min = fmax(t_min, min(min(min_diff.y, max_diff.y), t_max));
    t_max = min(t_max, fmax(fmax(min_diff.y, max_diff.y), t_min));

    t_min = fmax(t_min, min(min(min_diff.z, max_diff.z), t_max));
    t_max = min(t_max, fmax(fmax(min_diff.z, max_diff.z), t_min));*/
    
    return (t_max > fmax(t_min, 0.0f));
}

float4 shading(int2 pixel, Ray ray, Ray light_ray, int GI_CHECK, uint* seed, int bvh_size, __global BVHNodeGPU* bvh, int scene_size, __global Triangle* scene_data, __global Material* mat_data)
{   
    HitInfo hit_info = {-1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};

    if(!traceRay(&ray, &hit_info, bvh_size, bvh, scene_size, scene_data))
        return BACKGROUND_COLOR;
    
    if(hit_info.light_ID >= 0)
    {
        if(dot(ray.dir, light_sources[hit_info.light_ID].norm) < 0)
            return (float4)(1,1,1,1);
        else
            return (float4)(0.1,.1,.1,1);
    }
    float4 direct_color, indirect_color, throughput;    
    throughput = (float4) (1.f, 1.f, 1.f, 1.f);
    direct_color = (float4) (0.f, 0.f, 0.f, 0.f);   
    indirect_color = direct_color;
    
    // Compute Direct Illumination at the first hitpoint.
    int matID = scene_data[hit_info.triangle_ID].matID;
    if(!mat_data[matID].is_specular)
        direct_color =  evaluateDirectLighting(pixel, -ray.dir, hit_info, seed, bvh_size, bvh, scene_size, scene_data, mat_data);
    
    //Compute Indirect Illumination bouncing rays around.
    if(GI_CHECK)
    { 
        // Bug on AMD platforms. While(true) yields darker images.
        //while(true)
        float r = 0.0f;
        for(int i = 0; i < 100000; i++)        
        {
            HitInfo new_hitinfo = {-1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
            Ray new_ray;
            float pdf = 1, cosine_falloff, brdf_prob = 0.0f, ior_factor = 1.0f;
            bool is_refract = false, is_glossy = false;
            if(mat_data[matID].is_specular)
                sampleFresnelIncidence(&new_ray, hit_info, -ray.dir, &ior_factor, seed, scene_data, mat_data);
            else
            {
                is_glossy = sampleGlossyPdf(hit_info, scene_data, mat_data, seed, &brdf_prob);
            
                if(brdf_prob == 0.0f)
                    break;
                
                if(is_glossy)
                    phongSampleHemisphere(&new_ray, &pdf, -ray.dir, hit_info, seed, scene_data, mat_data);             
                else
                    cosineWeightedHemisphere(&new_ray, &pdf, hit_info, seed);               
            }            
            
            // If GI_ray hits nothing or pdf is zero or if ray hits light source return.
            // Note that in normal path tracing we would have skipped an iteration upon hitting a light source. But in Progressive path tracing
            // we return i.e. we ignore the sample completely since we are taking samples continuously.
            // Also Account for the light source if the object hit is specular.
            if(pdf <= 0.0f || !traceRay(&new_ray, &new_hitinfo, bvh_size, bvh, scene_size, scene_data) || new_hitinfo.light_ID >= 0)
            {
                if(mat_data[matID].is_specular && new_hitinfo.light_ID >= 0)
                {
                    indirect_color += throughput * light_sources[new_hitinfo.light_ID].emissive_col;
                }                
                break;
            }
            
            //If it hits object, accumulate the brdf  and geometry terms.            
            indirect_color += throughput * mat_data[scene_data[new_hitinfo.triangle_ID].matID].emissive;
            
            /* If material is specular or transmissive multiply by IOR factor (which contains non-1 values for refraction only case). The fresenel term gets cancelled out
             * when divided by the same probability of following a reflection or refraction path. The cos(theta) term gets cancelled due to the specular/transmissive bsdf.
             * The delta term gets cancelled by the pdf.
             */
            if(mat_data[matID].is_specular)          
                throughput = throughput * ior_factor;
            else
                throughput = throughput * evaluateBRDF(new_ray.dir, -ray.dir, hit_info, is_glossy, brdf_prob, scene_data, mat_data) * fmax(dot(new_ray.dir, hit_info.normal), 0.0f) / pdf; 
            
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
            
            matID = scene_data[new_hitinfo.triangle_ID].matID;
            if(!mat_data[matID].is_specular)
                indirect_color +=  throughput * evaluateDirectLighting(pixel, -new_ray.dir, new_hitinfo, seed, bvh_size, bvh, scene_size, scene_data, mat_data);
            hit_info = new_hitinfo;
            ray = new_ray;            
        }
    }
    return direct_color + indirect_color;
}

float4 evaluateDirectLighting(int2 pixel , float4 w_o, HitInfo hit, uint* seed, int bvh_size, __global BVHNodeGPU* bvh, int scene_size, __global Triangle* scene_data,  __global Material* mat_data)
{
    float4 emission = mat_data[scene_data[hit.triangle_ID].matID].emissive;
    float4 light_sample = (float4) (0.f, 0.f, 0.f, 0.f);
    float4 w_i;
    float light_pdf, brdf_prob = 0.0f; 
    bool sample_glossy = false;
    
    int j = sampleLights(hit, &light_pdf, &w_i, seed);
	if(j == -1 || light_pdf <= 0.0f)
		return emission;
	
	float len = length(w_i);
    len -= EPSILON*1.5f;
    w_i = normalize(w_i);
    
	Ray shadow_ray = {hit.hit_point + w_i * EPSILON, w_i, INFINITY, true};
	HitInfo shadow_hitinfo = {-1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
    shadow_ray.length = len; 
    
    //Direct Light Sampling     
	//If ray doesn't hit anything (exclude light source j while intersection check). This means light source j visible.
    if(!traceRay(&shadow_ray, &shadow_hitinfo, bvh_size, bvh, scene_size, scene_data))
    {
        sample_glossy = sampleGlossyPdf(hit, scene_data, mat_data, seed, &brdf_prob);
        if(brdf_prob == 0.0f)
            return emission;
		light_sample = evaluateBRDF(w_i, w_o, hit, sample_glossy, brdf_prob, scene_data, mat_data) * light_sources[j].emissive_col * fmax(dot(w_i, hit.normal), 0.0f);
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
        brdf_pdf = calcPhongPDF(w_i, w_o, hit, scene_data, mat_data);
    else
        brdf_pdf = calcCosPDF(w_i, hit.normal);
    
    mis_weight =  light_pdf;     
    powerHeuristic(&mis_weight, light_pdf, brdf_pdf, 2);
    light_sample *= mis_weight;
    
    // BRDF Sampling
    Ray brdf_sample_ray;
    
    if(sample_glossy)
        phongSampleHemisphere(&brdf_sample_ray, &brdf_pdf, w_o, hit, seed, scene_data, mat_data);
    else
        cosineWeightedHemisphere(&brdf_sample_ray, &brdf_pdf, hit, seed);
    
    if(brdf_pdf <= 0.0f)
        return light_sample + emission;                
    
    w_i = brdf_sample_ray.dir;  
    HitInfo new_hitinfo = {-1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};   
    
    //If traceRay doesnt hit anything or if it does not hit the same light source return only light sample.
    if(!traceRay(&brdf_sample_ray, &new_hitinfo, bvh_size, bvh, scene_size, scene_data) || new_hitinfo.light_ID != j)
       return light_sample + emission;
    
    mis_weight = brdf_pdf;
    powerHeuristic(&mis_weight, light_pdf, brdf_pdf, 2);
    brdf_sample =  evaluateBRDF(w_i, w_o, hit, sample_glossy, brdf_prob, scene_data, mat_data) * light_sources[j].emissive_col * fmax(dot(w_i, hit.normal), 0.0f);
    brdf_sample *=  mis_weight / brdf_pdf;
    
    return (light_sample + brdf_sample + emission);
    #endif
}

float4 evaluateBRDF(float4 w_i, float4 w_o, HitInfo hit_info, bool sample_glossy, float rr_prob, __global Triangle* scene_data, __global Material* mat_data)
{
    float4 color, refl_vec;
    float cos_alpha;    
    
    refl_vec = reflect(w_i, hit_info);
    refl_vec = normalize(refl_vec);
    
    int matID = scene_data[hit_info.triangle_ID].matID;
    
    if(!sample_glossy)
        color = mat_data[matID].diff * INV_PI / rr_prob;
    else
    {
        cos_alpha = pow(fmax(dot(w_o, refl_vec), 0.0f), mat_data[matID].alpha_x + mat_data[matID].alpha_y);      
        int phong_exp = mat_data[matID].alpha_x + mat_data[matID].alpha_y;
        color = mat_data[matID].spec * cos_alpha * (phong_exp + 2) * INV_PI * 0.5f / rr_prob;
    }        
    return color;    
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
        
        cosine_falloff = max(dot(temp_wi, hit_info.normal), 0.0f) * max(dot(-temp_wi, light_sources[i].norm), 0.0f);
        
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
            *light_pdf *= distance / fmax(dot(-temp_wi, light_sources[i].norm), 0.0f);
            return i;
        }
        
        weights[i] = length(light_sources[i].emissive_col) * cosine_falloff * area/ distance;
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
            *light_pdf *=  (dot(*w_i, *w_i) / (fmax(dot(-normalize(*w_i), light_sources[i].norm), 0.0f) ));
            return i;
        }
        cumulative_weight += weight;
    }
}

void phongSampleHemisphere (Ray* ray, float* pdf, float4 w_i, HitInfo hit_info, uint* seed, __global Triangle* scene_data, __global Material* mat_data)
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
    int matID = scene_data[hit_info.triangle_ID].matID;
    phong_exponent = mat_data[matID].alpha_x + mat_data[matID].alpha_y;

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

void sampleFresnelIncidence(Ray* ray, HitInfo hit_info, float4 w_i, float* ior_factor, uint* seed, __global Triangle* scene_data, __global Material* mat_data)
{
    int matID = scene_data[hit_info.triangle_ID].matID;
    float r, pdf;
    //If material is refractive as well, calc Fresnel Reflectance.
    if(mat_data[matID].is_transmissive)
    {
        pdf = evalFresnelReflectance(w_i, hit_info, ior_factor, scene_data, mat_data);        
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
                ray->dir = refract(w_i, hit_info, scene_data, mat_data);
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

float4 refract(float4 w_i, HitInfo hit_info, __global Triangle* scene_data, __global Material* mat_data)
{
    //Note w_i here means the incident direction (reversed so it originates from the surface). In traditional pathtracing
    // this is actually the w_o since it goes from the surface towards the camera.
    
    float4 refr_vec;
    float4 normal = hit_info.normal;
    float n1, n2;  // n1 = IOR of incident medium, n2 = other
    int matID = scene_data[hit_info.triangle_ID].matID;
    
    //If theta < 0 this means we are already inside the object. Hence n1 should equal ior of the object rather than air.
    if(dot(w_i, normal) < 0)
    {
        n1 = mat_data[matID].ior;
        n2 = 1;
        normal *= -1.0f;
    }
    else
    {
        n1 = 1;
        n2 = mat_data[matID].ior;
    }
    
    float4 wt_perp = n1/n2 * (dot(w_i, normal) * normal - w_i);
    float4 wt_parallel = sqrt(1 - length(wt_perp) * length(wt_perp)) * -normal;
    refr_vec = normalize(wt_perp + wt_parallel);  
    return refr_vec;
}

float evalFresnelReflectance(float4 w_i, HitInfo hit_info, float* ior_factor, __global Triangle* scene_data, __global Material* mat_data)
{
    //Note w_i here means the incident direction (reversed so it originates from the surface). In traditional pathtracing
    // this is actually the w_o since it goes from the surface towards the camera.
    
    float4 normal = hit_info.normal;
    float n1, n2;  // n1 = IOR of incident medium, n2 = other
    int matID = scene_data[hit_info.triangle_ID].matID;
    
    //If theta < 0 this means we are already inside the object. Hence n1 should equal ior of the object rather than air.    
    if(dot(w_i, normal) < 0)
    {
        n1 = mat_data[matID].ior;
        n2 = 1;
        normal *= -1.0f;
    }
    else
    {
        n1 = 1;
        n2 = mat_data[matID].ior;
    }
    
    float cosThetaI = dot(w_i, normal);
    
    float sinThetaI = sqrt(1 - cosThetaI*cosThetaI);
    float sinThetaT = n1 * sinThetaI / n2;
    float cosThetaT =  sqrt(1 - sinThetaT*sinThetaT);
    
    if(sinThetaT >= 1.0f && n1 > n2)
        return 1.0f;
    
    float4 ks = mat_data[matID].spec;
    float r0 =  ks.x + ks.y + ks.z;
    r0 /= 3.0f;
    *ior_factor = (n2*n2) / (n1*n1);
    if(n1 > n2)
        return (r0 + (1-r0) * (1- pow(cosThetaI,5.0f)));
    else
        return (r0 + (1-r0) * (1- pow(cosThetaT,5.0f)));
}

float calcPhongPDF(float4 w_i, float4 w_o, HitInfo hit_info, __global Triangle* scene_data, __global Material* mat_data)
{
    float4 refl_dir;
    refl_dir = 2*(dot(w_o, hit_info.normal)) * hit_info.normal - w_o;
    refl_dir = normalize(refl_dir);
    
    float costheta = fmax(0.0f, cos(dot(refl_dir, w_i)));
    float phong_exponent;
    int matID = scene_data[hit_info.triangle_ID].matID;
    phong_exponent = mat_data[matID].alpha_x + mat_data[matID].alpha_y;
    
    return (phong_exponent+1) * 0.5 * INV_PI * pow(costheta, phong_exponent);    
}

float calcCosPDF(float4 w_i, float4 normal)
{
    return fmax(dot(w_i, normal), 0.0f) * INV_PI;
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

bool sampleGlossyPdf(HitInfo hit, __global Triangle* scene_data, __global Material* mat_data, uint* seed, float* prob)
{
    *seed = xor_shift(*seed);
    float r = *seed / (float) UINT_MAX;  
    
    float4 spec, diff, sum;
    float pd, ps; 
    
    spec = mat_data[scene_data[hit.triangle_ID].matID].spec;
    diff = mat_data[scene_data[hit.triangle_ID].matID].diff;
    
    if(length(spec.xyz) == 0.0f)
    {
        *prob = 1.0f;
        return false;        
    }
    else if(length(diff.xyz) == 0.0f || diff.x + diff.y + diff.z == 0.0f)
    {
        *prob = 1.0f;
        return true;        
    }
    
    sum = spec + diff;
    float max_val = max(sum.x, max(sum.y, sum.z));
    
    if(max_val == sum.x)
    {
        pd = diff.x;
        ps = spec.x;
    }
    else if(max_val == sum.y)
    {
        pd = diff.y;
        ps = spec.y;
    }
    else
    {
        pd = diff.z;
        ps = spec.z;
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