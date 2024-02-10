#ifndef __BLOOM_FILTER_H
#define __BLOOM_FILTER_H

#include <memory>
#include <string>

namespace tool {
extern const char *kBloomFilterPath;
extern const char *kTimeMarkPath;
extern const char *kUsbAuthDir;

#define swap4(value)                                                    \
  value = (((value >> 24) & 0x000000ff) | ((value >> 8) & 0x0000ff00) | \
           ((value << 8) & 0x00ff0000) | ((value << 24) & 0xff000000))

#define swap8(value)                                 \
  value = (((value >> 56) & 0x00000000000000FFULL) | \
           ((value >> 40) & 0x000000000000FF00ULL) | \
           ((value >> 24) & 0x0000000000FF0000ULL) | \
           ((value >> 8) & 0x00000000FF000000ULL) |  \
           ((value << 8) & 0x000000FF00000000ULL) |  \
           ((value << 24) & 0x0000FF0000000000ULL) | \
           ((value << 40) & 0x00FF000000000000ULL) | \
           ((value << 56) & 0xFF00000000000000ULL))

const uint32_t kBfMaxItems = 1000 * 1000;
const double kBfMistakeRate = 0.00001;
const uint64_t kBfMagic = 0x5245544C49464D42U;

class BloomFilter {
#pragma pack(push, 1)
  struct Config {
    uint64_t magic;
    uint32_t max_item;
    union {
      uint64_t value;
      double mistake_rate;
    };
    uint32_t bits_count;
    uint32_t hash_count;
  };
#pragma pack(pop)

 public:
  void InitBloomFilter(uint32_t max_items = kBfMaxItems,
                       double mistake_rate = kBfMistakeRate);
  int SaveBloomFilter(const std::string &device);
  int LoadBloomFilter(const std::string &device);
  bool BloomAdd(const std::string &data);
  bool BloomCheck(const std::string &data);

 private:
  uint64_t Fnv1a(const void *data, size_t numBytes, int seed);

 private:
  std::unique_ptr<char[]> bits_;
  Config config_;
};
};  // namespace tool

#endif