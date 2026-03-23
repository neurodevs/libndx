#include "ndx/ble_state_provider.hpp"
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

@interface CBStateDelegate : NSObject <CBCentralManagerDelegate>
@end

@implementation CBStateDelegate
- (void)centralManagerDidUpdateState:(CBCentralManager*)central {}
@end

namespace ndx {

class CoreBluetoothStateProvider : public BleStateProvider {
public:
  CoreBluetoothStateProvider() {
    delegate_ = [[CBStateDelegate alloc] init];
    manager_ = [[CBCentralManager alloc] initWithDelegate:delegate_ queue:nil];
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
  }

  bool isPoweredOn() override {
    return manager_.state == CBManagerStatePoweredOn;
  }

private:
  CBStateDelegate* delegate_;
  CBCentralManager* manager_;
};

std::unique_ptr<BleStateProvider> createBleStateProvider() {
  return std::make_unique<CoreBluetoothStateProvider>();
}

}
