#define PI              3.14159265359f
#define INV_PI          0.31830988618f
#define EPSILON         0.0001f
#define DOT_THRESHOLD = 0.9995;
#define RR_THRESHOLD    3
#define WALL_SIZE       6
#define SPHERE_SIZE     2
#define LIGHT_SIZE      1

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
	float4 vert_normal;

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

__constant Quad light_sources[2] = { {     (float4)(-1.0f, 2.3f, -1.5f, 1.f), // position
                                                    (float4)(0.f, -1.f, 0.f, 0.f),      //norm
                                                    (float4)(6.f, 6.f, 6.f, 0.f),    //emissive col
                                                    (float4)(0.f, 0.f, 0.f, 0.f),       //diffuse col
                                                    (float4)(0.f, 0.f, 0.f, 0.f),       //specular col
                                                    (float4)(2.f, 0.f, 0.f, 0.f),       //edge_l
                                                    (float4)(0.f, 0.f, 1.0f, 0.f),       //edge_w
                                                    0.f                                 //phong exponent
                                               },

                                              
                                            };

__constant float4 SKY_COLOR =(float4) (0.588f, 0.88f, 1.0f, 1.0f);
__constant float4 BACKGROUND_COLOR =(float4) (0.4f, 0.4f, 0.4f, 1.0f);
__constant float4 PINK = (float4) (0.988f, 0.0588f, 0.7529f, 1.0f);

//Core Functions
void createRay(float pixel_x, float pixel_y, int img_width, int img_height, Ray* eye_ray, constant Camera* main_cam);
bool traceRay(Ray* ray, HitInfo* hit_info, int exclude_lightID, int scene_size, __global Triangle* scene_data);
float4 shading(int2 pixel, Ray ray, Ray light_ray, int GI_CHECK, uint* seed, int scene_size, __global Triangle* scene_data,  __global Material* mat_data);
float4 evaluateDirectLighting(int2 pixel, Ray ray, HitInfo hit_info, uint* seed, int scene_size, __global Triangle* scene_data,  __global Material* mat_data);
float4 evaluateBRDF(float4 w_i, float4 w_o, HitInfo hit_info, __global Triangle* scene_data, __global Material* mat_data );
int sampleLights(HitInfo hit_info, float* light_pdf, float4* w_i, uint* seed);

//Sampling Hemisphere Functions
void phongSampleHemisphere (Ray* ray, float* pdf, float4 w_o, HitInfo hit_info, uint* seed,  __global Triangle* scene_data, __global Material* mat_data);
void uniformSampleHemisphere(Ray* ray, float* pdf, HitInfo hit_info, uint* seed);
void cosineWeightedHemisphere(Ray* ray, float* pdf, HitInfo hit_info, uint* seed);

//PDF functions to return PDF values provided a ray direction
float calcPhongPDF(float4 w_i, float4 w_o, HitInfo hit_info, __global Triangle* scene_data, __global Material* mat_data);
float calcCosPDF(float4 w_i, HitInfo hit_info);

// Helper Functions
float4 inverseGammaCorrect(float4 color);
float getYluminance(float4 color);
uint wang_hash(uint seed);
uint xor_shift(uint seed);
void powerHeuristic(float* weight, float light_pdf, float brdf_pdf, int beta);

