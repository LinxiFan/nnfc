#ifndef _CODEC_ARITHMETIC_CODER_HH
#define _CODEC_ARITHMETIC_CODER_HH

#include <cassert>
#include <memory>
#include <vector>

namespace codec {

static constexpr uint64_t num_working_bits_ = 31;
static_assert(num_working_bits_ < 63);

static constexpr uint64_t max_range_ = static_cast<uint64_t>(1)
                                       << num_working_bits_;
static constexpr uint64_t min_range_ = (max_range_ >> 2) + 2;
static constexpr uint64_t max_ =
    max_range_ - 1;  // 0xFFFFFFFF for 32 working bits
static constexpr uint64_t min_ = 0;

static constexpr uint64_t top_mask_ = static_cast<uint64_t>(1)
                                      << (num_working_bits_ - 1);
static constexpr uint64_t second_mask_ = static_cast<uint64_t>(1)
                                         << (num_working_bits_ - 2);
static constexpr uint64_t mask_ = max_;

class SimpleModel {
 private:
  const std::vector<std::pair<uint32_t, uint32_t>> numerator;
  const uint32_t denominator;

 public:
  SimpleModel()
      : numerator({
            {0, 11000}, {11000, 11999}, {11999, 12000},
        }),
        denominator(numerator[numerator.size() - 1].second) {}
  ~SimpleModel() {}

  inline std::pair<uint32_t, uint32_t> symbol_numerator(uint32_t symbol) const {
    assert(symbol <= 2);
    return numerator[symbol];
  }

  inline uint32_t symbol_denominator() const { return denominator; }

  inline uint32_t size() { return 3; }
};

// A helper class that gives you a std::vector like interface but
// for individual bits.
class InfiniteBitVector {
 private:
  std::vector<char> bitvector_;
  size_t number_of_bits_;

 public:
  InfiniteBitVector() : bitvector_(), number_of_bits_(0) {}

  InfiniteBitVector(std::vector<char> bitvector)
      : bitvector_(bitvector), number_of_bits_(8 * bitvector.size()) {}

  ~InfiniteBitVector() {}

  void push_back_bit(const uint8_t bit) {
    if (bit != 0 and bit != 1) {
      throw std::runtime_error("bit must be 0 or 1");
    }

    const int bit_offset = number_of_bits_ % 8;
    if (bit_offset == 0) {
      bitvector_.push_back(0);
    }
    const size_t byte_offset = bitvector_.size() - 1;

    bitvector_[byte_offset] |= (bit << bit_offset);

    ++number_of_bits_;
  }

  uint8_t get_bit(const size_t bit_idx) const {
    const int bit_offset = bit_idx % 8;
    const size_t byte_offset = bit_idx / 8;

    if (byte_offset >= bitvector_.size()) {
      throw std::runtime_error("bit index out of range");
    }

    return (bitvector_[byte_offset] & (1 << bit_offset)) >> bit_offset;
  }

  size_t size() const { return number_of_bits_; }

  std::vector<char> vector() const { return bitvector_; }

  void print() const {
    for (size_t i = 0; i < this->size(); i++) {
      std::cout << static_cast<int>(this->get_bit(i));
    }
    std::cout << std::endl;
  }
};

template <class ProbabilityModel>
class ArithmeticEncoder {
 private:
  ProbabilityModel model_;
  InfiniteBitVector data_;

  uint64_t high_;
  uint64_t low_;
  uint64_t pending_bits_;
  bool finished_;

  inline void shift() {
    // grab the MSB of `low` (will be the same as `high`)
    const uint8_t bit = (low_ >> (num_working_bits_ - 1));
    assert(bit <= 0x1);
    assert(bit == (high_ >> (num_working_bits_ - 1)));
    assert((!!(high_ & top_mask_)) == bit);

    data_.push_back_bit(bit);

    // the pending bits will be the opposite of the
    // shifted bit.
    for (; pending_bits_ > 0; pending_bits_--) {
      data_.push_back_bit(bit ^ 0x1);
    }
    assert(pending_bits_ == 0);
  }

  inline void underflow() {
    assert(pending_bits_ < std::numeric_limits<decltype(pending_bits_)>::max());
    pending_bits_ += 1;
  }

