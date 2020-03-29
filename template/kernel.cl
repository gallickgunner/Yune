#yune-preproc kernel-name pathtracer

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
} AABB;

typedef struct BVHNodeGPU{
    AABB aabb;          //32 
    int vert_list[10]; //40
    int child_idx;      //4
    int vert_len;       //4 - total 80
} BVHNodeGPU;

typedef struct Mat4x4{
    float4 r1;
    float4 r2;
    float4 r3;
    float4 r4;
} Mat4x4;

typedef struct Camera{
    Mat4x4 view_mat;
    float view_plane_dist;  // total 68 bytes
    float pad[3];           // 12 bytes padding to reach 80 (next multiple of 16)
} Camera;

__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE |
                               CLK_ADDRESS_CLAMP_TO_EDGE   |
                               CLK_FILTER_NEAREST;

//The Camera parameter provided to the kernel follows the struct definition defined above. It contains a view to world matrix. And a view plane distance
//parameter which is the length of the view plane from the camera. The camera is initially looking in the -Z direction and origin at (0,0,0)

//The rand parameter contains a 32 bit random number on each iteration. In order to use different seeds for every pixel, you can use the wang hash algorithm
// to generate uncorrelated seed values from pixel IDs. You can check the given implementation for details on how to produce random numbers.

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
    
    //Start your code from here. It's not necessary to use Triangles/BVH. If you don't read any triangles, these buffers will be null
}
