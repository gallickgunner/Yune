__kernel void pathtracer(__write_only image2d_t outputImage, __read_only image2d_t inputImage,
                         __constant Camera* main_cam, int GI_CHECK, int reset, uint rand,
                         __global Triangle* scene_data, __constant Material* mat_data)
{
}