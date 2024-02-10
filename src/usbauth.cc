#include "usbauth.h"

#include "filefilter.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <string.h>

#include <fstream>
#include <sstream>
#endif
#include <filesystem>
namespace fs = std::filesystem;
#include <functional>

namespace usb {

bool UsbTool::IsStandardUsb() {
#ifdef _WIN32
  if (driver_[0] != 'A' && driver_[0] != 'B' &&
      GetDriveTypeA(driver_.c_str()) == DRIVE_REMOVABLE) {
    if (GetVolumeInformationA(driver_.c_str(), NULL, NULL, NULL, NULL, NULL,
                              NULL, 0)) {
      return true;
    }
  }
#else
  std::ifstream mounts("/proc/mounts");
  if (mounts.is_open()) {
    std::string line;
    while (std::getline(mounts, line)) {
      std::stringstream ss(line);
      std::string device, mount_point;
      ss >> device >> mount_point;
      if (driver_.substr(0, driver_.length() - 1) == mount_point) {
        if (0 == strncmp("/dev/sd", device.c_str(), 7)) {
          ss.str("");
          ss << "/sys/block/";
          for (size_t i = 5; i <= device.length(); ++i) {  //
            if ('0' <= device[i] && '9' >= device[i]) {
              break;
            }
            ss << device[i];
          }

          std::error_code err;
          std::string dev_pci_name =
              fs::read_symlink(ss.str(), err).generic_string();
          if (dev_pci_name.find("usb") != std::string::npos) {
            return true;
          }
        }
        return false;
      }
    }
  }
#endif
  return false;
}

bool UsbTool::IsUsbAuth() {
  const std::string bf_path = driver_ + tool::kBloomFilterPath;
  if (!fs::exists(bf_path)) {
    return false;
  }
  const std::string mk_path = driver_ + tool::kTimeMarkPath;
  if (!fs::exists(mk_path)) {
    return false;
  }
  return true;
}

bool UsbTool::Prase(int argc, char* argvs[]) {
  if (argc != 3) {
    return false;
  }

  const auto option = argvs[1];
  if (strncmp(option, "-authagain", 10) == 0) {
    mode_ = 1;
  } else if (strncmp(option, "-auth", 5) == 0) {
    mode_ = 0;
  } else if (strncmp(option, "-scan", 5) == 0) {
    mode_ = 2;
  } else {
    return false;
  }

  driver_ = argvs[2];
#ifdef _WIN32
  if (driver_.length() < 2 || driver_[1] != ':') {
    return false;
  }
  if (driver_.length() > 2) {
    driver_ = driver_.substr(0, 2);
  }
  driver_ += '/';
#else
  if (driver_[driver_.length() - 1] != '/') {
    driver_ += '/';
  }
#endif
  return true;
}

int UsbTool::Auth(bool again) {
  if (again && !IsUsbAuth()) return error_code::kSignFileErr;
  if (!IsStandardUsb()) return error_code::kUsbDeviceErr;
  tool::FileFilter ff;
  return ff.Auth(driver_);
}

int UsbTool::Scan() {
  if (!IsUsbAuth()) return error_code::kSignFileErr;
  if (!IsStandardUsb()) return error_code::kUsbDeviceErr;
  tool::FileFilter ff;
  return ff.Scan(driver_);
}

int UsbTool::Run(int argc, char* argvs[]) {
  if (!Prase(argc, argvs)) {
    return error_code::kOptionErr;
  }

  switch (mode_) {
    case 0:
      return Auth();
    case 1:
      return Auth(true);
    case 2:
      return Scan();
    default:
      return error_code::kOptionErr;
  }
  return error_code::kOptionErr;
}
}  // namespace usb

int main(int argc, char* argvs[]) {
  usb::UsbTool tool;
  return tool.Run(argc, argvs);
}