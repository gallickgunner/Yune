#define PI 				3.14159265359f
#define INV_PI 			0.31830988618f
#define RAYBUMP_EPSILON 0.01f
#define PLANE_SIZE 		5
#define SPHERE_SIZE 	2
#define LIGHT_SIZE 		2

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

	int obj_ID, plane_ID, light_ID;
	float4 hit_point;  // point of intersection.
	float4 normal;

} HitInfo;

typedef struct AreaLight{

	float4 pos;
	float4 norm;
	float4 emissive_col;
	float4 diffuse_col;
	float4 specular_col;	
	float4 edge_l;
	float4 edge_w;
	float power;
	float area;

} AreaLight;

typedef struct Sphere{

    float4 pos;
    float4 emissive_col;
    float4 diffuse_col;
	float4 specular_col;
	float radius;
	float phong_exponent;

} Sphere;

typedef struct Plane{

    float4 point;
	float4 norm;
    float4 emissive_col;
    float4 diffuse_col;
	float4 specular_col;
	float phong_exponent;
	
} Plane;

typedef struct Camera{
	Mat4x4 view_mat;	
	float view_plane_dist;
} Camera;

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                           CLK_ADDRESS_CLAMP_TO_EDGE   |
                           CLK_FILTER_NEAREST;

__constant Sphere spheres[SPHERE_SIZE] = { { (float4)(2.0f, -2.0f, -6.0f, 1.0f),								
								   (float4)(0, 0, 0, 0),
								   (float4)(0.488f, 0.488f, 0.488f, 0.f),
								   (float4)(0.274f, 0.274f, 0.274f, 0.f),								   
									1.5f, 10000.0f
								 },
							  
								 { (float4)(-4.0f, -2.0f, -6.0f, 1.0f),
								   (float4)(0, 0, 0, 0),
								   (float4)(0.6f, 0.6f, 0.6f, 0),
								   (float4)(0.3f, 0.3f, 0.3f, 0),								   
								   1.0f, 160.0f
								 }
							   };
__constant AreaLight light_sources[LIGHT_SIZE] = { { 	(float4)(-2.0f, 5.f, -6.5f, 1.f),
														(float4)(0.f, -1.f, 0.0f, 0),
														(float4)(0.78f, 0.78f, 0.78f, 0),
														(float4)(0, 0, 0, 0),
														(float4)(0.3f, 0.3f, 0.3f, 0),
														(float4)(4.f, 0, 0, 0),
														(float4)(0, 0, 1.0f, 0),
														120.0f, 4.0f
												   },
													  
												  { 	(float4)(-6.0f, -1.f, -2.f, 1.f),
														(float4)(1.0f, 0.f, 0.0f, 0),
														(float4)(0.78f, 0.78f, 0.78f, 0),
														(float4)(0, 0, 0, 0),
														(float4)(0.3f, 0.3f, 0.3f, 0),
														(float4)(0.f, 2.0f, 0, 0),
														(float4)(0, 0, -4.0f, 0),
														120.0f, 8.0f
												  }
										};
										
__constant Plane planes[PLANE_SIZE] = { { 	(float4)(0.f, -4.f, 0.f, 1.f),
											 (float4)(0.f, 1.f, 0.f, 0),
											 (float4)(0.f, 0.f, 0.f, 0),
											 (float4)(0.75f, 0.75f, 0.75f, 0),
											 (float4)(0, 0, 0, 0), 60.f
										   },							  
							   
										   { (float4)(0.f, 5.f, 0.f, 1.f),
											 (float4)(0.f, -1.f, 0.f, 0.f),
											 (float4)(0.f, 0.f, 0.f, 0),
											 (float4)(0.75f, 0.75f, 0.75f, 0),
											 (float4)(0, 0, 0, 0), 60.f
										   },   
										   
										   { (float4)(6.f, 0.f, 0.f, 0.f),
											 (float4)(-1.f, 0.f, 0.f, 0),
											 (float4)(0.f, 0.f, 0.f, 0),
											 (float4)(0.630,0.058,0.062, 0),
											 (float4)(0, 0, 0, 0), 60.f
										   },							   									   
										   
										   { (float4)(-6.f, 0.f, 0.f, 1.f),
											 (float4)(1.f, 0.f, 0.f, 0),
											 (float4)(0.f, 0.f, 0.f, 0),
											 (float4)(0.132,0.406,0.061, 0),
											 (float4)(0, 0, 0, 0), 60.f
										   },
										   
										   { (float4)(0.f, 0.f, -9.f, 0.f),
											 (float4)(0.f, 0.f, 1.f, 0),
											 (float4)(0.f, 0.f, 0.f, 0),
											 (float4)(0.75f, 0.75f, 0.75f, 0),
											 (float4)(0, 0, 0, 0), 60.f
										   }
										};
										
										
