#include "StaticTranslator.h"
#include <atomic>
#include <sstream>

#ifdef __NVCC__
#include <cuda_runtime.h>
#include <cublas_v2.h>

// Global static CUDA resources (thread-safe initialization)
namespace {
    std::atomic<bool> g_isInitialized{false};
    cublasHandle_t g_cublasHandle = nullptr;
    
    // CUDA kernel for binary matrix multiplication
    __global__ void binaryMatrixMultiplyKernel(
        const int* inputMatrix,      // N x 27 matrix (flattened)
        const int* transformMatrix,  // 27 x 27 matrix (flattened) 
        int* resultMatrix,           // N x 27 result matrix (flattened)
        int numWords,                // N (number of words)
        int matrixSize               // 27 (matrix dimension)
    ) {
        int row = blockIdx.y * blockDim.y + threadIdx.y;
        int col = blockIdx.x * blockDim.x + threadIdx.x;
        
        if (row < numWords && col < matrixSize) {
            int sum = 0;
            
            // Compute dot product for binary matrices
            for (int k = 0; k < matrixSize; ++k) {
                sum += inputMatrix[row * matrixSize + k] & transformMatrix[k * matrixSize + col];
            }
            
            // For binary matrices, any non-zero result becomes 1
            resultMatrix[row * matrixSize + col] = (sum > 0) ? 1 : 0;
        }
    }
    
    void initializeCudaResources() {
        if (g_isInitialized.load()) return;
        
        // Simple initialization - no mutex needed since we're not sharing resources
        if (!g_isInitialized.exchange(true)) {
            // Initialize cuBLAS (optional - not currently used)
            cublasStatus_t status = cublasCreate(&g_cublasHandle);
            if (status != CUBLAS_STATUS_SUCCESS) {
                g_isInitialized.store(false);
                throw std::runtime_error("Failed to create cuBLAS handle");
            }
        }
    }
    
    void cleanupCudaResources() {
        if (g_cublasHandle) {
            cublasDestroy(g_cublasHandle);
            g_cublasHandle = nullptr;
        }
        g_isInitialized.store(false);
    }
}

