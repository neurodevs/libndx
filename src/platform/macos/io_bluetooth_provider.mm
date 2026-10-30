#import <Foundation/Foundation.h>
#import <IOBluetooth/IOBluetooth.h>

#include <lsl_c.h>

#include <atomic>
#include <cstdio>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

#include "ndx/bluetooth_provider.hpp"

@interface NdxRfcommDelegate : NSObject <IOBluetoothRFCOMMChannelDelegate>
- (instancetype)initWithOnData:(ndx::OnDataCallback)on_data;
@end

@implementation NdxRfcommDelegate {
  ndx::OnDataCallback _on_data;
}

- (instancetype)initWithOnData:(ndx::OnDataCallback)on_data {
  self = [super init];
  if (self) _on_data = std::move(on_data);
  return self;
}

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)channel
                     data:(void*)dataPointer
                   length:(size_t)dataLength {
  if (!_on_data) return;

  ndx::Packet packet;
  packet.timestamp_sec = lsl_local_clock();

  const uint8_t* bytes = static_cast<const uint8_t*>(dataPointer);
  packet.data.assign(bytes, bytes + dataLength);

  _on_data(packet);
}

@end

namespace ndx {

std::function<std::vector<Device>()> BluetoothProviderSyscalls::list_paired_devices = []() {
  std::vector<Device> devices;

  @autoreleasepool {
    for (IOBluetoothDevice* device in [IOBluetoothDevice pairedDevices]) {
      devices.push_back({(device.addressString ?: @"").UTF8String, (device.name ?: @"").UTF8String});
    }
  }

  return devices;
};

namespace {

std::string describe_io_return(IOReturn result) {
  char buf[16];
  snprintf(buf, sizeof(buf), "0x%08X", result);
  return buf;
}

// Each provider opens its channel and writes on a thread of its own, whose run
// loop keeps spinning until disconnect. Incoming data, however, always arrives
// on the main run loop: see pump_bluetooth_events.
class IOBluetoothProvider : public BluetoothProvider {
public:
  ~IOBluetoothProvider() override { disconnect(); }

  bool is_powered_on() override {
    return [IOBluetoothHostController defaultController].powerState == kBluetoothHCIPowerStateON;
  }

  void connect(const std::string& address, int port, OnDataCallback on_data,
               OnConnectedCallback on_connected) override {
    std::promise<std::string> opened;
    auto open_error = opened.get_future();

    running_ = true;

    thread_ = std::thread([this, address, port, on_data = std::move(on_data), &opened]() mutable {
      @autoreleasepool {
        run_loop_ = CFRunLoopGetCurrent();
        [[NSRunLoop currentRunLoop] addPort:[NSMachPort port] forMode:NSDefaultRunLoopMode];

        const std::string error = open_channel(address, port, std::move(on_data));
        opened.set_value(error);

        if (!error.empty()) return;

        while (running_) {
          @autoreleasepool {
            [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                     beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
          }
        }

        close_channel();
      }
    });

    const std::string error = open_error.get();

    if (!error.empty()) {
      disconnect();
      throw std::runtime_error("BluetoothProvider: " + error);
    }

    if (on_connected) {
      Device device{address, name_};
      on_connected(&device);
    }
  }

  void disconnect() override {
    if (!thread_.joinable()) return;

    running_ = false;
    if (run_loop_) CFRunLoopWakeUp(run_loop_);

    thread_.join();
    run_loop_ = nullptr;
  }

  bool write(const uint8_t* data, size_t len) override {
    if (!running_ || !run_loop_) return false;

    std::vector<uint8_t> bytes(data, data + len);
    std::vector<uint8_t>* bytes_ptr = &bytes;

    __block IOReturn result = kIOReturnNotOpen;
    dispatch_semaphore_t done = dispatch_semaphore_create(0);

    CFRunLoopPerformBlock(run_loop_, kCFRunLoopDefaultMode, ^{
      if (channel_) {
        result = [channel_ writeSync:bytes_ptr->data() length:static_cast<UInt16>(bytes_ptr->size())];
      }
      dispatch_semaphore_signal(done);
    });
    CFRunLoopWakeUp(run_loop_);

    const long timed_out = dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
    return timed_out == 0 && result == kIOReturnSuccess;
  }

private:
  std::string open_channel(const std::string& address, int port, OnDataCallback on_data) {
    IOBluetoothDevice* device = [IOBluetoothDevice deviceWithAddressString:@(address.c_str())];

    if (!device) {
      return "no bluetooth device with address " + address;
    }

    // Open the baseband connection first: opening the channel without one
    // silently stalls.
    IOReturn result = [device openConnection];

    if (result != kIOReturnSuccess) {
      return "could not connect to " + address + " (" + describe_io_return(result) + ")";
    }

    delegate_ = [[NdxRfcommDelegate alloc] initWithOnData:std::move(on_data)];

    IOBluetoothRFCOMMChannel* channel = nil;
    result = [device openRFCOMMChannelSync:&channel
                             withChannelID:static_cast<BluetoothRFCOMMChannelID>(port)
                                  delegate:delegate_];

    if (result != kIOReturnSuccess) {
      [device closeConnection];
      delegate_ = nil;
      return "could not open rfcomm channel " + std::to_string(port) + " on " + address +
             " (" + describe_io_return(result) + ")";
    }

    device_ = device;
    channel_ = channel;
    name_ = device.name ? device.name.UTF8String : address;

    return "";
  }

  void close_channel() {
    [channel_ setDelegate:nil];
    [channel_ closeChannel];
    [device_ closeConnection];

    channel_ = nil;
    device_ = nil;
    delegate_ = nil;
  }

  std::atomic<bool> running_{false};
  std::thread thread_;
  CFRunLoopRef run_loop_ = nullptr;

  IOBluetoothDevice* device_ = nil;
  IOBluetoothRFCOMMChannel* channel_ = nil;
  NdxRfcommDelegate* delegate_ = nil;
  std::string name_;
};

}

void pump_bluetooth_events() {
  @autoreleasepool {
    while (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true) == kCFRunLoopRunHandledSource) {
    }
  }
}

std::unique_ptr<BluetoothProvider> create_bluetooth_provider() {
  return std::make_unique<IOBluetoothProvider>();
}

}
