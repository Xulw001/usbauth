#ifndef __USB_AUTH_H
#define __USB_AUTH_H
#include <string>

namespace error_code {
enum {
  kIOFailure = -1,
  kSuccess = 0,
  kOptionErr = 1,
  kUsbDeviceErr,
  kSignFileErr,
  kScanErr,
};
}

namespace usb {

class UsbTool {
 public:
  int Run(int argc, char* argvs[]);

 private:
  bool Prase(int argc, char* argvs[]);
  int Auth(bool again = false);
  int Scan();

 private:
  bool IsStandardUsb();
  bool IsUsbAuth();

 private:
  std::string driver_;
  int mode_;
};
};  // namespace usb

#endif