// Local matched-workload probe; GPU Unlock benchmark session 8855e7b.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cuda_runtime.h>
#include <cuda_fp16.h>
#include <cuda_bf16.h>
#include <cublas_v2.h>

static void cu(cudaError_t code) {
    if(code!=cudaSuccess){std::fprintf(stderr,"CUDA: %s\n",cudaGetErrorString(code));std::exit(2);}
}
static void blas(cublasStatus_t code) {
    if(code!=CUBLAS_STATUS_SUCCESS){std::fprintf(stderr,"cuBLAS status %d\n",(int)code);std::exit(2);}
}
template<class T> static T convert(float x);
template<> __half convert(float x){return __float2half_rn(x);}
template<> __nv_bfloat16 convert(float x){return __float2bfloat16_rn(x);}

template<class T> static bool bench(cublasHandle_t handle,const char *mode,cudaDataType type,int n) {
    const size_t cells=(size_t)n*n;
    std::vector<T> a(cells),b(cells);
    std::vector<float> result(cells);
    // The performance inputs have a cheap exact reference for every output
    // cell. Separate validate_gemm exercises varied seeded numerical inputs.
    for(int col=0;col<n;col++)for(int row=0;row<n;row++){
        a[row+(size_t)col*n]=convert<T>((row%13-6)/8.0f);
        b[row+(size_t)col*n]=convert<T>((col%11-5)/8.0f);
    }
    T *da=nullptr,*db=nullptr;float *dc=nullptr;
    cu(cudaMalloc(&da,cells*sizeof(T)));cu(cudaMalloc(&db,cells*sizeof(T)));cu(cudaMalloc(&dc,cells*sizeof(float)));
    cu(cudaMemcpy(da,a.data(),cells*sizeof(T),cudaMemcpyHostToDevice));
    cu(cudaMemcpy(db,b.data(),cells*sizeof(T),cudaMemcpyHostToDevice));
    const float alpha=1,beta=0;
    auto launch=[&](){blas(cublasGemmEx(handle,CUBLAS_OP_N,CUBLAS_OP_N,n,n,n,&alpha,
        da,type,n,db,type,n,&beta,dc,CUDA_R_32F,n,CUBLAS_COMPUTE_32F,CUBLAS_GEMM_DEFAULT_TENSOR_OP));};
    for(int i=0;i<10;i++)launch();cu(cudaDeviceSynchronize());
    std::vector<double> rates;
    std::printf("{\"mode\":\"%s\",\"n\":%d,\"warmups\":10,\"iterations\":20,\"reps\":5,\"times_ms\":[",mode,n);
    for(int rep=0;rep<5;rep++){
        cudaEvent_t start,stop;cu(cudaEventCreate(&start));cu(cudaEventCreate(&stop));
        cu(cudaEventRecord(start));for(int i=0;i<20;i++)launch();cu(cudaEventRecord(stop));cu(cudaEventSynchronize(stop));
        float elapsed=0;cu(cudaEventElapsedTime(&elapsed,start,stop));
        if(!(elapsed>0)||!std::isfinite(elapsed)){std::fprintf(stderr,"invalid timing\n");std::exit(3);}
        double ms=elapsed/20.0;rates.push_back(2.0*n*n*n/(ms*1e9));
        std::printf("%s%.8f",rep?",":"",ms);
        cu(cudaEventDestroy(start));cu(cudaEventDestroy(stop));
    }
    cu(cudaMemcpy(result.data(),dc,cells*sizeof(float),cudaMemcpyDeviceToHost));
    size_t failures=0;
    for(int col=0;col<n;col++)for(int row=0;row<n;row++){
        float expected=n*((row%13-6)/8.0f)*((col%11-5)/8.0f);
        if(result[row+(size_t)col*n]!=expected)failures++;
    }
    std::sort(rates.begin(),rates.end());
    std::printf("],\"median_TF_s\":%.6f,\"best_TF_s\":%.6f,\"cells_checked\":%zu,\"failures\":%zu}\n",
        rates[2],rates.back(),cells,failures);std::fflush(stdout);
    cu(cudaFree(da));cu(cudaFree(db));cu(cudaFree(dc));return failures==0;
}
int main(){
    cublasHandle_t handle;blas(cublasCreate(&handle));bool pass=true;
    for(int n:{1024,4096,8192}){
        pass&=bench<__half>(handle,"FP16/FP32",CUDA_R_16F,n);
        pass&=bench<__nv_bfloat16>(handle,"BF16/FP32",CUDA_R_16BF,n);
    }
    blas(cublasDestroy(handle));return pass?0:1;
}
