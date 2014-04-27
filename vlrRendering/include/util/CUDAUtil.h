#ifndef VLR_RENDERING_CUDAUTIL
#define VLR_RENDERING_CUDAUTIL

#include <stdio.h>
#include <cuda.h>
#include <cuda_runtime_api.h>

#ifdef __CUDACC__
	#define HOST_DEVICE_FUNC __host__ __device__
#else
	#define HOST_DEVICE_FUNC
#endif

#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }
void gpuAssert(cudaError_t code, char *file, int line, bool abort=true);

namespace vlr
{
	namespace rendering
	{
		namespace test
		{
			template <typename T>
			HOST_DEVICE_FUNC inline void swap(T& a, T& b)
			{
				T temp = a;
				a = b;
				b = temp;
			}
		}
		
		__device__ int get_child_index(unsigned int mask);
	}
}

#endif /* VLR_RENDERING_CUDAUTIL */