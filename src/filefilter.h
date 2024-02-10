#ifndef __FILE_FILTER_H
#define __FILE_FILTER_H
#include <string>

#include "bloomfilter.h"

namespace tool {
struct FileSign {
  std::string path;
  time_t mtime;
  size_t size;
};

class FileFilter {
 public:
  int Scan(const std::string &driver_path);
  int Auth(const std::string &driver_path);

 private:
  int GetTimeOffset(const std::string &driver_path, time_t *mk);
  int SetTimeOffset(const std::string &driver_path);

 private:
  BloomFilter bf_;
};

};  // namespace tool

#endif