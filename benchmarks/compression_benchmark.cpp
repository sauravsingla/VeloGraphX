#include "velographx/storage/compressed_adjacency.hpp"
#include "velographx/storage/compressed_decode_simd.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main() {
  using clock = std::chrono::steady_clock;
  using namespace velographx::storage;

  struct Family { const char* name; std::uint32_t gap; };
  const std::vector<Family> families{{"dense", 1}, {"medium", 16}, {"sparse", 4096}};
  constexpr std::size_t values = 100000;
  constexpr int repeat = 5;

  std::cout << "family,codec,values,encoded_bytes,ratio,encode_us,decode_us\n";

  for (const auto& family : families) {
    std::vector<VertexId> ids;
    ids.reserve(values);
    VertexId current = 0;
    for (std::size_t i = 0; i < values; ++i) {
      current += family.gap + static_cast<std::uint32_t>(i % 7 == 0);
      ids.push_back(current);
    }

    {
      long long encode_us = 0, decode_us = 0;
      std::vector<std::uint8_t> encoded;
      std::vector<VertexId> decoded;
      for (int r = 0; r < repeat; ++r) {
        auto t0 = clock::now();
        encoded = variable_byte_delta_encode(ids);
        auto t1 = clock::now();
        decoded = variable_byte_delta_decode(encoded);
        auto t2 = clock::now();
        encode_us += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        decode_us += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
      }
      if (decoded != ids) return 2;
      std::cout << family.name << ",varbyte," << values << ',' << encoded.size() << ','
                << compression_ratio_bytes(ids, encoded) << ',' << encode_us / repeat << ','
                << decode_us / repeat << '\n';
    }

    {
      long long encode_us = 0, decode_us = 0;
      BlockedVariableByte encoded;
      std::vector<VertexId> decoded;
      for (int r = 0; r < repeat; ++r) {
        auto t0 = clock::now();
        encoded = blocked_variable_byte_encode(ids, 128);
        auto t1 = clock::now();
        decoded = blocked_variable_byte_decode(encoded);
        auto t2 = clock::now();
        encode_us += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        decode_us += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
      }
      if (decoded != ids) return 3;
      std::cout << family.name << ",blocked-varbyte," << values << ',' << encoded.payload.size() << ','
                << compression_ratio_bytes(ids, encoded) << ',' << encode_us / repeat << ','
                << decode_us / repeat << '\n';
    }

    {
      long long encode_us = 0, scalar_us = 0, vector_us = 0;
      SimdFriendlyDelta encoded;
      std::vector<VertexId> decoded;
      for (int r = 0; r < repeat; ++r) {
        auto t0 = clock::now();
        encoded = simd_friendly_delta_encode(ids, 128);
        auto t1 = clock::now();
        decoded = simd_friendly_delta_decode(encoded);
        auto t2 = clock::now();
        auto decoded_vector = simd_friendly_delta_decode_vectorized(encoded);
        auto t3 = clock::now();
        if (decoded_vector != ids) return 4;
        encode_us += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        scalar_us += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        vector_us += std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();
      }
      if (decoded != ids) return 5;
      std::cout << family.name << ",fixed-width-scalar," << values << ',' << encoded.payload.size() << ','
                << compression_ratio_bytes(ids, encoded) << ',' << encode_us / repeat << ','
                << scalar_us / repeat << '\n';
      std::cout << family.name << ",fixed-width-vectorized," << values << ',' << encoded.payload.size() << ','
                << compression_ratio_bytes(ids, encoded) << ',' << encode_us / repeat << ','
                << vector_us / repeat << '\n';
    }
  }
  return 0;
}
