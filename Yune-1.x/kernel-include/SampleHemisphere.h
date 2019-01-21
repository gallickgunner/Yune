void phongsampleHemisphere (Ray* ray, float* pdf, float4 w_i, HitInfo hit_info, uint* seed);
void uniformSampleHemisphere(Ray* ray, float* pdf, HitInfo hit_info, uint* seed);
void cosineWeightedHemisphere(Ray* ray, float* pdf, HitInfo hit_info, uint* seed);

void phongsampleHemisphere (Ray* ray, float* pdf, float4 w_i, HitInfo hit_info, uint* seed)
{
    /* Create a new coordinate system for Normal Space where Z aligns with Reflection Direction. */
    Mat4x4 normal_to_world;
    float4 Ny, Nx, Nz;
    
    //Note that w_i points towards the surface.
    Nz = w_i - 2*(dot(w_i, hit_info.normal)) * hit_info.normal;
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
    if(hit_info.obj_ID > 0)
        phong_exponent = spheres[hit_info.obj_ID].phong_exponent;
    else
        phong_exponent = planes[hit_info.plane_ID].phong_exponent;
    
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
    ray->origin = hit_info.hit_point + ray->dir * RAYBUMP_EPSILON;
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
    ray->origin = hit_info.hit_point + ray->dir * RAYBUMP_EPSILON;
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
    ray->origin = hit_info.hit_point + ray->dir * RAYBUMP_EPSILON;
    ray->is_shadow_ray = false;
    ray->length = INFINITY;
    
    *pdf = z * INV_PI;
}