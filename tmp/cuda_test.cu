#include <iostream>
#include <math.h>

using namespace std;


__global__
void add(int n, float *x, float *y){
    int index = threadIdx.x;
    int stride = blockDim.x;
    for (int i = index; i < n; i += stride){
        y[i] = x[i] + y[i];
    }
}

int main(){    
    int N = 1 << 24;
    float *x, *y;
    cudaMallocManaged(&x, N * sizeof(float));
    cudaMallocManaged(&y, N * sizeof(float));

    for (int i = 0; i < N; i++){
        x[i] = 1.0f;
        y[i] = 2.0f;
    }

    int blockSize = 1024;
    int gridSize = 12;

    add<<<gridSize, blockSize>>>(N, x, y);

    cudaDeviceSynchronize();

    float maxError = 0.0f;
    for (int i = 0; i < N; i++){
        maxError = fmax(maxError, fabs(y[i] - 3.0f));
    }
    std::cout << "Max error: " << maxError << std::endl;

    cudaFree(x);
    cudaFree(y);
    return 0;
}