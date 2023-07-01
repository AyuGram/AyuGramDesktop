/*
MIT License

Copyright (c) 2021 Yanjie He

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace bit_converter {

using std::vector;

/**
 * @brief Convert the specified 16-bit signed integer value to bytes.
 */
template <typename OutputIt>
inline OutputIt i16_to_bytes(int16_t value, bool is_big_endian,
                             OutputIt output_it) {
  if (is_big_endian) {
    *output_it = static_cast<uint8_t>(value >> 8);
    output_it++;
    *output_it = static_cast<uint8_t>(value & 0xFF);
    output_it++;
  } else {
    *output_it = static_cast<uint8_t>(value & 0xFF);
    output_it++;
    *output_it = static_cast<uint8_t>(value >> 8);
    output_it++;
  }
  return output_it;
}

/**
 * @brief Convert the specified 16-bit unsigned integer value to bytes.
 */
template <typename OutputIt>
inline OutputIt u16_to_bytes(uint16_t value, bool is_big_endian,
                             OutputIt output_it) {
  return i16_to_bytes(static_cast<uint16_t>(value), is_big_endian, output_it);
}

/**
 * @brief Convert the specified 32-bit signed integer value to bytes.
 */
template <typename OutputIt>
inline OutputIt i32_to_bytes(int32_t value, bool is_big_endian,
                             OutputIt output_it) {
  if (is_big_endian) {
    for (int i = 0; i < static_cast<int>(sizeof(int32_t)); i++) {
      *output_it = static_cast<uint8_t>((value >> (24 - i * 8)) & 0xFF);
      output_it++;
    }
  } else {
    for (int i = static_cast<int>(sizeof(int32_t)) - 1; i >= 0; i--) {
      *output_it = static_cast<uint8_t>((value >> (24 - i * 8)) & 0xFF);
      output_it++;
    }
  }
  return output_it;
}

/**
 * @brief Convert the specified 32-bit unsigned integer value to bytes.
 */
template <typename OutputIt>
inline OutputIt u32_to_bytes(uint32_t value, bool is_big_endian,
                             OutputIt output_it) {
  return i32_to_bytes(static_cast<int32_t>(value), is_big_endian, output_it);
}

/**
 * @brief Convert the specified 64-bit signed integer value to bytes.
 */
template <typename OutputIt>
inline OutputIt i64_to_bytes(int64_t value, bool is_big_endian,
                             OutputIt output_it) {
  if (is_big_endian) {
    for (int i = 0; i < static_cast<int>(sizeof(int64_t)); i++) {
      *output_it = static_cast<uint8_t>((value >> (56 - i * 8)) & 0xFF);
      output_it++;
    }
  } else {
    for (int i = static_cast<int>(sizeof(int64_t)) - 1; i >= 0; i--) {
      *output_it = static_cast<uint8_t>((value >> (56 - i * 8)) & 0xFF);
      output_it++;
    }
  }
  return output_it;
}

/**
 * @brief Convert the specified 64-bit unsigned integer value to bytes.
 */
template <typename OutputIt>
inline OutputIt u64_to_bytes(uint64_t value, bool is_big_endian,
                             OutputIt output_it) {
  return i64_to_bytes(static_cast<int64_t>(value), is_big_endian, output_it);
}

template <typename OutputIt>
inline OutputIt create_bytes_from_bits(const vector<bool> &bits,
                                       OutputIt output_it) {
  for (int i = 0; i < static_cast<int>(bits.size() / 8); i++) {
    uint8_t b = 0;
    for (int j = 0; j < 8; j++) {
      b = b + (bits[i * 8 + j] << j);
    }
    *output_it = b;
    output_it++;
  }
  return output_it;
}

/**
 * @brief Convert the specified single-precision floating-point value to bytes.
 */
template <typename OutputIt>
inline OutputIt f32_to_bytes(float_t value, bool is_big_endian,
                             OutputIt output_it) {
  vector<bool> bits;
  bits.reserve(sizeof(float_t) * 8);
  bits.push_back(value < 0);

  int exponent;
  float_t mantissa = std::frexp(value, &exponent);
  exponent = (exponent - 1) + 127;

  for (int i = 0; i < 8; i++) {
    bits.push_back(exponent % 2);
    exponent = exponent / 2;
  }
  std::reverse(bits.begin() + 1, bits.begin() + 1 + 8);
  mantissa = mantissa * 2 - 1;
  for (int i = 0; i < 23; i++) {
    mantissa = mantissa * 2;
    if (mantissa >= 1.0) {
      mantissa = mantissa - 1.0;
      bits.push_back(true);
    } else {
      bits.push_back(false);
    }
  }
  if (is_big_endian) {
    return create_bytes_from_bits(bits, output_it);
  } else {
    std::reverse(bits.begin(), bits.end());
    return create_bytes_from_bits(bits, output_it);
  }
}

/**
 * @brief Convert the specified double-precision floating-point value to bytes.
 */
template <typename OutputIt>
inline OutputIt f64_to_bytes(double_t value, bool is_big_endian,
                             OutputIt output_it) {
  vector<bool> bits;
  bits.reserve(sizeof(double_t) * 8);
  bits.push_back(value < 0);

  int exponent;
  double_t mantissa = std::frexp(value, &exponent);
  exponent = (exponent - 1) + 1023;

  for (int i = 0; i < 11; i++) {
    bits.push_back(exponent % 2);
    exponent = exponent / 2;
  }
  std::reverse(bits.begin() + 1, bits.begin() + 1 + 11);
  mantissa = mantissa * 2 - 1;
  for (int i = 0; i < 52; i++) {
    mantissa = mantissa * 2;
    if (mantissa >= 1.0) {
      mantissa = mantissa - 1.0;
      bits.push_back(true);
    } else {
      bits.push_back(false);
    }
  }
  if (is_big_endian) {
    return create_bytes_from_bits(bits, output_it);
  } else {
    std::reverse(bits.begin(), bits.end());
    return create_bytes_from_bits(bits, output_it);
  }
}