/*__constant Camera main_cam = 	{	{ (float4) (1.f, 0, 0, 0),
									  (float4) (0, 1.f, 0, 0),
									  (float4) (0, 0, 1.f, 0),
									  (float4) (0.0f, 0.0f, 0.0f, 1.0f),
									}, 1.0f			
								};*/
__constant float4 SKY_COLOR =(float4) (0.588f, 0.88f, 1.0f, 1.0f);


void createRay(float pixel_x, float pixel_y, int img_width, int img_height, Ray* eye_ray, constant Camera* main_cam);
bool traceRay(Ray* ray, HitInfo* hit_info, int exclude_lightID);
float4 shading(Ray ray, int GI_BOUNCES, int GI_CHECK, uint* seed);
float4 evaluateDirectLighting(Ray ray, HitInfo hit_info, uint* seed);
float4 evaluateBRDF(float4 w_i, float4 w_o, HitInfo hit_info);
int sampleLights(HitInfo hit_info, float* geometry_term, float* light_pdf, float4* w_i, uint* seed);
void sampleHemisphere(Ray* ray, HitInfo hit_info, uint* seed);
void uniformSampleHemisphere(uint* seed, float* x, float* y, float* z);
void cosineWeightedHemisphere(uint* seed, float* x, float* y, float* z);
uint wang_hash(uint seed);
uint xor_shift(uint seed);
void powerHeuristic(float* weight, float light_pdf, float brdf_pdf, int beta);

__kernel void evaluatePixelRadiance(__write_only image2d_t outputImage, __read_only image2d_t inputImage,
									__constant Camera* main_cam, int RR_THRESHOLD, int GI_CHECK, int reset, uint rand)
{
	int img_width = get_image_width(outputImage);
	int img_height = get_image_height(outputImage);
	
	int2 pixel = (int2)(get_global_id(0), get_global_id(1));
	
	if (pixel.x >= img_width || pixel.y >= img_height)
        return;
	
	//float4 temp = read_imagef(inputImage, sampler, pixel);
	
	
	//create a camera ray 
	Ray eye_ray;
	float r1, r2;
	uint seed = pixel.y* img_width + pixel.x;
	seed =  rand * seed;
	seed =  wang_hash(seed);
	
	float4 color = (float4) (0.f, 0.f, 0.f, 1.f);
	
	seed = xor_shift(seed);
	r1 = seed / (float) UINT_MAX;
	seed =  xor_shift(seed);
	r2 =  seed / (float) UINT_MAX;
	
	createRay(pixel.x + r1, pixel.y + r2, img_width, img_height, &eye_ray, main_cam);			
	color = shading(eye_ray, RR_THRESHOLD, GI_CHECK, &seed);
	
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

	float x, y, z, aspect_ratio;
	aspect_ratio = (img_width*1.0)/img_height;

	x = aspect_ratio *((2.0 * pixel_x/img_width) - 1);
	y = (2.0 * pixel_y/img_height) -1 ;
	z = -main_cam->view_plane_dist;
	
	eye_ray->dir = normalize( (float4) (x,y,z,0));
	
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
					
	
	float4 direct_color, indirect_color, total_color, throughput;	
	throughput = (float4) (1.f, 1.f, 1.f, 1.f);
	direct_color = (float4) (0.f, 0.f, 0.f, 0.f);	
	indirect_color = direct_color;
	total_color = direct_color;
	
	// Compute Direct Illumination at the first hitpoint.
	direct_color =  evaluateDirectLighting(ray, hit_info, seed);
	
	//Compute Indirect Illumination by storing all the seondary rays first.
	if(GI_CHECK)
	{	
		int x = 0;
		while(true)
		{
			HitInfo new_hitinfo = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
			Ray new_ray;			
			
			sampleHemisphere(&new_ray, hit_info, seed);
			new_ray.dir = normalize(new_ray.dir);
			
			// If GI_ray hits nothing, store sky color and break out of bounce loop.
			if(!traceRay(&new_ray, &new_hitinfo, -1))
			{
				break;
			}
			// Skip this iteration as ray hits light source.
			else if(new_hitinfo.light_ID >= 0)
			{	
				continue;
			}		
			
			/*
			 * If it hits object, store explicit direct illumination at that point. We dont multiply dot() since PDF for cos weighted
			 * is cos(theta)/pi thus the cos terms get cancelled.
			 */
				
			throughput = throughput * evaluateBRDF(new_ray.dir, -ray.dir, hit_info) * PI; 
			
			/* Russian Roulette: Use RR after few bounces. Terminate paths with low enough value of throughput. Throughput is basically the value
			 * that the path survives. The termination probability is (1 - throughput). If the path survives boost the energy by
			 * (1/p)
			 */
			
			if(x > RR_THRESHOLD)
			{			
				float p = min(max(throughput.x, max(throughput.y, throughput.z)), 0.85f);
				*seed = xor_shift(*seed);
				float r = *seed / (float) UINT_MAX;	
				
				if(r >= p)
				{
					break;				
				}
				throughput *= 1/p;
			}
			
			indirect_color +=  throughput * evaluateDirectLighting(new_ray, new_hitinfo, seed);
			hit_info = new_hitinfo;
			ray = new_ray;
			x++;
			
		}
	}
	total_color = direct_color + indirect_color;
	return total_color;
}

