#pragma once

#include <cstdint>
#include <type_traits>
#ifdef _WIN32
#include <intrin.h>
#define __builtin_clz __lzcnt
#define __builtin_clzll __lzcnt64
static int __builtin_ctz(long int val) {
    int count = 0;
    unsigned long int ulv = static_cast<unsigned long int>(val);
    while (!(ulv & 1) && (count < 32)) {
        ulv >>= 1;
        count++;
    }
    return count;
}

static int __builtin_ctzll(long long int val) {
    int count = 0;
    unsigned long long int ulv = static_cast<unsigned long long int>(val);
    while (!(ulv & 1) && (count < 64)) {
        ulv >>= 1;
        count++;
    }
    return count;
}
#endif

namespace gifproc::util {

// bool is_big_endian() {
//    constexpr union {
//       uint32_t _4byte;
//       uint8_t _byte[4];
//    } v = { 0x01020304 };
//    return v._byte[0] == 1 && v._byte[1] == 2 && v._byte[2] == 3 && v._byte[3] == 4;
// }
// bool is_little_endian() {
//    constexpr union {
//       uint32_t _4byte;
//       uint8_t _byte[4];
//    } v = { 0x01020304 };
//    return v._byte[0] == 4 && v._byte[1] == 3 && v._byte[2] == 2 && v._byte[3] == 1;
// }
constexpr bool is_big_endian() { return false; }
constexpr bool is_little_endian() { return true; }
constexpr std::size_t to_byte(std::size_t bit) { return bit >> 3; }
constexpr std::size_t to_bit(std::size_t bytes) { return bytes << 3; }
constexpr std::size_t bit_align(std::size_t bit) { return bit & ~7; }

template <typename T>
constexpr std::size_t bitsize_v = sizeof(T) << 3;

template <std::size_t _Bits>
struct smallest_uintegral_imp {
   static_assert(_Bits <= 64, "Invalid size for integral type");
};

template <std::size_t _Bits>
struct smallest_uintegral {
   using type = typename smallest_uintegral_imp<bit_align(_Bits + 7)>::type;
};

template <>
struct smallest_uintegral_imp<0> {
   using type = uint8_t;
};

template <>
struct smallest_uintegral_imp<8> {
   using type = uint8_t;
};

template <>
struct smallest_uintegral_imp<16> {
   using type = uint16_t;
};

template <>
struct smallest_uintegral_imp<24> {
   using type = uint32_t;
};

template <>
struct smallest_uintegral_imp<32> {
   using type = uint32_t;
};

template <>
struct smallest_uintegral_imp<40> {
   using type = uint64_t;
};

template <>
struct smallest_uintegral_imp<48> {
   using type = uint64_t;
};

template <>
struct smallest_uintegral_imp<56> {
   using type = uint64_t;
};

template <>
struct smallest_uintegral_imp<64> {
   using type = uint64_t;
};



template <typename T>
T gen_mask(int begin, int end) {
   static_assert(std::is_integral_v<T>, "gen_mask only works on integral types");
   const T mask_left = begin == 0 ? ~(T{0}) : (static_cast<T>(T{1} << (bitsize_v<T> - begin)) - 1);
   const T mask_right = ~(static_cast<T>(T{1} << (bitsize_v<T> - end - 1)) - 1);
   return mask_left & mask_right;
}

template <typename T>
T gen_mask_reverse(int lsb, int msb) {
   static_assert(std::is_integral_v<T>, "gen_mask_reverse only works on integral types");
   const T mask_left = msb == (bitsize_v<T> - 1) ? ~(T{0}) : (static_cast<T>(T{1} << (msb + 1)) - 1);
   const T mask_right = ~(static_cast<T>(T{1} << lsb) - 1);
   return mask_left & mask_right;
}

template <typename T>
struct bitfld {
   static_assert(std::is_integral_v<T>, "bitfld only supports raw integral types");
   static_assert(sizeof(T) <= sizeof(unsigned int) || sizeof(T) == sizeof(unsigned long long), "bitfld only supports "
         "integral types sized less than unsigned int, or equal to unsigned long long");
   using T_u = typename std::make_unsigned_t<T>;

   T_u _mask;
   T_u _value;

   constexpr bitfld(T val, T mask)
         : _mask(static_cast<T_u>(mask)), _value(static_cast<T_u>(val) & _mask) {}
   constexpr bitfld(T val, int lsb, int msb)
         : _mask(static_cast<T_u>(gen_mask_reverse<T>(lsb, msb))), _value(static_cast<T_u>(val) & _mask) {}
   constexpr bitfld() : _mask(T_u{0}), _value(T_u{0}) {}

