extern "C"{
#include <Python.h>
#include <TH/TH.h>
    //#include <THC/THC.h>
}

#include <sys/mman.h>

#include <chrono>
#include <cstring>
#include <iostream>

//#include "common.hh"

//extern THCState *state;

extern "C" PyObject* device_to_host_copy(PyObject *, PyObject *args, PyObject *){

    // THFloatTensor *dest = (THFloatTensor*) dest_;
    // THCudaTensor *src = (THCudaTensor*) src_;
    
    // // sanity checking
    // THArgCheck(THCudaTensor_isContiguous(state, src), 2, "src tensor must be contiguous");
    // THArgCheck(THCudaTensor_nDimension(state, src) == 4, 2, "src tensor must be 4D");

    // // munge the blobs
    // size_t n_size = THCudaTensor_size(state, src, 0);
    // size_t c_size = THCudaTensor_size(state, src, 1);
    // size_t h_size = THCudaTensor_size(state, src, 2);
    // size_t w_size = THCudaTensor_size(state, src, 3);
    // size_t src_size = n_size * c_size * h_size * w_size;
    // float* src_data = THCudaTensor_data(state, src);

    // float *dest_data = THFloatTensor_data(dest);
    // TorchFloatBlob4D dest_blob{dest, dest_data, 0, 0, 0, 0};
    // dest_blob.resize(n_size, c_size, h_size, w_size);

    // // more sanity checking
    // THArgCheck(THFloatTensor_isContiguous(dest), 2, "destination tensor must be contiguous");
    // THArgCheck(THFloatTensor_nDimension(dest) == 4, 2, "destination tensor must be 4D");
    // THArgCheck(sizeof(float)*src_size == sizeof(float)*dest_blob.size(), 2, "sizes do not match");

    // // copy memory
    // THCudaCheck(cudaMemcpy(dest_blob.data, src_data, sizeof(float)*dest_blob.size(), cudaMemcpyDeviceToHost));
    
    Py_RETURN_NONE;
    
}
