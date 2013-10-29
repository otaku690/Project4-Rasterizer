// CIS565 CUDA Rasterizer: A simple rasterization pipeline for Patrick Cozzi's CIS565: GPU Computing at the University of Pennsylvania
// Written by Yining Karl Li, Copyright (c) 2012 University of Pennsylvania

#ifndef RASTERIZEKERNEL_H
#define RASTERIZEKERNEL_H

#include <stdio.h>
#include <thrust/random.h>
#include <cuda.h>
#include <cmath>
#include "glm/glm.hpp"
#include "statVal.h"

#if CUDA_VERSION >= 5000
    #include <helper_math.h>
#else
    #include <cutil_math.h>
#endif

void initDeviceBuf(ushort width, ushort height, float* vbo, int vbosize, float* cbo, int cbosize, 
                 int* ibo, int ibosize );
void kernelCleanup();
void cudaRasterizeCore(uchar4* pos, ushort width, ushort height, float frame, float* vbo, int vbosize,
                       float* cbo, int cbosize, int* ibo, int ibosize, VertUniform &vsUniform, FragUniform &fsUniform
                       );

#endif //RASTERIZEKERNEL_H
