#ifndef _NNFC_JPEG_H
#define _NNFC_JPEG_H

#include <cstdint>
#include <vector>

#include "nnfc_CXXAPI.hh"
#include "tensor.hh"

namespace nnfc {

    class JPEGEncoder
    {
    private:
        int quantizer_;
        
    public:
        JPEGEncoder(int quantizer);
        ~JPEGEncoder();

        std::vector<uint8_t> forward(nn::Tensor<float, 3> input);
        nn::Tensor<float, 3> backward(nn::Tensor<float, 3> input);

        static nnfc::cxxapi::constructor_type_list initialization_params() { return { {"quantizer", typeid(int)} }; }
    };
    
    class JPEGDecoder
    {
    private:
        
    public:
        JPEGDecoder();
        ~JPEGDecoder();

        nn::Tensor<float, 3> forward(std::vector<uint8_t> input);
        nn::Tensor<float, 3> backward(nn::Tensor<float, 3> input);

        static nnfc::cxxapi::constructor_type_list initialization_params() { return {}; }
    };

}

#endif // _NNFC_JPEG_H