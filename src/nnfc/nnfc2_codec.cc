#include <any>
#include <cstdint>
#include <iostream>
#include <vector>

#include "nnfc2_codec.hh"
#include "tensor.hh"
#include "codec/utils.hh"

static constexpr int BLOCK_WIDTH = 8;

static constexpr int ZIGZAG_ORDER[][2] = {
    {0, 0},
    {1, 0}, {0, 1},
    {0, 2}, {1, 1}, {2, 0},
    {3, 0}, {2, 1}, {1, 2}, {0, 3},
    {0, 4}, {1, 3}, {2, 2}, {3, 1}, {4, 0},
    {5, 0}, {4, 1}, {3, 2}, {2, 3}, {1, 4}, {0, 5},
    {0, 6}, {1, 5}, {2, 4}, {3, 3}, {4, 2}, {5, 1}, {6, 0},
    {7, 0}, {6, 1}, {5, 2}, {4, 3}, {3, 4}, {2, 5}, {1, 6}, {0, 7},
    {1, 7}, {2, 6}, {3, 5}, {4, 4}, {5, 3}, {6, 2}, {7, 1}, 
    {7, 2}, {6, 3}, {5, 4}, {4, 5}, {3, 6}, {2, 7},
    {3, 7}, {4, 6}, {5, 5}, {6, 4}, {7, 3},  
    {7, 4}, {6, 5}, {5, 6}, {4, 7},
    {5, 7}, {6, 6}, {7, 5},
    {7, 6}, {6, 7},
    {7, 7}
};
static constexpr int ZIGZAG_LENGTH = sizeof(ZIGZAG_ORDER) / sizeof(ZIGZAG_ORDER[0]);


nnfc::NNFC2Encoder::NNFC2Encoder() :
    quantizer_(1) {}

nnfc::NNFC2Encoder::~NNFC2Encoder() {}

std::vector<uint8_t> nnfc::NNFC2Encoder::forward(const nn::Tensor<float, 3> t_input) const {

  const uint64_t dim0 = t_input.dimension(0);
  const uint64_t dim1 = t_input.dimension(1);
  const uint64_t dim2 = t_input.dimension(2);

  assert(dim1 % BLOCK_WIDTH == 0);
  assert(dim2 % BLOCK_WIDTH == 0);
  
  std::vector<uint8_t> encoding;

  // add header
  {
      uint64_t dim0_ = dim0;
      uint64_t dim1_ = dim1;
      uint64_t dim2_ = dim2;
      uint8_t *dim0_bytes = reinterpret_cast<uint8_t *>(&dim0_);
      uint8_t *dim1_bytes = reinterpret_cast<uint8_t *>(&dim1_);
      uint8_t *dim2_bytes = reinterpret_cast<uint8_t *>(&dim2_);
      for (size_t i = 0; i < sizeof(uint64_t); i++) {
        encoding.push_back(dim0_bytes[i]);
      }
      for (size_t i = 0; i < sizeof(uint64_t); i++) {
        encoding.push_back(dim1_bytes[i]);
      }
      for (size_t i = 0; i < sizeof(uint64_t); i++) {
        encoding.push_back(dim2_bytes[i]);
      }
  }
  
  // code block for doing 8-bit linear quantization. DCT for integers does not exist yet.
  // Eigen::Tensor<uint8_t, 3, Eigen::RowMajor> q_input =
  //     ((t_input.tensor() - min) * (255 / (max - min))).cast<uint8_t>();
  // Eigen::Tensor<float, 3, Eigen::RowMajor> input_t =
  //     (q_input.cast<float>() * ((max - min) / 255)) + min;
  // nn::Tensor<float, 3> input(input_t);

  nn::Tensor<float, 3> dct_input(std::move(codec::utils::dct(t_input, BLOCK_WIDTH)));
  nn::Tensor<float, 3> input(dct_input);

  const float dct_min = dct_input.minimum();
  const float dct_max = dct_input.maximum();
  //Eigen::Tensor<uint8_t, 3, Eigen::RowMajor> input = ((255 * (dct_input.tensor() - dct_min)) / (quantizer_ * dct_max)).cast<uint8_t>();

  // add min and max
  {
      float min_ = dct_min;
      float max_ = dct_max;
      uint8_t *min_bytes = reinterpret_cast<uint8_t *>(&min_);
      uint8_t *max_bytes = reinterpret_cast<uint8_t *>(&max_);
      for (size_t i = 0; i < sizeof(float); i++) {
        encoding.push_back(min_bytes[i]);
      }
      for (size_t i = 0; i < sizeof(float); i++) {
        encoding.push_back(max_bytes[i]);
      }
  }

  // add quantizer
  {
      int32_t quantizer = quantizer_;
      uint8_t *quantizer_bytes = reinterpret_cast<uint8_t *>(&quantizer);
      for (size_t i = 0; i < sizeof(int32_t); i++) {
          encoding.push_back(quantizer_bytes[i]);
      }
  }
  
  // arithmetic encode and serialize data
  for (size_t channel = 0; channel < dim0; channel++) {
      for (size_t block_row = 0; block_row < dim1 / BLOCK_WIDTH; block_row++) {
          for (size_t block_col = 0; block_col < dim2 / BLOCK_WIDTH; block_col++) {
              
              for (size_t i = 0; i < ZIGZAG_LENGTH; i++) {
                  const size_t row_offset = BLOCK_WIDTH*block_row + ZIGZAG_ORDER[i][0];
                  const size_t col_offset = BLOCK_WIDTH*block_col + ZIGZAG_ORDER[i][1];

                  // check if (value / quantizer) > 0, if not encode EOB and break
                  // otherwise encode the symbol as normal
                  float element = input(channel, row_offset, col_offset);
                  uint8_t *bytes = reinterpret_cast<uint8_t *>(&element);
                  
                  encoding.push_back(bytes[0]);
                  encoding.push_back(bytes[1]);
                  encoding.push_back(bytes[2]);
                  encoding.push_back(bytes[3]);
              }
          }
      }
  }

  // straight forward serialization
  // for (size_t i = 0; i < dim0; i++) {
  //   for (size_t j = 0; j < dim1; j++) {
  //     for (size_t k = 0; k < dim2; k++) {
  //       float element = input(i, j, k);
  //       uint8_t *bytes = reinterpret_cast<uint8_t *>(&element);

  //       encoding.push_back(bytes[0]);
  //       encoding.push_back(bytes[1]);
  //       encoding.push_back(bytes[2]);
  //       encoding.push_back(bytes[3]);
  //     }
  //   }
  // }

  return encoding;
}

