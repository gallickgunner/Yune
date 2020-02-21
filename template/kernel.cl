__kernel void mykernelfunc(__write_only image2d_t outputImage, __read_only image2d_t inputImage, __constant Camera* main_cam, 
                           __global Triangle* scene_data, __global Material* mat_data, int GI_CHECK, int reset, uint rand)
{
    // Code
}