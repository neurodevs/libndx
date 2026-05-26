#include "ndx/ble_provider.hpp"

using OnDataCallback = std::function<void(const ndx::Packet&)>;
#include <lsl_c.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#include <unordered_map>
#include <string>

namespace ndx { class CoreBluetoothProvider; }

@interface CoreBluetoothDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
- (instancetype)initWithProvider:(ndx::CoreBluetoothProvider*)provider;
@property (nonatomic, strong) CBPeripheral* peripheral;
@end

namespace ndx {

class CoreBluetoothProvider : public BleProvider {
public:
  CoreBluetoothProvider() {
    state_sem_ = dispatch_semaphore_create(0);
    delegate_ = [[CoreBluetoothDelegate alloc] initWithProvider:this];
    dispatch_queue_t queue = dispatch_queue_create("com.ndx.bluetooth", DISPATCH_QUEUE_SERIAL);
    manager_ = [[CBCentralManager alloc] initWithDelegate:delegate_ queue:queue];
    dispatch_semaphore_wait(state_sem_, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
    state_sem_ = nil;
  }

  bool is_powered_on() override {
    return manager_.state == CBManagerStatePoweredOn;
  }

  void scan_all(int duration_ms, ScanResultCallback on_complete) override {}

  void scanForAdvertisement(const std::string& uuid, OnDataCallback on_advertisement_data) {
    advertisement_target_id_ = [NSString stringWithUTF8String:uuid.c_str()];
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

  void scan_for_peripheral(const std::string& uuid, CharCallbacks callbacks) override {
    peripheral_target_id_ = [NSString stringWithUTF8String:uuid.c_str()];
    for (auto& entry : callbacks)
      char_callbacks_[entry.char_uuid] = std::move(entry.on_data);
    [manager_ scanForPeripheralsWithServices:nil options:advertisementScanOptions()];
  }

  int read_rssi() override {
    if (delegate_.peripheral)
      [delegate_.peripheral readRSSI];
    return rssi_;
  }

  void onStateUpdated() {
    if (state_sem_) dispatch_semaphore_signal(state_sem_);
  }

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

  void onConnectedPeripheral(CBPeripheral* peripheral) {
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
      std::string uuid = characteristic.UUID.UUIDString.UTF8String;
      characteristics_[uuid] = characteristic;
      if (characteristic.properties & CBCharacteristicPropertyNotify) {
        [peripheral setNotifyValue:YES forCharacteristic:characteristic];
      }
    }
  }

  void write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len) override {
    auto it = characteristics_.find(char_uuid);
    if (it == characteristics_.end()) return;
    NSData* nsdata = [NSData dataWithBytes:data length:len];
    [delegate_.peripheral writeValue:nsdata
                   forCharacteristic:it->second
                                type:CBCharacteristicWriteWithoutResponse];
  }

  void onCharacteristicValue(CBCharacteristic* characteristic) {
    std::string uuid = characteristic.UUID.UUIDString.UTF8String;
    auto it = char_callbacks_.find(uuid);
    if (it == char_callbacks_.end()) return;

    uint64_t timestamp_ms = static_cast<uint64_t>(lsl_local_clock() * 1000.0);
    NSData* data = characteristic.value;
    Packet packet;
    packet.timestamp_ms = timestamp_ms;
    const uint32_t* bytes = static_cast<const uint32_t*>(data.bytes);
    packet.data.assign(bytes, bytes + data.length);
    it->second(packet);
  }

  void disconnect_peripheral(const std::string& uuid) override {
    if (!delegate_.peripheral) return;
    if (![delegate_.peripheral.identifier.UUIDString isEqualToString:[NSString stringWithUTF8String:uuid.c_str()]]) return;
    [manager_ cancelPeripheralConnection:delegate_.peripheral];
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
  dispatch_semaphore_t state_sem_ = nil;
  NSString* peripheral_target_id_ = nil;
  NSString* advertisement_target_id_ = nil;
  std::unordered_map<std::string, std::function<void(const Packet&)>> char_callbacks_;
  OnDataCallback on_advertisement_data_;
  int rssi_ = 0;
  std::unordered_map<std::string, CBCharacteristic*> characteristics_;
};

std::unique_ptr<BleProvider> create_ble_provider() {
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

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
  if (central.state != CBManagerStateUnknown &&
      central.state != CBManagerStateResetting) {
    _provider->onStateUpdated();
  }
}

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
  _provider->onConnectedPeripheral(peripheral);
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

- (void)peripheral:(CBPeripheral*)peripheral
      didReadRSSI:(NSNumber*)RSSI
            error:(NSError*)error {
  if (!error)
    _provider->onRssi(RSSI.intValue);
}

@end
