#include "filefilter.h"

#include "usbauth.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <string.h>
#endif
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
namespace tm_ = std::chrono;
namespace tool {
// the elapse time from file clock to system_clock
auto elapse =
    tm_::duration_cast<tm_::seconds>(
        std::filesystem::file_time_type::clock::now().time_since_epoch() -
        tm_::system_clock::now().time_since_epoch())
        .count();

int FileFilter::Scan(const std::string &driver_path) {
  time_t time_diff = 0;
  int err = GetTimeOffset(driver_path, &time_diff);
  if (err != error_code::kSuccess) {
    return err;
  }

  err = bf_.LoadBloomFilter(driver_path);
  if (err != error_code::kSuccess) {
    return err;
  }

  std::stringstream ss;
  try {
    for (fs::recursive_directory_iterator it(driver_path);
         it != fs::recursive_directory_iterator(); it++) {
      std::string path = it->path().generic_u8string().substr(
          driver_path.size(), std::string::npos);
      if (strcmp(kBloomFilterPath, path.c_str()) == 0 ||
          strcmp(kTimeMarkPath, path.c_str()) == 0 ||
          strcmp(kUsbAuthDir, path.c_str()) == 0)
        continue;

      if (!fs::is_directory(it->path()) && !fs::is_regular_file(it->path()))
        continue;

      ss.str("");
      ss << path;
      auto time = tm_::duration_cast<tm_::seconds>(
                      it->last_write_time().time_since_epoch())
                      .count();
      ss << (time_diff + time);
      size_t file_size = 0x1000;
      if (fs::is_regular_file(it->path())) {
        file_size = it->file_size();
      }
      ss << file_size;

      if (!bf_.BloomCheck(ss.str())) {
        return error_code::kScanErr;
      }
    }
  } catch (fs::filesystem_error &e) {
    return error_code::kIOFailure;
  }
  return error_code::kSuccess;
}

int FileFilter::Auth(const std::string &driver_path) {
  std::stringstream ss;
  try {
    bf_.InitBloomFilter();
    for (fs::recursive_directory_iterator it(driver_path);
         it != fs::recursive_directory_iterator(); it++) {
      std::string path = it->path().generic_u8string().substr(
          driver_path.size(), std::string::npos);
      if (strcmp(kBloomFilterPath, path.c_str()) == 0 ||
          strcmp(kTimeMarkPath, path.c_str()) == 0 ||
          strcmp(kUsbAuthDir, path.c_str()) == 0)
        continue;

      if (!fs::is_directory(it->path()) && !fs::is_regular_file(it->path()))
        continue;

      ss.str("");
      ss << path;
      auto time = tm_::duration_cast<tm_::seconds>(
                      it->last_write_time().time_since_epoch())
                      .count();
      ss << (time - elapse);
      size_t file_size = 0x1000;
      if (fs::is_regular_file(it->path())) {
        file_size = it->file_size();
      }
      ss << file_size;

      if (!bf_.BloomAdd(ss.str())) {
        return error_code::kIOFailure;
      }
    }
  } catch (fs::filesystem_error &e) {
    return error_code::kIOFailure;
  }

  int err = bf_.SaveBloomFilter(driver_path);
  if (err != error_code::kSuccess) {
    return err;
  }

  return SetTimeOffset(driver_path);
}

const uint64_t kTmMagic = 0x5245544C49464D43U;
int FileFilter::GetTimeOffset(const std::string &driver_path, time_t *mk) {
  try {
    struct {
      uint64_t magic;
      time_t base_time;
    } bin;
    std::ifstream file;
    std::string time_mk_path = driver_path + kTimeMarkPath;
    file.open(time_mk_path.c_str(), std::ios_base::in | std::ios_base::binary);
    file.read((char *)&bin, sizeof(bin));
    file.close();

    union endian {
      int val;
      char flg;
    } p = {0x01};
    if (p.flg) {
      swap8(bin.magic);
      swap8(bin.base_time);
    }
    if (bin.magic != kTmMagic) return error_code::kSignFileErr;

    std::error_code err;
    std::string bf_path = driver_path + kBloomFilterPath;
    auto time = tm_::duration_cast<tm_::seconds>(
                    fs::last_write_time(bf_path, err).time_since_epoch())
                    .count();
    if (err) return error_code::kIOFailure;

    *mk = bin.base_time - time;  // Get System Time Diff between auth and scan
  } catch (std::ifstream::failure &e) {
    return error_code::kIOFailure;
  }

  return error_code::kSuccess;
}

int FileFilter::SetTimeOffset(const std::string &driver_path) {
  try {
    std::error_code err;
    std::string bf_path = driver_path + kBloomFilterPath;
    auto time = tm_::duration_cast<tm_::seconds>(
                    fs::last_write_time(bf_path, err).time_since_epoch())
                    .count();
    if (err) return error_code::kIOFailure;

    struct {
      uint64_t magic;
      time_t base_time;
    } bin = {kTmMagic, time - elapse};
    union endian {
      int val;
      char flg;
    } p = {0x01};
    if (p.flg) {
      swap8(bin.magic);
      swap8(bin.base_time);
    }

    std::ofstream file;
    std::string time_mk_path = driver_path + kTimeMarkPath;
    file.open(time_mk_path.c_str(), std::ios_base::out | std::ios_base::binary);
    file.write((char *)&bin, sizeof(bin));
    file.close();
  } catch (std::ifstream::failure &e) {
    return error_code::kIOFailure;
  }

  return error_code::kSuccess;
}

}  // namespace tool