float4 evaluateDirectLighting(Ray ray, HitInfo hit_info, uint* seed)
{	

	//Note that we for calculating light source contribution we use light_color * light_power/area to get the total intensity.
	float4 brdf_sample = (float4) (0.f, 0.f, 0.f, 0.f);
	float4 light_sample = (float4) (0.f, 0.f, 0.f, 0.f);
	float4 w_i;
	float light_pdf, brdf_pdf, geometry_term, mis_weight, cosine_falloff, distance;
	
	//We use MIS with Power Heuristic.

	//Direct Light Sampling
	int j = sampleLights(hit_info, &geometry_term, &light_pdf, &w_i, seed);
	if(j == -1)
		return light_sample;
	
	Ray shadow_ray = {hit_info.hit_point + w_i * RAYBUMP_EPSILON, w_i, INFINITY, true};
	HitInfo shadow_hitinfo = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
	
	if(!traceRay(&shadow_ray, &shadow_hitinfo, j) )
	{
		mis_weight =  light_pdf;
		brdf_pdf = max(dot(w_i, hit_info.normal), 0.0f) * INV_PI;
		powerHeuristic(&mis_weight, light_pdf, brdf_pdf, 2);		
		light_sample =  evaluateBRDF(w_i, -ray.dir, hit_info) * light_sources[j].emissive_col * geometry_term;	
		light_sample *=  mis_weight/light_pdf;
	}
	//return light_sample;
	
	// BRDF Sampling
	Ray brdf_sample_ray;
	sampleHemisphere(&brdf_sample_ray, hit_info, seed);
	w_i = brdf_sample_ray.dir;
	
	brdf_sample_ray.dir = normalize(brdf_sample_ray.dir);
	HitInfo new_hitinfo = {-1, -1, -1, (float4)(0,0,0,1), (float4)(0,0,0,0)};
	
	//If traceRay doesnt hit anything or if it does not hit the same light source return only light sample.
	if(!traceRay(&brdf_sample_ray, &new_hitinfo, -1))
	   return light_sample;
    else if(new_hitinfo.light_ID != j)
		return light_sample;
	
	distance = dot(w_i, w_i);
	w_i = normalize(w_i);  
	
	cosine_falloff = max(dot(w_i, hit_info.normal), 0.0f) * max(dot(-w_i, light_sources[j].norm), 0.0f);     
	brdf_pdf = max(dot(w_i, hit_info.normal), 0.0f) * INV_PI;	
    mis_weight = brdf_pdf;
	powerHeuristic(&mis_weight, light_pdf, brdf_pdf, 2);
	
	brdf_sample =  evaluateBRDF(w_i, -ray.dir, hit_info) * light_sources[j].emissive_col * light_sources[j].power * cosine_falloff / (light_sources[j].area *distance);
	brdf_sample *=  mis_weight / brdf_pdf;
	
	return (light_sample + brdf_sample);
}