__kernel void pathtracer(__write_only image2d_t outputImage, __read_only image2d_t inputImage, __constant Camera* main_cam, 
                         __global Triangle* scene_data, __global Material* mat_data, int GI_CHECK, int reset, uint rand, int scene_size)
{
    int img_width = get_image_width(outputImage);
    int img_height = get_image_height(outputImage);
    	
    int2 pixel = (int2)(get_global_id(0), get_global_id(1));
	
    if (pixel.x >= img_width || pixel.y >= img_height)
        return;
        
    //create a camera ray and light ray
    Ray eye_ray, light_ray;
    float r1, r2;
    uint seed = pixel.y* img_width + pixel.x;
    seed =  rand * seed;
	
	//if(pixel.x == 1)
	//printf("%2.2v4hlf \n", light_sources[0].pos);
	
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
	//createLightRay(pixel,&light_ray,&seed);
    color = shading(pixel, eye_ray, light_ray, GI_CHECK ,&seed, scene_size, scene_data, mat_data);
   
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



bool traceRay(Ray* ray, HitInfo* hit_info, int exclude_lightID, int scene_size, __global Triangle* scene_data)
{
     bool flag = false;

	 for (int i =0 ; i < scene_size; i++)
	 {
		float4 v1v2 = scene_data[i].v1 - scene_data[i].v2; 
		float4 v1v3 = scene_data[i].v1 - scene_data[i].v3;
		
		float4 N =  scene_data[i].vn1;
		
		float Area = length(N)/2 ;
		
		float NdotD = dot(N,ray->dir);

		if ( fabs(NdotD) > EPSILON )
		{ 
			//float dist = dot( N, scene_data[i].v1);
			float t = ( dot(N, scene_data[i].v1 - ray->origin) ) / dot(N,ray->dir);			
			if ( t > 0 && t < ray->length  ) 
			{
				
				float4 P = ray->origin + t * ray->dir;
				
				float4 Edge1 = scene_data[i].v2 - scene_data[i].v1;
				float4 Vp1 = P - scene_data[i].v1;
				float4 PerpToTri = cross(Edge1,Vp1);
				
				if ( dot(N,PerpToTri) > 0 )
				{
					float4 Edge2 = scene_data[i].v3 - scene_data[i].v2;
					float4 Vp2 = P - scene_data[i].v2;
					float4 PerpToTri = cross(Edge2,Vp2);
					
                    if( dot(N,PerpToTri) > 0 )
                    {
                        float4 Edge3 = scene_data[i].v1 - scene_data[i].v3;
                        float4 Vp3= P - scene_data[i].v3;
                        float4 PerpToTri = cross(Edge3,Vp3);
                        
                        if( dot(N,PerpToTri) > 0 )
                        {
                            //float a = 0.001;
                            
                            float4 N1 = normalize(scene_data[i].vn1);
                            float4 N2 = normalize(scene_data[i].vn2);
                            float4 N3 = normalize(scene_data[i].vn3);
                            
                            float4 N1N2_lerp = N1*t + N2*(t-1);
                            float4 N1N3_lerp = N1*t + N3*(t-1);
                            
                            hit_info->vert_normal = N;
                            hit_info->hit_point = P;
                            hit_info->normal = normalize(N1N2_lerp*t + N1N3_lerp*(t-1));
                            hit_info->triangle_ID = i;
                            ray->length  = t;
                            
                            flag = true;
                            
                        }							
                    }
				}				
			}	
		}
	 }
    
	
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
                    hit_info->hit_point = ray->origin + (ray->dir * t);
					hit_info->light_ID = i;
                    hit_info->triangle_ID = -1;
					if(i != exclude_lightID)
						flag = true;
					else
						flag = false;
				}        
                
            }
        }       
    }

    return flag;
	
}