nn::Tensor<float, 3> nnfc::NNFC2Encoder::backward(const nn::Tensor<float, 3> input) const {
   return input;
}

nnfc::NNFC2Decoder::NNFC2Decoder() {}

nnfc::NNFC2Decoder::~NNFC2Decoder() {}

nn::Tensor<float, 3> nnfc::NNFC2Decoder::forward(const std::vector<uint8_t> input) const {

  // deserialize
  // undo arithmetic encoding
  // unquantize
  // undo dct

  uint64_t dim0;
  uint64_t dim1;
  uint64_t dim2;
  {
      uint8_t *dim0_bytes = reinterpret_cast<uint8_t *>(&dim0);
      uint8_t *dim1_bytes = reinterpret_cast<uint8_t *>(&dim1);
      uint8_t *dim2_bytes = reinterpret_cast<uint8_t *>(&dim2);
      
      constexpr size_t dim0_offset = 0 * sizeof(uint64_t);
      constexpr size_t dim1_offset = 1 * sizeof(uint64_t);
      constexpr size_t dim2_offset = 2 * sizeof(uint64_t);
      for (size_t i = 0; i < sizeof(uint64_t); i++) {
          dim0_bytes[i] = input[i + dim0_offset];
          dim1_bytes[i] = input[i + dim1_offset];
          dim2_bytes[i] = input[i + dim2_offset];
      }
  }
  
  float min;
  float max;
  {
      uint8_t *min_bytes = reinterpret_cast<uint8_t *>(&min);
      uint8_t *max_bytes = reinterpret_cast<uint8_t *>(&max);

      constexpr size_t min_offset = 3 * sizeof(uint64_t);
      constexpr size_t max_offset = sizeof(float) + 3 * sizeof(uint64_t);
      for (size_t i = 0; i < sizeof(float); i++) {
          min_bytes[i] = input[i + min_offset];
      }
      for (size_t i = 0; i < sizeof(float); i++) {
          max_bytes[i] = input[i + max_offset];
      }
  }

  int32_t quantizer;
  {
      uint8_t *quantizer_bytes = reinterpret_cast<uint8_t *>(&quantizer);

      constexpr size_t quantizer_offset = 2*sizeof(float) + 3*sizeof(uint64_t);
      for (size_t i = 0; i < sizeof(int32_t); i++) {
          quantizer_bytes[i] = input[i + quantizer_offset];
      }
  }
  
  assert(dim1 % BLOCK_WIDTH == 0);
  assert(dim2 % BLOCK_WIDTH == 0);

  constexpr size_t header_offset = sizeof(int32_t) + 2*sizeof(float) + 3*sizeof(uint64_t);
  
  nn::Tensor<float, 3> f_output(dim0, dim1, dim2);

  for (size_t channel = 0; channel < dim0; channel++) {
      for (size_t block_row = 0; block_row < dim1 / BLOCK_WIDTH; block_row++) {
          for (size_t block_col = 0; block_col < dim2 / BLOCK_WIDTH; block_col++) {
              
              for (size_t i = 0; i < ZIGZAG_LENGTH; i++) {
                  const size_t row_offset = BLOCK_WIDTH*block_row + ZIGZAG_ORDER[i][0];
                  const size_t col_offset = BLOCK_WIDTH*block_col + ZIGZAG_ORDER[i][1];
                  
                  float element;
                  uint8_t *bytes = reinterpret_cast<uint8_t *>(&element);
                  
                  size_t offset = sizeof(float) * (dim1 * dim2 * channel +
                                                   BLOCK_WIDTH * dim2 * block_row +
                                                   BLOCK_WIDTH * BLOCK_WIDTH * block_col + i) + header_offset;
                  bytes[0] = input[offset];
                  bytes[1] = input[offset + 1];
                  bytes[2] = input[offset + 2];
                  bytes[3] = input[offset + 3];
                  
                  f_output(channel, row_offset, col_offset) = element;
              }
          }
      }
  }
  
  // straight forward deserialization
  // for (size_t i = 0; i < dim0; i++) {
  //   for (size_t j = 0; j < dim1; j++) {
  //     for (size_t k = 0; k < dim2; k++) {
  //       float element;
  //       uint8_t *bytes = reinterpret_cast<uint8_t *>(&element);

  //       size_t offset = sizeof(float)*(dim1 * dim2 * i + dim2 * j + k) + 3*sizeof(uint64_t);
  //       bytes[0] = input[offset];
  //       bytes[1] = input[offset + 1];
  //       bytes[2] = input[offset + 2];
  //       bytes[3] = input[offset + 3];

  //       f_output(i, j, k) = element;
  //     }
  //   }
  // }

  nn::Tensor<float, 3> output(std::move(codec::utils::idct(f_output, BLOCK_WIDTH)));
  
  return output;
}

nn::Tensor<float, 3> nnfc::NNFC2Decoder::backward(const nn::Tensor<float, 3> input) const {
  return input;
}
