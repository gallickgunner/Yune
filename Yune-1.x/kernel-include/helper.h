float getYluminance(float4 color);
uint wang_hash(uint seed);
uint xor_shift(uint seed);
void powerHeuristic(float* weight, float light_pdf, float brdf_pdf, int beta);


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