 public:
  ArithmeticEncoder()
      : model_(),
        data_(),
        high_(max_),
        low_(min_),
        pending_bits_(0),
        finished_(false) {}

  ~ArithmeticEncoder() {}

  void encode_symbol(const uint32_t symbol) {
    if (finished_) {
      throw std::runtime_error(
          "`finished` already called, cannot encode more symbols.");
    }

    const uint64_t range = high_ - low_ + 1;
    assert(range <= max_range_);
    assert(range >= min_range_);

    const std::pair<uint64_t, uint64_t> sym_prob =
        model_.symbol_numerator(symbol);
    const uint64_t sym_high = sym_prob.second;
    const uint64_t sym_low = sym_prob.first;
    const uint32_t denominator = model_.symbol_denominator();
    assert(sym_high > sym_low);

    // check if overflow would happen
    assert((range >= 1) or
           (sym_high < (std::numeric_limits<uint64_t>::max() / range)));
    assert((range >= 1) or
           (sym_low < (std::numeric_limits<uint64_t>::max() / range)));

    assert((denominator >= 1) and
           (low_ < (std::numeric_limits<uint64_t>::max() -
                    (range * sym_high) / denominator) +
                       1));
    assert((denominator >= 1) and
           (low_ < (std::numeric_limits<uint64_t>::max() -
                    (range * sym_low) / denominator)));

    const uint64_t new_high = low_ + (sym_high * range) / denominator - 1;
    const uint64_t new_low = low_ + (sym_low * range) / denominator;

    assert(new_high <= max_);
    assert(new_low <= max_);
    assert(new_high == (mask_ & new_high));
    assert(new_low == (mask_ & new_low));

    high_ = new_high;
    low_ = new_low;
    assert(high_ > low_);

    while (true) {
      // if the MSB of both numbers match, then shift out a bit
      // into the vector and shift out all `pending bits`.
      if (((high_ ^ low_) & top_mask_) == 0) {
        shift();

        low_ = (low_ << 1) & mask_;
        high_ = ((high_ << 1) & mask_) | 0x1;

        assert(high_ <= max_);
        assert(low_ <= max_);
        assert(high_ > low_);
      }
      // the second highest bit of `high` is a 1 and the second
      // highest bit of `low` is 0, then the `low` and `high`
      // are converging. To handle this, we increment
      // `pending_bits` and shift `high` and `low`. The value
      // true value of the shifted bits will be determined once
      // the MSB bits match after consuming more symbols.
      else if ((low_ & ~high_ & second_mask_) != 0) {
        underflow();

        low_ = (low_ << 1) & (mask_ >> 1);
        high_ = ((high_ << 1) & (mask_ >> 1)) | top_mask_ | 1;

        assert(high_ <= max_);
        assert(low_ <= max_);
        assert(high_ > low_);
      } else {
        break;
      }
    }
  }

  std::vector<char> finish() {
    if (finished_) {
      throw std::runtime_error(
          "`finished` already called, cannot encode more symbols.");
    }

    encode_symbol(2);
    finished_ = true;

    data_.push_back_bit(0x1);

    return data_.vector();
  }
};

template <class ProbabilityModel>
class ArithmeticDecoder {
 private:
  ProbabilityModel model_;
  InfiniteBitVector data_;

  uint64_t high_;
  uint64_t low_;
  uint64_t value_;

  size_t bit_idx_;
  bool done_;

  inline void shift() {
    // grab the MSB of `low` (will be the same as `high`)
    const uint8_t bit = (low_ >> (num_working_bits_ - 1));
    assert(bit <= 0x1);
    assert(bit == (high_ >> (num_working_bits_ - 1)));
    assert((!!(high_ & top_mask_)) == bit);

    assert(((value_ & top_mask_) >> (num_working_bits_ - 1)) == bit);

    uint8_t bitvector_bit = 0;
    if (bit_idx_ < data_.size()) {
      bitvector_bit = static_cast<uint64_t>(data_.get_bit(bit_idx_)) & 0x1;
    }
    value_ = ((value_ << 1) & mask_) | bitvector_bit;
    assert(value_ <= max_);
    bit_idx_++;
  }

