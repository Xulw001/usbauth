#include "bloomfilter.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <memory.h>
#endif
#include <math.h>

#include <filesystem>
#include <fstream>

#include "usbauth.h"

namespace tool {

const char *kUsbAuthDir = ".usb";
const char *kBloomFilterPath = ".usb/bf.bin";
const char *kTimeMarkPath = ".usb/tm.mk";

#ifndef USB_BYTES
#define BITS 8
#define GetBit(arr, i) (arr[i / BITS] & (1 << (i % BITS)))
#define SetBit(arr, i) arr[i / BITS] = (arr[i / BITS] | (1 << (i % BITS)))
#else
#define BITS 1
#define GetBit(arr, i) (arr[i])
#define SetBit(arr, i) arr[i] = 0x01
#endif

int BloomFilter::LoadBloomFilter(const std::string &device) {
  try {
    std::string bf_path = device + kBloomFilterPath;
    std::ifstream file;
    file.open(bf_path.c_str(), std::ios_base::in | std::ios_base::binary);
    file.read((char *)&config_, sizeof(config_));

    union endian {
      int val;
      char flg;
    } p = {0x01};
    if (p.flg) {
      swap8(config_.magic);
      swap4(config_.max_item);
      swap8(config_.value);
      swap4(config_.bits_count);
      swap4(config_.hash_count);
    }

    bits_.reset(new char[config_.bits_count / BITS]);
    file.read(bits_.get(), config_.bits_count / BITS);
    file.close();

    if (config_.magic != kBfMagic) {
      return error_code::kSignFileErr;
    }
  } catch (std::ifstream::failure &e) {
    return error_code::kIOFailure;
  }
  return error_code::kSuccess;
}

int BloomFilter::SaveBloomFilter(const std::string &device) {
  Config cf(config_);
  union endian {
    int val;
    char flg;
  } p = {0x01};
  if (p.flg) {
    swap8(cf.magic);
    swap4(cf.max_item);
    swap8(cf.value);
    swap4(cf.bits_count);
    swap4(cf.hash_count);
  }

  try {
    std::string dir = device + kUsbAuthDir;
    if (!std::filesystem::exists(dir)) {
      std::filesystem::create_directory(dir);
    }

    std::string bf_path = device + kBloomFilterPath;
    std::ofstream file;
    file.open(bf_path.c_str(), std::ios_base::out | std::ios_base::binary);

    file.write((char *)&cf, sizeof(cf));
    file.write(bits_.get(), config_.bits_count / BITS);
    file.close();
  } catch (std::ifstream::failure &e) {
    return error_code::kIOFailure;
  }
  return error_code::kSuccess;
}

void BloomFilter::InitBloomFilter(uint32_t max_items, double mistake_rate) {
  int bitarrs = 0;
  bitarrs = ceil(-1.00 * max_items * log(mistake_rate) / 0.480453);
  bitarrs = (bitarrs + 31) / 32 * 32;  // align for int
  config_.magic = kBfMagic;
  config_.max_item = max_items;
  config_.mistake_rate = mistake_rate;
  config_.hash_count = floor(0.693147 * bitarrs / max_items);
  config_.bits_count = bitarrs;
  bits_.reset(new char[config_.bits_count / BITS]);
  memset(bits_.get(), 0x00, config_.bits_count / BITS);
  return;
}

#ifdef _WIN32
#define Debug(fmt, ...)            \
  char buff[1024] = {0};           \
  sprintf(buff, fmt, __VA_ARGS__); \
  OutputDebugStringA(buff);
#else
#define Debug(fmt, ...)
#endif

bool BloomFilter::BloomAdd(const std::string &data) {
  for (uint32_t i = 0; i < config_.hash_count; i++) {
    uint64_t hash = Fnv1a(data.c_str(), data.length(), i);
    uint64_t index = hash % config_.bits_count;
    SetBit(bits_, index);
    Debug("%s --- idx = %lld --- arr = %d\n", data.c_str(), index,
          (int)bits_[index / 8]);
  }
  return true;
}

bool BloomFilter::BloomCheck(const std::string &data) {
  for (uint32_t i = 0; i < config_.hash_count; i++) {
    uint64_t hash = Fnv1a(data.c_str(), data.length(), i);
    uint64_t index = hash % config_.bits_count;
    Debug("%s --- idx = %lld --- arr = %d\n", data.c_str(), index,
          (int)bits_[index / 8]);
    if (GetBit(bits_, index) == 0x00) {
      return false;
    }
  }

  return true;
}

uint64_t BloomFilter::Fnv1a(const void *data, size_t numBytes, int seed) {
  uint64_t hash = 0xcbf29ce484222325ull;
  const uint8_t *bytes = static_cast<const uint8_t *>(data);
  for (size_t i = 0; i < numBytes; ++i) {
    hash ^= bytes[i];
    hash *= 0x100000001b3ull;
  }

  uint8_t seed_arr[32] = {0};
#ifdef _WIN32
  numBytes = sprintf_s((char *)seed_arr, sizeof(seed_arr) - 1, "%d", seed);
#else
  numBytes = snprintf((char *)seed_arr, sizeof(seed_arr) - 1, "%d", seed);
#endif
  for (size_t i = 0; i < numBytes; ++i) {
    hash ^= seed_arr[i];
    hash *= 0x100000001b3ull;
  }

  return hash;
}

};  // namespace tool