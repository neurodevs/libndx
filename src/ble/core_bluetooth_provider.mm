#include "ndx/ble_provider.hpp"
#include <lsl_c.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

namespace ndx { class CoreBluetoothProvider; }

@interface CoreBluetoothDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
- (instancetype)initWithProvider:(ndx::CoreBluetoothProvider*)provider;
@end

namespace ndx {

class CoreBluetoothProvider : public BleProvider {
public:
  CoreBluetoothProvider() {
    delegate_ = [[CoreBluetoothDelegate alloc] initWithProvider:this];
    manager_ = [[CBCentralManager alloc] initWithDelegate:delegate_ queue:nil];
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
  }

  bool isPoweredOn() override {
    return manager_.state == CBManagerStatePoweredOn;
  }

  void scanForPeripheral(const std::string& id, OnDataCallback on_data) override {
    target_id_ = [NSString stringWithUTF8String:id.c_str()];
    on_data_ = on_data;
    [manager_ scanForPeripheralsWithServices:nil options:nil];
  }

  void onDiscoveredPeripheral(CBPeripheral* peripheral) {
    if ([peripheral.identifier.UUIDString isEqualToString:target_id_]) {
      peripheral_ = peripheral;
      [manager_ stopScan];
      [manager_ connectPeripheral:peripheral_ options:nil];
    }
  }

  void onConnected(CBPeripheral* peripheral) {
    peripheral.delegate = delegate_;
    [peripheral discoverServices:nil];
  }

  void onDiscoveredServices(CBPeripheral* peripheral) {
    for (CBService* service in peripheral.services) {
      [peripheral discoverCharacteristics:nil forService:service];
    }
  }

  void onDiscoveredCharacteristics(CBPeripheral* peripheral, CBService* service) {
    for (CBCharacteristic* characteristic in service.characteristics) {
      if (characteristic.properties & CBCharacteristicPropertyNotify) {
        [peripheral setNotifyValue:YES forCharacteristic:characteristic];
      }
    }
  }

  void onCharacteristicValue(CBCharacteristic* characteristic) {
    uint64_t timestamp_ms = static_cast<uint64_t>(lsl_local_clock() * 1000.0);
    NSData* data = characteristic.value;
    Packet packet;
    packet.timestamp_ms = timestamp_ms;
    const uint32_t* bytes = static_cast<const uint32_t*>(data.bytes);
    packet.data.assign(bytes, bytes + data.length / sizeof(uint32_t));
    on_data_(packet);
  }

private:
  CoreBluetoothDelegate* delegate_;
  CBCentralManager* manager_;
  CBPeripheral* peripheral_;
  NSString* target_id_;
  OnDataCallback on_data_;
};

std::unique_ptr<BleProvider> createBleProvider() {
  return std::make_unique<CoreBluetoothProvider>();
}

}

@implementation CoreBluetoothDelegate {
  ndx::CoreBluetoothProvider* _provider;
}

- (instancetype)initWithProvider:(ndx::CoreBluetoothProvider*)provider {
  self = [super init];
  _provider = provider;
  return self;
}

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {}

- (void)centralManager:(CBCentralManager*)central
 didDiscoverPeripheral:(CBPeripheral*)peripheral
     advertisementData:(NSDictionary*)advertisementData
                  RSSI:(NSNumber*)RSSI {
  _provider->onDiscoveredPeripheral(peripheral);
}

- (void)centralManager:(CBCentralManager*)central
  didConnectPeripheral:(CBPeripheral*)peripheral {
  _provider->onConnected(peripheral);
}

- (void)peripheral:(CBPeripheral*)peripheral
didDiscoverServices:(NSError*)error {
  _provider->onDiscoveredServices(peripheral);
}

- (void)peripheral:(CBPeripheral*)peripheral
didDiscoverCharacteristicsForService:(CBService*)service
             error:(NSError*)error {
  _provider->onDiscoveredCharacteristics(peripheral, service);
}

- (void)peripheral:(CBPeripheral*)peripheral
didUpdateValueForCharacteristic:(CBCharacteristic*)characteristic
             error:(NSError*)error {
  _provider->onCharacteristicValue(characteristic);
}

@end