float4 shading(int2 pixel, Ray ray, Ray light_ray, int GI_CHECK, uint* seed, int scene_size, __global Triangle* scene_data, __global Material* mat_data)
{   
    HitInfo hit_info = {-1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
	HitInfo hit_light = {-1, 0, (float4)(0,0,0,1), light_sources[0].norm};

    if(!traceRay(&ray, &hit_info, -1, scene_size, scene_data))
        return BACKGROUND_COLOR;
    
    if(hit_info.light_ID >= 0)
    {
        if(dot(ray.dir, light_sources[hit_info.light_ID].norm) < 0)
            return (float4)(1,1,1,1);
        else
            return (float4)(0.5,.5,.5,1);
    }
                    
    //if(hit_info.quad_ID == -1 && hit_info.sphere_ID == -1)
		//return (float4) (1.0,0.0,.0,1.0);
    float4 direct_color, indirect_color, throughput;    
    throughput = (float4) (1.f, 1.f, 1.f, 1.f);
    direct_color = (float4) (0.f, 0.f, 0.f, 0.f);   
    indirect_color = direct_color;
    
    // Compute Direct Illumination at the first hitpoint.
    direct_color =  evaluateDirectLighting(pixel, ray, hit_info, seed, scene_size, scene_data, mat_data);
    
    //Compute Indirect Illumination by storing all the seondary rays first.
    if(GI_CHECK)
    {   
        int x = 0;
        // Bug on AMD platforms. While(true) yields darker images.
        for(int i = 0; i < 100000; i++)
        //while(true)
        {
            HitInfo new_hitinfo = {-1, -1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
            Ray new_ray;
            float pdf = 1, cosine_falloff, spec_prob = 0.0f, r;
            
            *seed = xor_shift(*seed);
            r = *seed / (float) UINT_MAX;   
            
            
			float4 spec_color = mat_data[scene_data[hit_info.triangle_ID].matID].spec;            
			spec_prob =  fmax(spec_color.x, fmax(spec_color.y, spec_color.z));
            
            if(r <= spec_prob)
               phongSampleHemisphere(&new_ray, &pdf, -ray.dir, hit_info, seed, scene_data, mat_data);             
            else
                cosineWeightedHemisphere(&new_ray, &pdf, hit_info, seed);            
            
            // If GI_ray hits nothing or pdf is zero or if ray hits light source return.
            // Note that in normal path tracing we would have skipped an iteration upon hitting a light source. But in Progressive path tracing
            // we return i.e. we ignore the sample completely since we are taking samples continuously.
            if(!traceRay(&new_ray, &new_hitinfo, -1, scene_size, scene_data) || pdf == 0.0f || new_hitinfo.light_ID >= 0)
                break;
                
            //If it hits object, accumulate the brdf  and geometry terms.
            throughput = throughput * evaluateBRDF(new_ray.dir, -ray.dir, hit_info, scene_data, mat_data) * max(dot(new_ray.dir, hit_info.normal), 0.0f) / pdf; 
        
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
            
            indirect_color +=  throughput * evaluateDirectLighting(pixel, new_ray, new_hitinfo, seed, scene_size, scene_data, mat_data);
            hit_info = new_hitinfo;
            ray = new_ray;
            x++;
        }
    }
    return direct_color + indirect_color;
}

float4 evaluateDirectLighting(int2 pixel , Ray ray, HitInfo hit_info, uint* seed, int scene_size, __global Triangle* scene_data,  __global Material* mat_data)
{
	float4 direct_color = (float4) (0.f, 0.f, 0.f, 0.f);
	float4 w_i;
	float light_pdf;
	//Direct Light Sampling
	int j = sampleLights(hit_info, &light_pdf, &w_i, seed);
	if(j == -1 || light_pdf == 0)
		return direct_color;
	
	Ray shadow_ray = {hit_info.hit_point + w_i * EPSILON, w_i, INFINITY, true};
	HitInfo shadow_hitinfo = {-1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
	
	//If ray doesn't hit anything (exclude light source j while intersection check). This means light source j visible.
    if(!traceRay(&shadow_ray, &shadow_hitinfo, 0, scene_size, scene_data) )
    {
		direct_color = evaluateBRDF(w_i, -ray.dir, hit_info, scene_data, mat_data) * light_sources[j].emissive_col * max(dot(w_i, hit_info.normal), 0.0f);
		direct_color *= 1/light_pdf;
	}
	
	return direct_color;
	/*
    float4 brdf_sample = (float4) (0.f, 0.f, 0.f, 0.f);
    float4 light_sample = (float4) (0.f, 0.f, 0.f, 0.f);
    float4 emission;
    float4 w_i;
    float light_pdf, brdf_pdf, mis_weight, spec_prob, r;
    
    //We use MIS with Power Heuristic. The exponent can be set to 1 for balance heuristic.
    //Emission 
    if(hit_info.triangle_ID >= 0)
		emission = mat_data[scene_data[hit_info.triangle_ID].matID].emissive;
    
    //Direct Light Sampling
    int j = sampleLights(hit_info, &light_pdf, &w_i, seed);
    if(j == -1 || light_pdf == 0.0f)
        return light_sample + emission;
    
    Ray shadow_ray = {hit_info.hit_point + w_i * EPSILON, w_i, INFINITY, true};
    HitInfo shadow_hitinfo = {-1, -1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
    
    //If ray doesn't hit anything (exclude light source j while intersection check). This means light source j visible.
    if(!traceRay(&shadow_ray, &shadow_hitinfo, j, scene_size, scene_data) )
    {
        *seed = xor_shift(*seed);
        r = *seed / (float) UINT_MAX;   
		
        //Use probability to pick either diffuse or glossy pdf        
		if (hit_info.triangle_ID >= 0)
			spec_prob = length(mat_data[scene_data[hit_info.triangle_ID].matID].spec);
        
        if(r <= spec_prob)
            brdf_pdf = calcPhongPDF(w_i, -ray.dir, hit_info, scene_data, mat_data);
        else
            brdf_pdf = calcCosPDF(w_i, hit_info);

        mis_weight =  light_pdf;        
        powerHeuristic(&mis_weight, light_pdf, brdf_pdf, 1);
        
        light_sample =  evaluateBRDF(w_i, -ray.dir, hit_info, scene_data, mat_data) * light_sources[j].emissive_col * max(dot(w_i, hit_info.normal), 0.0f);
        light_sample *=  mis_weight/light_pdf;
        
        //Commented lines for comparing with/without MIS
        //light_sample *=  1/light_pdf;
    }
    //return light_sample + emission;   
    
    light_sample = light_sample + emission;
    
    // BRDF Sampling
    Ray brdf_sample_ray;
    
    if(r <= spec_prob)
        phongSampleHemisphere(&brdf_sample_ray, &brdf_pdf, -ray.dir, hit_info, seed, scene_data, mat_data);
    else
        cosineWeightedHemisphere(&brdf_sample_ray, &brdf_pdf, hit_info, seed);
    
    if(brdf_pdf == 0.0f)
        return light_sample;                
    
    w_i = brdf_sample_ray.dir;  
    HitInfo new_hitinfo = {-1, -1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};   
    
    //If traceRay doesnt hit anything or if it does not hit the same light source return only light sample.
    if(!traceRay(&brdf_sample_ray, &new_hitinfo, -1, scene_size, scene_data))
       return light_sample;
    else if(new_hitinfo.light_ID != j)
        return light_sample;

    mis_weight = brdf_pdf;
    powerHeuristic(&mis_weight, light_pdf, brdf_pdf, 1);
    brdf_sample =  evaluateBRDF(w_i, -ray.dir, hit_info, scene_data, mat_data) * light_sources[j].emissive_col * max(dot(w_i, hit_info.normal), 0.0f);
    brdf_sample *=  mis_weight / brdf_pdf;
    
    return (light_sample + brdf_sample);*/
}

float4 evaluateBRDF(float4 w_i, float4 w_o, HitInfo hit_info, __global Triangle* scene_data, __global Material* mat_data)
{
    float4 emissive, diffuse, specular, refl_vec;
    float mirror_config;
    
    refl_vec = (2*dot(w_i, hit_info.normal)) *hit_info.normal - w_i;
    refl_vec = normalize(refl_vec);
	
	if(hit_info.triangle_ID >= 0)
	{
		mirror_config = pow(max(dot(w_o, refl_vec), 0.0f), mat_data[scene_data[hit_info.triangle_ID].matID].alpha_x + mat_data[scene_data[hit_info.triangle_ID].matID].alpha_y);       
        emissive = mat_data[scene_data[hit_info.triangle_ID].matID].emissive;
        diffuse = mat_data[scene_data[hit_info.triangle_ID].matID].diff * INV_PI;
        specular = mat_data[scene_data[hit_info.triangle_ID].matID].spec * mirror_config * ((mat_data[scene_data[hit_info.triangle_ID].matID].alpha_x+mat_data[scene_data[hit_info.triangle_ID].matID].alpha_y)+2) * INV_PI * 0.5f;
	}
	
    return (emissive + diffuse + specular);
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
            *w_i = temp_wi;
            *light_pdf = 1/area;
            *light_pdf *= distance / max(dot(-temp_wi, light_sources[i].norm), 0.0f);
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
            *light_pdf *=  (dot(*w_i, *w_i) / (max(dot(-normalize(*w_i), light_sources[i].norm), 0.0f) ));
            *w_i = normalize(*w_i);
            return i;
        }
        cumulative_weight += weight;
    }
}

void phongSampleHemisphere (Ray* ray, float* pdf, float4 w_o, HitInfo hit_info, uint* seed, __global Triangle* scene_data, __global Material* mat_data)
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
	
	if (hit_info.triangle_ID > 0)
		phong_exponent = mat_data[scene_data[hit_info.triangle_ID].matID].alpha_x + mat_data[scene_data[hit_info.triangle_ID].matID].alpha_y;

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

float calcPhongPDF(float4 w_i, float4 w_o, HitInfo hit_info, __global Triangle* scene_data, __global Material* mat_data)
{
    float4 refl_dir;
    refl_dir = 2*(dot(w_o, hit_info.normal)) * hit_info.normal - w_o;
    refl_dir = normalize(refl_dir);
    
    float costheta = cos(dot(refl_dir, w_i));
    float phong_exponent;
    
    if(hit_info.triangle_ID > 0)
		phong_exponent = mat_data[scene_data[hit_info.triangle_ID].matID].alpha_x + mat_data[scene_data[hit_info.triangle_ID].matID].alpha_y;
    
}

float calcCosPDF(float4 w_i, HitInfo hit_info)
{
    return max(dot(w_i, hit_info.normal), 0.0f) * INV_PI;
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

void powerHeuristic(float* weight, float light_pdf, float brdf_pdf, int beta)
{
    *weight = (pown(*weight, beta)) / (pown(light_pdf, beta) + pown(brdf_pdf, beta) );  
}