  inline void underflow() {
    uint64_t bitvector_bit = 0;
    if (bit_idx_ < data_.size()) {
      bitvector_bit = static_cast<uint64_t>(data_.get_bit(bit_idx_)) & 0x1;
    }
    value_ =
        (value_ & top_mask_) | ((value_ << 1) & (mask_ >> 1)) | bitvector_bit;
    bit_idx_++;
    assert(value_ <= max_);
  }

 public:
  ArithmeticDecoder(std::vector<char> data)
      : model_(),
        data_(data),
        high_(max_),
        low_(min_),
        value_(0),
        bit_idx_(0),
        done_(false)

  {
    for (size_t i = 0; i < num_working_bits_ and i < data_.size(); i++) {
      value_ |= (static_cast<uint64_t>(data_.get_bit(i))
                 << (num_working_bits_ - i - 1));
      bit_idx_++;
    }
    assert(value_ <= max_);
  }

  ~ArithmeticDecoder() {}

  uint32_t decode_symbol() {
    if (done_) {
      throw std::runtime_error("done decoding input already.");
    }

    const uint64_t range = high_ - low_ + 1;
    assert(range <= max_range_);
    assert(range >= min_range_);

    bool sym_set = false;
    uint32_t sym = -1;
    for (uint64_t sym_idx = 0; sym_idx < model_.size(); sym_idx++) {
      const std::pair<uint64_t, uint64_t> sym_prob =
          model_.symbol_numerator(sym_idx);
      const uint32_t denominator = model_.symbol_denominator();
      assert(sym_prob.second > sym_prob.first);

      // check if overflow would happen
      assert((range >= 1) or (sym_prob.second <
                              (std::numeric_limits<uint64_t>::max() / range)));
      assert((range >= 1) or
             (sym_prob.first < (std::numeric_limits<uint64_t>::max() / range)));

      assert((denominator >= 1) and
             (low_ < (std::numeric_limits<uint64_t>::max() -
                      (range * sym_prob.second) / denominator) +
                         1));
      assert((denominator >= 1) and
             (low_ < (std::numeric_limits<uint64_t>::max() -
                      (range * sym_prob.first) / denominator)));

      const uint64_t sym_high =
          low_ + (sym_prob.second * range) / denominator - 1;
      const uint64_t sym_low = low_ + (sym_prob.first * range) / denominator;

      assert(sym_high <= max_);
      assert(sym_low <= max_);
      assert(sym_high == (mask_ & sym_high));
      assert(sym_low == (mask_ & sym_low));

      if (value_ < sym_high and value_ >= sym_low) {
        sym = sym_idx;
        high_ = sym_high;
        low_ = sym_low;
        sym_set = true;
        break;
      }
    }
    assert(sym_set);
    assert(sym < std::numeric_limits<uint64_t>::max());
    assert(high_ > low_);
    const uint32_t symbol = sym;

    if (symbol == 2) {
      done_ = true;
      return symbol;
    }

    while (true) {
      // if the MSB of both numbers match, then shift out a bit
      // into the vector and shift out all `pending bits`.
      if (((high_ ^ low_) & top_mask_) == 0) {
        shift();

        low_ = (low_ << 1) & mask_;
        high_ = ((high_ << 1) & mask_) | 0x1;

        assert(high_ <= max_);
        assert(low_ <= max_);
        assert(high_ > low_);
      }
      // the second highest bit of `high` is a 1 and the second
      // highest bit of `low` is 0, then the `low` and `high`
      // are converging. To handle this, we increment
      // `pending_bits` and shift `high` and `low`. The value
      // true value of the shifted bits will be determined once
      // the MSB bits match after consuming more symbols.
      else if ((low_ & ~high_ & second_mask_) != 0) {
        underflow();

        low_ = (low_ << 1) & (mask_ >> 1);
        high_ = ((high_ << 1) & (mask_ >> 1)) | top_mask_ | 1;

        assert(high_ <= max_);
        assert(low_ <= max_);
        assert(high_ > low_);
      } else {
        break;
      }
    }

    return symbol;
  }

  inline bool done() const { return done_; }
};
}

#endif  // _CODEC_ARITHMETIC_CODER_HH