/**
 * @brief Returns a 16-bit signed integer converted from two bytes at a
 * specified position in a byte sequence.
 */
template <typename InputIt>
inline int16_t bytes_to_i16(InputIt input_it, bool is_big_endian) {
  if (is_big_endian) {
    int16_t result = *input_it;
    input_it++;
    result = (result << 8) + *input_it;
    input_it++;
    return result;
  } else {
    int16_t result = *input_it;
    input_it++;
    result = result + (static_cast<int16_t>(*input_it) << 8);
    input_it++;
    return result;
  }
}

/**
 * @brief Returns a 16-bit unsigned integer converted from two bytes at a
 * specified position in a byte sequence.
 */
template <typename InputIt>
inline uint16_t bytes_to_u16(InputIt input_it, bool is_big_endian) {
  return static_cast<uint16_t>(bytes_to_i16(input_it, is_big_endian));
}

/**
 * @brief Returns a 32-bit signed integer converted from four bytes at a
 * specified position in a byte sequence.
 */
template <typename InputIt>
inline int32_t bytes_to_i32(InputIt input_it, bool is_big_endian) {
  int32_t result = 0;
  if (is_big_endian) {
    for (int i = 0; i < static_cast<int>(sizeof(int32_t)); i++) {
      result = result + ((*input_it) << (8 * (4 - 1 - i)));
      input_it++;
    }
    return result;
  } else {
    for (int i = 0; i < static_cast<int>(sizeof(int32_t)); i++) {
      result = result + ((*input_it) << (8 * i));
      input_it++;
    }
    return result;
  }
}

/**
 * @brief Returns a 32-bit unsigned integer converted from four bytes at a
 * specified position in a byte sequence.
 */
template <typename InputIt>
inline uint32_t bytes_to_u32(InputIt input_it, bool is_big_endian) {
  return static_cast<uint32_t>(bytes_to_i32(input_it, is_big_endian));
}

/**
 * @brief Returns a 64-bit signed integer converted from eight bytes at a
 * specified position in a byte sequence.
 */
template <typename InputIt>
inline int64_t bytes_to_i64(InputIt input_it, bool is_big_endian) {
  int64_t result = 0;
  if (is_big_endian) {
    for (int i = 0; i < static_cast<int>(sizeof(int64_t)); i++) {
      result = result + (static_cast<int64_t>(*input_it) << (8 * (8 - 1 - i)));
      input_it++;
    }
    return result;
  } else {
    for (int i = 0; i < static_cast<int>(sizeof(int64_t)); i++) {
      result = result + (static_cast<int64_t>(*input_it) << (8 * i));
      input_it++;
    }
    return result;
  }
}

/**
 * @brief Returns a 64-bit unsigned integer converted from eight bytes at a
 * specified position in a byte sequence.
 */
template <typename InputIt>
inline uint64_t bytes_to_u64(InputIt input_it, bool is_big_endian) {
  return static_cast<uint64_t>(input_it, is_big_endian);
}

/**
 * @brief Returns a single-precision floating-point number converted from four
 * bytes at a specified position in a byte sequence.
 */
template <typename InputIt>
inline float_t bytes_to_f32(InputIt input_it, bool is_big_endian) {
  vector<bool> bits;
  bits.reserve(sizeof(float_t) * 8);
  for (int i = 0; i < static_cast<int>(sizeof(float_t)); i++) {
    uint8_t b = *input_it;
    input_it++;
    for (int j = 0; j < 8; j++) {
      bits.push_back(b % 2);
      b = b / 2;
    }
  }
  if (!is_big_endian) {
    std::reverse(bits.begin(), bits.end());
  }
  int sign = bits[0] ? (-1) : (+1);
  int exponent = 0;
  for (int i = 0; i < 8; i++) {
    exponent = exponent + bits[1 + i] * (1 << (8 - 1 - i));
  }
  exponent = exponent - 127;
  float_t mantissa = 1.0;
  float_t cur = 0.5;
  for (int i = 0; i < 23; i++) {
    mantissa = mantissa + bits[1 + 8 + i] * cur;
    cur = cur / 2;
  }
  return sign * std::ldexp(mantissa, exponent);
}

/**
 * @brief Returns a double-precision floating-point number converted from eight
 * bytes at a specified position in a byte sequence.
 */
template <typename InputIt>
inline double_t bytes_to_f64(InputIt input_it, bool is_big_endian) {
  vector<bool> bits;
  bits.reserve(sizeof(double_t) * 8);
  for (int i = 0; i < static_cast<int>(sizeof(double_t)); i++) {
    uint8_t b = *input_it;
    input_it++;
    for (int j = 0; j < 8; j++) {
      bits.push_back(b % 2);
      b = b / 2;
    }
  }
  if (!is_big_endian) {
    std::reverse(bits.begin(), bits.end());
  }
  int sign = bits[0] ? (-1) : (+1);
  int exponent = 0;
  for (int i = 0; i < 11; i++) {
    exponent = exponent + bits[1 + i] * (1 << (11 - 1 - i));
  }
  exponent = exponent - 1023;
  double_t mantissa = 1.0;
  double_t cur = 0.5;
  for (int i = 0; i < 52; i++) {
    mantissa = mantissa + bits[1 + 11 + i] * cur;
    cur = cur / 2;
  }
  return sign * std::ldexp(mantissa, exponent);
}

}; // namespace bit_converter