   constexpr int mask_len() const {
      if constexpr (sizeof(T) <= sizeof(unsigned int)) {
         return bitsize_v<unsigned int> -
                __builtin_clz(static_cast<unsigned int>(_mask)) -
                __builtin_ctz(static_cast<unsigned int>(_mask));
      } else {  // sizeof(T) == sizeof(unsigned long long)
         return bitsize_v<unsigned long long> -
                __builtin_clzll(static_cast<unsigned long long>(_mask)) -
                __builtin_ctzll(static_cast<unsigned long long>(_mask));
      }
   }

   constexpr bitfld<T> trim_mask_right(int new_len) const {
      if constexpr (sizeof(T) <= sizeof(unsigned int)) {
         constexpr int kExtraBits = bitsize_v<unsigned int> - bitsize_v<T>;
         const int leading_bits = __builtin_clz(static_cast<unsigned int>(_mask)) - kExtraBits;
         const T maskoff_right = ~((T{1} << (bitsize_v<T> - (leading_bits + new_len))) - 1);
         return bitfld<T>(static_cast<T>(_value), static_cast<T>(_mask & maskoff_right));
      } else {  // sizeof(T) == sizeof(unsigned long long)
         const int leading_bits = __builtin_clzll(static_cast<unsigned long long>(_mask));
         const T maskoff_right = ~((T{1} << (bitsize_v<T> - (leading_bits + new_len))) - 1);
         return bitfld<T>(static_cast<T>(_value), static_cast<T>(_mask & maskoff_right));
      }
   }
   
   constexpr bitfld<T> extract_to_lsb() const {
      if constexpr (sizeof(T) <= sizeof(unsigned int)) {
         const int rsh_amt = __builtin_ctz(static_cast<unsigned int>(_mask));
         return bitfld<T>(static_cast<T>(_value >> rsh_amt), static_cast<T>(_mask >> rsh_amt));
      } else {  // sizeof(T) == sizeof(unsigned long long)
         const int rsh_amt = __builtin_ctzll(static_cast<unsigned long long>(_mask));
         return bitfld<T>(static_cast<T>(_value >> rsh_amt), static_cast<T>(_mask >> rsh_amt));
      }
   }

   constexpr bitfld<T> pack_to_position(int from_lsb) const {
      const int from_msb = bitsize_v<T> - from_lsb - 1;
      if constexpr (sizeof(T) <= sizeof(unsigned int)) {
         constexpr int kExtraBits = bitsize_v<unsigned int> - bitsize_v<T>;
         const int leading_bits = __builtin_clz(static_cast<unsigned int>(_mask)) - kExtraBits;
         if (leading_bits < from_msb) {
            return bitfld<T>(static_cast<T>(_value >> (from_msb - leading_bits)),
                             static_cast<T>(_mask >> (from_msb - leading_bits)));
         }
         return bitfld<T>(static_cast<T>(_value << (leading_bits - from_msb)),
                          static_cast<T>(_mask << (leading_bits - from_msb)));
      } else {  // sizeof(T) == sizeof(unsigned long long)
         const int leading_bits = __builtin_clzll(static_cast<unsigned long long>(_mask));
         if (leading_bits < from_msb) {
            return bitfld<T>(static_cast<T>(_value >> (from_msb - leading_bits)),
                             static_cast<T>(_mask >> (from_msb - leading_bits)));
         }
         return bitfld<T>(static_cast<T>(_value << (leading_bits - from_msb)),
                          static_cast<T>(_mask << (leading_bits - from_msb)));
      }
   }
};

template <typename T>
constexpr bitfld<T> create_nbits(T value, int num_bits) {
   return bitfld<T>(value, 0, num_bits - 1);
}

template <typename T>
constexpr int min_bitsize(T value) {
   constexpr auto leading_t = [](auto t) -> int {
      if constexpr (sizeof(T) <= sizeof(unsigned int)) {
         constexpr int kExtraBits = bitsize_v<unsigned int> - bitsize_v<T>;
         return __builtin_clz(static_cast<unsigned int>(t - 1)) - kExtraBits;
      } else {  // sizeof(T) == sizeof(unsigned long long)
         return __builtin_clzll(static_cast<unsigned long long>(t - 1));
      }
   };
   if (value == 0) {
      return 1;
   } else {
      return bitsize_v<T> - leading_t(value);
   }
}

}