void StaticTranslator::performMatrixMultiplicationCuda(
    const std::vector<std::vector<int>>& inputMatrix,
    const std::vector<std::vector<int>>& transformMatrix,
    std::vector<std::vector<int>>& resultMatrix
) {
    size_t numWords = inputMatrix.size();
    if (numWords == 0) return;
    
    if (numWords > 1000) {
        throw std::runtime_error("Word count exceeds maximum supported size for CUDA");
    }
    
    // Initialize CUDA if needed (lightweight - no shared memory allocation)
    initializeCudaResources();
    
    // Allocate per-operation device memory (no sharing between threads)
    int* d_inputMatrix = nullptr;
    int* d_transformMatrix = nullptr; 
    int* d_resultMatrix = nullptr;
    
    // Allocate device memory for this operation
    size_t inputSize = numWords * 27 * sizeof(int);
    size_t transformSize = 27 * 27 * sizeof(int);
    size_t resultSize = numWords * 27 * sizeof(int);
    
    cudaError_t error;
    
    // Allocate input matrix
    error = cudaMalloc(&d_inputMatrix, inputSize);
    if (error != cudaSuccess) {
        throw std::runtime_error("CUDA memory allocation failed (input): " + std::string(cudaGetErrorString(error)));
    }
    
    // Allocate transform matrix  
    error = cudaMalloc(&d_transformMatrix, transformSize);
    if (error != cudaSuccess) {
        cudaFree(d_inputMatrix);
        throw std::runtime_error("CUDA memory allocation failed (transform): " + std::string(cudaGetErrorString(error)));
    }
    
    // Allocate result matrix
    error = cudaMalloc(&d_resultMatrix, resultSize);
    if (error != cudaSuccess) {
        cudaFree(d_inputMatrix);
        cudaFree(d_transformMatrix);
        throw std::runtime_error("CUDA memory allocation failed (result): " + std::string(cudaGetErrorString(error)));
    }
    
    // Flatten matrices for CUDA
    std::vector<int> flatInput(numWords * 27);
    std::vector<int> flatTransform(27 * 27);
    std::vector<int> flatResult(numWords * 27);
    
    // Fill input matrix
    for (size_t i = 0; i < numWords; ++i) {
        for (size_t j = 0; j < 27; ++j) {
            flatInput[i * 27 + j] = inputMatrix[i][j];
        }
    }
    
    // Fill transform matrix
    for (size_t i = 0; i < 27; ++i) {
        for (size_t j = 0; j < 27; ++j) {
            flatTransform[i * 27 + j] = transformMatrix[i][j];
        }
    }
    
    // Copy data to device
    error = cudaMemcpy(d_inputMatrix, flatInput.data(), inputSize, cudaMemcpyHostToDevice);
    if (error != cudaSuccess) {
        cudaFree(d_inputMatrix);
        cudaFree(d_transformMatrix);
        cudaFree(d_resultMatrix);
        throw std::runtime_error("CUDA memory copy failed (input): " + std::string(cudaGetErrorString(error)));
    }
    
    error = cudaMemcpy(d_transformMatrix, flatTransform.data(), transformSize, cudaMemcpyHostToDevice);
    if (error != cudaSuccess) {
        cudaFree(d_inputMatrix);
        cudaFree(d_transformMatrix);
        cudaFree(d_resultMatrix);
        throw std::runtime_error("CUDA memory copy failed (transform): " + std::string(cudaGetErrorString(error)));
    }
    
    // Set up kernel launch parameters
    dim3 blockSize(16, 16);
    dim3 gridSize((27 + blockSize.x - 1) / blockSize.x, (numWords + blockSize.y - 1) / blockSize.y);
    
    // Launch kernel
    binaryMatrixMultiplyKernel<<<gridSize, blockSize>>>(
        d_inputMatrix, d_transformMatrix, d_resultMatrix, 
        static_cast<int>(numWords), 27
    );
    
    // Check for kernel launch errors
    error = cudaGetLastError();
    if (error != cudaSuccess) {
        cudaFree(d_inputMatrix);
        cudaFree(d_transformMatrix);
        cudaFree(d_resultMatrix);
        throw std::runtime_error("CUDA kernel launch failed: " + std::string(cudaGetErrorString(error)));
    }
    
    // Wait for completion
    error = cudaDeviceSynchronize();
    if (error != cudaSuccess) {
        cudaFree(d_inputMatrix);
        cudaFree(d_transformMatrix);
        cudaFree(d_resultMatrix);
        throw std::runtime_error("CUDA kernel execution failed: " + std::string(cudaGetErrorString(error)));
    }
    
    // Copy result back to host
    error = cudaMemcpy(flatResult.data(), d_resultMatrix, resultSize, cudaMemcpyDeviceToHost);
    if (error != cudaSuccess) {
        cudaFree(d_inputMatrix);
        cudaFree(d_transformMatrix);
        cudaFree(d_resultMatrix);
        throw std::runtime_error("CUDA memory copy failed (result): " + std::string(cudaGetErrorString(error)));
    }
    
    // Clean up device memory
    cudaFree(d_inputMatrix);
    cudaFree(d_transformMatrix);
    cudaFree(d_resultMatrix);
    
    // Unflatten result matrix
    for (size_t i = 0; i < numWords; ++i) {
        for (size_t j = 0; j < 27; ++j) {
            resultMatrix[i][j] = flatResult[i * 27 + j];
        }
    }
}

// Helper functions for CUDA availability and device info
bool isCudaAvailable_impl() {
    int deviceCount = 0;
    cudaError_t error = cudaGetDeviceCount(&deviceCount);
    return (error == cudaSuccess && deviceCount > 0);
}

std::string getCudaDeviceInfo_impl() {
    if (!isCudaAvailable_impl()) {
        return "CUDA not available";
    }
    
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    
    std::ostringstream info;
    info << "Device: " << prop.name 
         << ", Compute Capability: " << prop.major << "." << prop.minor
         << ", Global Memory: " << (prop.totalGlobalMem / (1024 * 1024)) << " MB"
         << ", Multiprocessors: " << prop.multiProcessorCount;
    
    return info.str();
}

// Register cleanup function to be called at program exit
static struct CudaCleanup {
    ~CudaCleanup() {
        cleanupCudaResources();
    }
} g_cudaCleanup;

#else

void StaticTranslator::performMatrixMultiplicationCuda(
    const std::vector<std::vector<int>>& inputMatrix,
    const std::vector<std::vector<int>>& transformMatrix,
    std::vector<std::vector<int>>& resultMatrix
) {
    throw std::runtime_error("CUDA support was not compiled into this binary");
}

bool isCudaAvailable_impl() {
    return false;
}

std::string getCudaDeviceInfo_impl() {
    return "CUDA not compiled";
}

#endif