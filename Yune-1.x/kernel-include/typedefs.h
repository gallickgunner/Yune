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
								   (float4)(0.5f, 0.5f, 0.5f, 0.f),
								   (float4)(0.35f, 0.35f, 0.35f, 0.f),								   
									1.5f, 100.0f
								 },
							  
								 { (float4)(-4.0f, -2.0f, -6.0f, 1.0f),
								   (float4)(0, 0, 0, 0),
								   (float4)(0.6f, 0.6f, 0.6f, 0),
								   (float4)(0.2f, 0.2f, 0.2f, 0),								   
								   1.0f, 10.0f
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
													  
												  { 	(float4)(-6.0f, 0.5f, -4.0f, 1.f),
														(float4)(1.0f, 0.f, 0.0f, 0),
														(float4)(0.78f, 0.78f, 0.78f, 0),
														(float4)(0, 0, 0, 0),
														(float4)(0.3f, 0.3f, 0.3f, 0),
														(float4)(0.f, 2.0f, 0, 0),
														(float4)(0, 0, -2.0f, 0),
														120.0f, 4.0f
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
											 (float4)(0.f, 0.f, 0.f, 0), 260.f
										   }
										};
										
__constant float4 SKY_COLOR =(float4) (0.588f, 0.88f, 1.0f, 1.0f);
__constant float4 PINK = (float4) (0.988f, 0.0588f, 0.7529f, 1.0f);
