/*
Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#include <assert.h>
#include <stdio.h>
#include <algorithm>
#include <stdlib.h>
#include <iostream>

#include "hip/hip_runtime.h"

#include <rocprofiler-sdk-roctx/roctx.h>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>

// Macro for checking GPU API return values
#define hipCheck(call)                                                                          \
do{                                                                                             \
    hipError_t gpuErr = call;                                                                   \
    if(hipSuccess != gpuErr){                                                                   \
        printf("GPU API Error - %s:%d: '%s'\n", __FILE__, __LINE__, hipGetErrorString(gpuErr)); \
        exit(1);                                                                                \
    }                                                                                           \
}while(0)

#define WIDTH     1024
#define HEIGHT    1024

#define NUM       (WIDTH*HEIGHT)

#define THREADS_PER_BLOCK_X  16
#define THREADS_PER_BLOCK_Y  16
#define THREADS_PER_BLOCK_Z  1

__global__ void
vectoradd_float(float* __restrict__ a, const float* __restrict__ b, const float* __restrict__ c, int width, int height)

  {

      int x = blockDim.x * blockIdx.x + threadIdx.x;
      int y = blockDim.y * blockIdx.y + threadIdx.y;

      int i = y * width + x;
      if ( i < (width * height)) {
        a[i] = b[i] + c[i];
      }

  }

using namespace std;

int main(int argc, char* argv[]) {

  float* hostA;
  float* hostB;
  float* hostC;

  float* deviceA;
  float* deviceB;
  float* deviceC;

  hipDeviceProp_t devProp;
  hipCheck(hipGetDeviceProperties(&devProp, 0));
  cerr << " System minor " << devProp.minor << endl;
  cerr << " System major " << devProp.major << endl;
  cerr << " agent prop name " << devProp.name << endl;
  cerr << "hip Device prop succeeded " << endl ;

  cali::ConfigManager mgr;
  if (argc > 1)
    mgr.add(argv[1]);
  if (mgr.error()) {
    cerr << "cali::ConfigManager: " << mgr.error_msg() << endl;
    return 1;
  }

  cali_init();
  mgr.start();

  roctxRangePush("main");

  int i;
  int errors;

  hostA = (float*)malloc(NUM * sizeof(float));
  hostB = (float*)malloc(NUM * sizeof(float));
  hostC = (float*)malloc(NUM * sizeof(float));

  // initialize the input data
  for (i = 0; i < NUM; i++) {
    hostB[i] = (float)i;
    hostC[i] = (float)i*100.0f;
  }

  roctxRangePush("copy_h2d");

  hipCheck(hipMalloc((void**)&deviceA, NUM * sizeof(float)));
  hipCheck(hipMalloc((void**)&deviceB, NUM * sizeof(float)));
  hipCheck(hipMalloc((void**)&deviceC, NUM * sizeof(float)));

  hipCheck(hipMemcpy(deviceB, hostB, NUM*sizeof(float), hipMemcpyHostToDevice));
  hipCheck(hipMemcpy(deviceC, hostC, NUM*sizeof(float), hipMemcpyHostToDevice));

  roctxRangePop();
  roctxRangePush("vectoradd");

  //hipLaunchKernelGGL(vectoradd_float,
  //                dim3(WIDTH/THREADS_PER_BLOCK_X, HEIGHT/THREADS_PER_BLOCK_Y),
  //                dim3(THREADS_PER_BLOCK_X, THREADS_PER_BLOCK_Y),
  //                0, 0,
  //                deviceA ,deviceB ,deviceC ,WIDTH ,HEIGHT);
  dim3 grid(WIDTH/THREADS_PER_BLOCK_X, HEIGHT/THREADS_PER_BLOCK_Y);
  dim3 block(THREADS_PER_BLOCK_X, THREADS_PER_BLOCK_Y);
  vectoradd_float<<<grid, block, 0, 0>>>(deviceA, deviceB, deviceC, WIDTH, HEIGHT);

  roctxRangePop();
  roctxRangePush("copy_d2h");

  hipCheck(hipMemcpy(hostA, deviceA, NUM*sizeof(float), hipMemcpyDeviceToHost));

  roctxRangePop();
  roctxRangePush("verify");

  // verify the results
  errors = 0;
  for (i = 0; i < NUM; i++) {
    if (hostA[i] != (hostB[i] + hostC[i])) {
      errors++;
    }
  }
  if (errors!=0) {
    cerr << "FAILED: " << errors << " errors" << endl;
  } else {
    cerr << "PASSED!" << endl;
  }

  roctxRangePop();

  hipCheck(hipFree(deviceA));
  hipCheck(hipFree(deviceB));
  hipCheck(hipFree(deviceC));

  free(hostA);
  free(hostB);
  free(hostC);

  //hipResetDefaultAccelerator();
  roctxRangePop(); // main
  mgr.flush();

  return errors;
}
