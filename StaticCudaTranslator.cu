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
    
    // Pre-allocated memory pools for performance
    thread_local void* g_deviceInputPool = nullptr;
    thread_local void* g_deviceTransformPool = nullptr;
    thread_local void* g_deviceResultPool = nullptr;
    thread_local size_t g_poolSizeInput = 0;
    thread_local size_t g_poolSizeTransform = 0;
    thread_local size_t g_poolSizeResult = 0;
    
    // Maximum batch size for optimal GPU utilization
    constexpr size_t MAX_BATCH_WORDS = 10000;     // Process up to 10K words at once
    constexpr size_t MAX_BATCH_MAPPINGS = 50000; // Process up to 50K mappings at once (reasonable GPU memory usage)
    constexpr size_t MATRIX_DIM = 27;
    
    // Optimized CUDA kernel with shared memory and better memory access
    __global__ void binaryMatrixMultiplyKernel(
        const int* __restrict__ inputMatrix,      // N x 27 matrix (flattened)
        const int* __restrict__ transformMatrix,  // 27 x 27 matrix (flattened) 
        int* __restrict__ resultMatrix,           // N x 27 result matrix (flattened)
        int numWords,                             // N (number of words)
        int matrixSize                            // 27 (matrix dimension)
    ) {
        // Shared memory for transform matrix (27x27 = 729 ints = 2.9KB)
        __shared__ int sharedTransform[27 * 27];
        
        int row = blockIdx.y * blockDim.y + threadIdx.y;
        int col = blockIdx.x * blockDim.x + threadIdx.x;
        int tid = threadIdx.y * blockDim.x + threadIdx.x;
        int threadsPerBlock = blockDim.x * blockDim.y;
        
        // Collaboratively load transform matrix into shared memory
        for (int i = tid; i < 27 * 27; i += threadsPerBlock) {
            sharedTransform[i] = transformMatrix[i];
        }
        __syncthreads();
        
        if (row < numWords && col < matrixSize) {
            int sum = 0;
            
            // Compute dot product using shared memory transform matrix
            const int* inputRow = &inputMatrix[row * matrixSize];
            
            // Unroll loop for better performance (27 iterations)
            #pragma unroll
            for (int k = 0; k < 27; ++k) {
                sum += inputRow[k] & sharedTransform[k * 27 + col];
            }
            
            // For binary matrices, any non-zero result becomes 1
            resultMatrix[row * matrixSize + col] = (sum > 0) ? 1 : 0;
        }
    }
    
    // High-performance batch kernel for processing multiple mappings in parallel
    __global__ void batchMatrixMultiplyKernel(
        const int* __restrict__ inputMatrix,      // numWords x 27 matrix (same for all mappings)
        const int* __restrict__ transformBatch,   // numMappings x (27 x 27) matrices
        int* __restrict__ resultBatch,            // numMappings x (numWords x 27) results
        int numWords,                             // Number of words (typically 100)
        int numMappings                           // Number of mappings to process in batch
    ) {
        int mappingId = blockIdx.z;                              // Which mapping (0 to numMappings-1)
        int row = blockIdx.y * blockDim.y + threadIdx.y;         // Word index (0 to numWords-1)
        int col = blockIdx.x * blockDim.x + threadIdx.x;         // Output column (0 to 26)
        
        if (mappingId >= numMappings || row >= numWords || col >= 27) return;
        
        // Shared memory for this mapping's transform matrix (27x27 = 729 ints = 2.9KB)
        __shared__ int sharedTransform[27 * 27];
        
        int tid = threadIdx.y * blockDim.x + threadIdx.x;
        int threadsPerBlock = blockDim.x * blockDim.y;
        
        // Collaboratively load transform matrix for this mapping into shared memory
        const int* thisTransform = &transformBatch[mappingId * 27 * 27];
        for (int i = tid; i < 27 * 27; i += threadsPerBlock) {
            sharedTransform[i] = thisTransform[i];
        }
        __syncthreads();
        
        // Compute matrix multiplication for this specific mapping
        int sum = 0;
        const int* inputRow = &inputMatrix[row * 27];
        
        // Unroll the inner loop for better performance
        #pragma unroll
        for (int k = 0; k < 27; ++k) {
            sum += inputRow[k] & sharedTransform[k * 27 + col];
        }
        
        // Store result for this mapping
        int resultIndex = mappingId * (numWords * 27) + row * 27 + col;
        resultBatch[resultIndex] = (sum > 0) ? 1 : 0;
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
    
    void ensureMemoryPools(size_t maxWords) {
        size_t inputSize = maxWords * MATRIX_DIM * sizeof(int);
        size_t transformSize = MATRIX_DIM * MATRIX_DIM * sizeof(int);
        size_t resultSize = maxWords * MATRIX_DIM * sizeof(int);
        
        // Allocate or reallocate input pool if needed
        if (g_poolSizeInput < inputSize) {
            if (g_deviceInputPool) cudaFree(g_deviceInputPool);
            cudaMalloc(&g_deviceInputPool, inputSize);
            g_poolSizeInput = inputSize;
        }
        
        // Allocate or reallocate transform pool if needed  
        if (g_poolSizeTransform < transformSize) {
            if (g_deviceTransformPool) cudaFree(g_deviceTransformPool);
            cudaMalloc(&g_deviceTransformPool, transformSize);
            g_poolSizeTransform = transformSize;
        }
        
        // Allocate or reallocate result pool if needed
        if (g_poolSizeResult < resultSize) {
            if (g_deviceResultPool) cudaFree(g_deviceResultPool);
            cudaMalloc(&g_deviceResultPool, resultSize);
            g_poolSizeResult = resultSize;
        }
    }
    
    void cleanupMemoryPools() {
        if (g_deviceInputPool) {
            cudaFree(g_deviceInputPool);
            g_deviceInputPool = nullptr;
            g_poolSizeInput = 0;
        }
        if (g_deviceTransformPool) {
            cudaFree(g_deviceTransformPool);
            g_deviceTransformPool = nullptr;
            g_poolSizeTransform = 0;
        }
        if (g_deviceResultPool) {
            cudaFree(g_deviceResultPool);
            g_deviceResultPool = nullptr;
            g_poolSizeResult = 0;
        }
    }
    
    void cleanupCudaResources() {
        cleanupMemoryPools();
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
    
    // Cap at maximum batch size for optimal GPU utilization
    if (numWords > MAX_BATCH_WORDS) {
        throw std::runtime_error("Word count exceeds maximum CUDA batch size");
    }
    
    // Initialize CUDA if needed
    initializeCudaResources();
    
    // Ensure memory pools are large enough
    ensureMemoryPools(numWords);
    
    // Calculate sizes
    size_t inputSize = numWords * MATRIX_DIM * sizeof(int);
    size_t transformSize = MATRIX_DIM * MATRIX_DIM * sizeof(int);
    size_t resultSize = numWords * MATRIX_DIM * sizeof(int);
    
    // Flatten matrices for CUDA (reuse vectors to avoid allocations)
    static thread_local std::vector<int> flatInput, flatTransform, flatResult;
    flatInput.resize(numWords * MATRIX_DIM);
    flatTransform.resize(MATRIX_DIM * MATRIX_DIM);
    flatResult.resize(numWords * MATRIX_DIM);
    
    // Fill input matrix (optimized)
    for (size_t i = 0; i < numWords; ++i) {
        const auto& row = inputMatrix[i];
        std::copy(row.begin(), row.end(), flatInput.begin() + i * MATRIX_DIM);
    }
    
    // Fill transform matrix (optimized)
    for (size_t i = 0; i < MATRIX_DIM; ++i) {
        const auto& row = transformMatrix[i];
        std::copy(row.begin(), row.end(), flatTransform.begin() + i * MATRIX_DIM);
    }
    
    // Use pre-allocated device memory pools
    int* d_inputMatrix = static_cast<int*>(g_deviceInputPool);
    int* d_transformMatrix = static_cast<int*>(g_deviceTransformPool);
    int* d_resultMatrix = static_cast<int*>(g_deviceResultPool);
    
    // Copy data to device (async would be better, but sync for simplicity)
    cudaError_t error = cudaMemcpy(d_inputMatrix, flatInput.data(), inputSize, cudaMemcpyHostToDevice);
    if (error != cudaSuccess) {
        throw std::runtime_error("CUDA input copy failed: " + std::string(cudaGetErrorString(error)));
    }
    
    error = cudaMemcpy(d_transformMatrix, flatTransform.data(), transformSize, cudaMemcpyHostToDevice);
    if (error != cudaSuccess) {
        throw std::runtime_error("CUDA transform copy failed: " + std::string(cudaGetErrorString(error)));
    }
    
    // Optimize kernel launch parameters for better occupancy
    dim3 blockSize(32, 8);  // 256 threads per block (better than 16x16=256)
    dim3 gridSize((MATRIX_DIM + blockSize.x - 1) / blockSize.x, 
                  (numWords + blockSize.y - 1) / blockSize.y);
    
    // Launch kernel
    binaryMatrixMultiplyKernel<<<gridSize, blockSize>>>(
        d_inputMatrix, d_transformMatrix, d_resultMatrix, 
        static_cast<int>(numWords), MATRIX_DIM
    );
    
    // Check for kernel launch errors
    error = cudaGetLastError();
    if (error != cudaSuccess) {
        throw std::runtime_error("CUDA kernel launch failed: " + std::string(cudaGetErrorString(error)));
    }
    
    // Synchronize only when we need the result
    error = cudaDeviceSynchronize();
    if (error != cudaSuccess) {
        throw std::runtime_error("CUDA kernel execution failed: " + std::string(cudaGetErrorString(error)));
    }
    
    // Copy result back to host
    error = cudaMemcpy(flatResult.data(), d_resultMatrix, resultSize, cudaMemcpyDeviceToHost);
    if (error != cudaSuccess) {
        throw std::runtime_error("CUDA result copy failed: " + std::string(cudaGetErrorString(error)));
    }
    
    // Unflatten result matrix (optimized)
    for (size_t i = 0; i < numWords; ++i) {
        auto& row = resultMatrix[i];
        std::copy(flatResult.begin() + i * MATRIX_DIM, 
                  flatResult.begin() + (i + 1) * MATRIX_DIM, 
                  row.begin());
    }
}

// High-performance batch processing for multiple mappings
void StaticTranslator::performBatchMatrixMultiplicationCuda(
    const std::vector<std::vector<int>>& inputMatrix,
    const std::vector<std::vector<std::vector<int>>>& transformMatrices,
    std::vector<std::vector<std::vector<int>>>& resultMatrices
) {
    size_t numWords = inputMatrix.size();
    size_t numMappings = transformMatrices.size();
    
    if (numWords == 0 || numMappings == 0) return;
    
    // Cap at maximum batch sizes for optimal GPU utilization
    if (numWords > MAX_BATCH_WORDS) {
        throw std::runtime_error("Word count exceeds maximum CUDA batch size");
    }
    if (numMappings > MAX_BATCH_MAPPINGS) {
        throw std::runtime_error("Mapping count exceeds maximum CUDA batch size");
    }
    
    // Initialize CUDA if needed
    initializeCudaResources();
    
    // Calculate memory requirements
    size_t inputSize = numWords * MATRIX_DIM * sizeof(int);
    size_t transformBatchSize = numMappings * MATRIX_DIM * MATRIX_DIM * sizeof(int);
    size_t resultBatchSize = numMappings * numWords * MATRIX_DIM * sizeof(int);
    
    // Ensure memory pools are large enough for batch processing
    ensureMemoryPools(std::max(numWords, numMappings * numWords));
    
    // Allocate additional memory for transform and result batches if needed
    void* d_transformBatch = nullptr;
    void* d_resultBatch = nullptr;
    
    cudaError_t error = cudaMalloc(&d_transformBatch, transformBatchSize);
    if (error != cudaSuccess) {
        throw std::runtime_error("CUDA transform batch allocation failed: " + std::string(cudaGetErrorString(error)));
    }
    
    error = cudaMalloc(&d_resultBatch, resultBatchSize);
    if (error != cudaSuccess) {
        cudaFree(d_transformBatch);
        throw std::runtime_error("CUDA result batch allocation failed: " + std::string(cudaGetErrorString(error)));
    }
    
    // Prepare host data (reuse thread-local vectors for efficiency)
    static thread_local std::vector<int> flatInput, flatTransforms, flatResults;
    flatInput.resize(numWords * MATRIX_DIM);
    flatTransforms.resize(numMappings * MATRIX_DIM * MATRIX_DIM);
    flatResults.resize(numMappings * numWords * MATRIX_DIM);
    
    // Fill input matrix (same for all mappings)
    for (size_t i = 0; i < numWords; ++i) {
        const auto& row = inputMatrix[i];
        std::copy(row.begin(), row.end(), flatInput.begin() + i * MATRIX_DIM);
    }
    
    // Fill transform matrices batch
    for (size_t mapping = 0; mapping < numMappings; ++mapping) {
        size_t offset = mapping * MATRIX_DIM * MATRIX_DIM;
        for (size_t i = 0; i < MATRIX_DIM; ++i) {
            const auto& row = transformMatrices[mapping][i];
            std::copy(row.begin(), row.end(), flatTransforms.begin() + offset + i * MATRIX_DIM);
        }
    }
    
    // Copy data to device
    error = cudaMemcpy(g_deviceInputPool, flatInput.data(), inputSize, cudaMemcpyHostToDevice);
    if (error != cudaSuccess) {
        cudaFree(d_transformBatch);
        cudaFree(d_resultBatch);
        throw std::runtime_error("CUDA input copy failed: " + std::string(cudaGetErrorString(error)));
    }
    
    error = cudaMemcpy(d_transformBatch, flatTransforms.data(), transformBatchSize, cudaMemcpyHostToDevice);
    if (error != cudaSuccess) {
        cudaFree(d_transformBatch);
        cudaFree(d_resultBatch);
        throw std::runtime_error("CUDA transform batch copy failed: " + std::string(cudaGetErrorString(error)));
    }
    
    // Configure 3D grid for batch processing
    dim3 blockSize(16, 8, 1);  // 128 threads per block (good for most GPUs)
    dim3 gridSize(
        (MATRIX_DIM + blockSize.x - 1) / blockSize.x,      // Columns
        (numWords + blockSize.y - 1) / blockSize.y,        // Rows  
        numMappings                                         // Mappings (Z dimension)
    );
    
    // Launch batch kernel - processes all mappings in parallel!
    batchMatrixMultiplyKernel<<<gridSize, blockSize>>>(
        static_cast<int*>(g_deviceInputPool),
        static_cast<int*>(d_transformBatch),
        static_cast<int*>(d_resultBatch),
        static_cast<int>(numWords),
        static_cast<int>(numMappings)
    );
    
    // Check for kernel launch errors
    error = cudaGetLastError();
    if (error != cudaSuccess) {
        cudaFree(d_transformBatch);
        cudaFree(d_resultBatch);
        throw std::runtime_error("CUDA batch kernel launch failed: " + std::string(cudaGetErrorString(error)));
    }
    
    // Synchronize to wait for completion
    error = cudaDeviceSynchronize();
    if (error != cudaSuccess) {
        cudaFree(d_transformBatch);
        cudaFree(d_resultBatch);
        throw std::runtime_error("CUDA batch kernel execution failed: " + std::string(cudaGetErrorString(error)));
    }
    
    // Copy results back to host
    error = cudaMemcpy(flatResults.data(), d_resultBatch, resultBatchSize, cudaMemcpyDeviceToHost);
    if (error != cudaSuccess) {
        cudaFree(d_transformBatch);
        cudaFree(d_resultBatch);
        throw std::runtime_error("CUDA batch result copy failed: " + std::string(cudaGetErrorString(error)));
    }
    
    // Clean up temporary device memory
    cudaFree(d_transformBatch);
    cudaFree(d_resultBatch);
    
    // Unflatten results for each mapping
    resultMatrices.resize(numMappings);
    for (size_t mapping = 0; mapping < numMappings; ++mapping) {
        resultMatrices[mapping].resize(numWords, std::vector<int>(MATRIX_DIM));
        size_t offset = mapping * numWords * MATRIX_DIM;
        
        for (size_t i = 0; i < numWords; ++i) {
            auto& row = resultMatrices[mapping][i];
            std::copy(flatResults.begin() + offset + i * MATRIX_DIM,
                      flatResults.begin() + offset + (i + 1) * MATRIX_DIM,
                      row.begin());
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