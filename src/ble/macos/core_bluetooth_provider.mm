#include "ndx/ble_provider.hpp"
#include <lsl_c.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

namespace ndx { class CoreBluetoothProvider; }

@interface CoreBluetoothDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
- (instancetype)initWithProvider:(ndx::CoreBluetoothProvider*)provider;
@property (nonatomic, strong) CBPeripheral* peripheral;
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

  void scanAll(int duration_ms, ScanResultCallback on_complete) override {}

  void scanForAdvertisement(const std::string& id, OnDataCallback on_advertisement_data) {
    advertisement_target_id_ = [NSString stringWithUTF8String:id.c_str()];
    on_advertisement_data_ = on_advertisement_data;
    NSDictionary* opts = @{CBCentralManagerScanOptionAllowDuplicatesKey: @YES};
    [manager_ scanForPeripheralsWithServices:nil options:opts];
  }

  void onDiscoveredAdvertisement(CBPeripheral* peripheral, NSDictionary* advertisementData) {
    if (!advertisement_target_id_) return;
    if (![peripheral.identifier.UUIDString isEqualToString:advertisement_target_id_]) return;

    uint64_t timestamp_ms = static_cast<uint64_t>(lsl_local_clock() * 1000.0);

    NSData* mfgData = advertisementData[CBAdvertisementDataManufacturerDataKey];
    if (!mfgData || mfgData.length < 4) return;

    Packet packet;
    packet.timestamp_ms = timestamp_ms;
    const uint32_t* words = static_cast<const uint32_t*>(mfgData.bytes);
    packet.data.assign(words, words + mfgData.length / sizeof(uint32_t));
    on_advertisement_data_(packet);
  }

  void scanForPeripheral(const std::string& id, OnDataCallback on_data) override {
    peripheral_target_id_ = [NSString stringWithUTF8String:id.c_str()];
    on_peripheral_data_ = on_data;
    [manager_ scanForPeripheralsWithServices:nil options:advertisementScanOptions()];
  }

  int getRssi() override { return rssi_; }

  void onRssi(int rssi) { rssi_ = rssi; }

  void onDiscoveredPeripheral(CBPeripheral* peripheral) {
    if (!peripheral_target_id_) return;
    if (![peripheral.identifier.UUIDString isEqualToString:peripheral_target_id_]) return;

    NSLog(@"discovered: %@ name: %@", peripheral.identifier.UUIDString, peripheral.name);
    delegate_.peripheral = peripheral;
    peripheral_target_id_ = nil;
    [manager_ stopScan];
    [manager_ connectPeripheral:peripheral options:nil];

    // Restart scan if advertisement mode is still active
    if (advertisement_target_id_) {
      NSDictionary* opts = @{CBCentralManagerScanOptionAllowDuplicatesKey: @YES};
      [manager_ scanForPeripheralsWithServices:nil options:opts];
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
    on_peripheral_data_(packet);
  }

private:
  // Returns allowDuplicates:YES if advertisement mode is active, so both
  // modes can coexist on the single CBCentralManager scan.
  NSDictionary* advertisementScanOptions() {
    if (advertisement_target_id_)
      return @{CBCentralManagerScanOptionAllowDuplicatesKey: @YES};
    return nil;
  }

  CoreBluetoothDelegate* delegate_;
  CBCentralManager* manager_;
  NSString* peripheral_target_id_ = nil;
  NSString* advertisement_target_id_ = nil;
  OnDataCallback on_peripheral_data_;
  OnDataCallback on_advertisement_data_;
  int rssi_ = 0;
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
  _provider->onRssi(RSSI.intValue);
  _provider->onDiscoveredPeripheral(peripheral);
  _provider->onDiscoveredAdvertisement(peripheral, advertisementData);
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