float4 evaluateBRDF(float4 w_i, float4 w_o, HitInfo hit_info)
{
	float4 diffuse, specular, halfway_vec;
	float mirror_config;
	
	halfway_vec = w_o + w_i;
	halfway_vec = normalize(halfway_vec);
	
	if(hit_info.plane_ID >= 0)
	{
		mirror_config = pow(max(dot(hit_info.normal, halfway_vec), 0.0f), planes[hit_info.plane_ID].phong_exponent);		
		diffuse = planes[hit_info.plane_ID].diffuse_col * INV_PI;
		specular = planes[hit_info.plane_ID].specular_col * mirror_config * (planes[hit_info.plane_ID].phong_exponent+8) * INV_PI/ 8;
	}
	else
	{		
		mirror_config = pow(max(dot(hit_info.normal, halfway_vec), 0.0f), spheres[hit_info.obj_ID].phong_exponent);		
		diffuse = spheres[hit_info.obj_ID].diffuse_col * INV_PI;
		specular = spheres[hit_info.obj_ID].specular_col * mirror_config * (spheres[hit_info.obj_ID].phong_exponent+8) * INV_PI/ 8;
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

void sampleHemisphere (Ray* ray, HitInfo hit_info, uint* seed)
{
	/* Create a new coordinate system for Normal Space */
	Mat4x4 normal_to_world;
	float4 Ny, Nx, Nz;
	Ny = hit_info.normal;
	
	if ( fabs(Ny.y) > fabs(Ny.x) )
		Nz = (float4) (0.f, -Ny.z, Ny.y, 0.f);
	else
		Nz = (float4) (Ny.z, 0, -Ny.x, 0.f);
	
	Nz = normalize(Nz);
	Nx = normalize(cross(Ny, Nz));
	
	normal_to_world.r1 = (float4) (Nx.x, Ny.x, Nz.x, hit_info.hit_point.x);
	normal_to_world.r2 = (float4) (Nx.y, Ny.y, Nz.y, hit_info.hit_point.y);
	normal_to_world.r3 = (float4) (Nx.z, Ny.z, Nz.z, hit_info.hit_point.z);
	normal_to_world.r4 = (float4) (Nx.w, Ny.w, Nz.w, hit_info.hit_point.w);
	
	float x, y, z;
	//uniformSampleHemisphere(seed, &x, &y, &z);
	cosineWeightedHemisphere(seed, &x, &y, &z);
	
	float4 ray_dir = (float4) (x, y, z, 0);
	ray->dir.x = dot(normal_to_world.r1, ray_dir);
	ray->dir.y = dot(normal_to_world.r2, ray_dir);
	ray->dir.z = dot(normal_to_world.r3, ray_dir);
	ray->dir.w = 0;
	
	ray->origin = hit_info.hit_point + ray->dir * RAYBUMP_EPSILON;
	ray->is_shadow_ray = false;
	ray->length = INFINITY;
		
}

void uniformSampleHemisphere(uint* seed, float* x, float* y, float* z)
{
	/* Sample hemisphere according to area.  */
	*seed = xor_shift(*seed);
	float r1 = *seed / (float) UINT_MAX;
	*seed = xor_shift(*seed);
	float r2 = *seed / (float) UINT_MAX;
	
	/*  
	theta = inclination (from Y), phi = azimuth. Need theta in [0, pi/2] and phi in [0, 2pi]
	=> Y = r cos(theta)
	=> X = r sin(theta) cos(phi)
	=> Z = r sin(theta) sin(phi)
	
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
    float sinTheta = sqrt(1 - r1*r1); 	// uniform
	*y = r1;             // r * cos(theta)  r = 1
	*x = sinTheta * cos(phi);  // r * sin(theta) cos(phi)
    *z = sinTheta * sin(phi);  // r * sin(theta) sin(phi)  
}

void cosineWeightedHemisphere(uint* seed, float* x, float* y, float* z)
{
	*seed = xor_shift(*seed);
	float r1 = *seed / (float) UINT_MAX;
	*seed = xor_shift(*seed);
	float r2 = *seed / (float) UINT_MAX;
	
	/*  
	theta = inclination (from Y), phi = azimuth. Need theta in [0, pi/2] and phi in [0, 2pi]
	=> Y = r cos(theta)
	=> X = r sin(theta) cos(phi)
	=> Z = r sin(theta) sin(phi)
	
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
	*y = sqrt(1-r1);             // r * cos(theta),  r = 1	
	*x = sinTheta * cos(phi);  // r * sin(theta) cos(phi)
    *z = sinTheta * sin(phi);  // r * sin(theta) sin(phi)
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
	*weight = pown(*weight, beta) / ( pown(light_pdf, beta) + pown(brdf_pdf, beta) );	
}