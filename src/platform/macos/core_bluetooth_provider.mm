#include "ndx/acquisition_backend.hpp"
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
    ble_queue_ = dispatch_queue_create("com.ndx.bluetooth", DISPATCH_QUEUE_SERIAL);
    manager_ = [[CBCentralManager alloc] initWithDelegate:delegate_ queue:ble_queue_];
    dispatch_semaphore_wait(state_sem_, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
    state_sem_ = nil;
  }

  ~CoreBluetoothProvider() {
    CBCentralManager* manager = manager_;
    dispatch_sync(ble_queue_, ^{
      manager.delegate = nil;
      [manager stopScan];
    });
  }

  bool is_powered_on() override {
    return manager_.state == CBManagerStatePoweredOn;
  }

  void discover_ble_uuid(const std::string& name_prefix, std::function<void(const std::string&)> on_discovered) override {
    NSString* prefix_ns = [NSString stringWithUTF8String:name_prefix.c_str()];
    __block auto cb = std::move(on_discovered);
    dispatch_async(ble_queue_, ^{
      discover_prefix_ = prefix_ns;
      on_uuid_discovered_ = std::move(cb);
      if (manager_.state == CBManagerStatePoweredOn) {
        [manager_ scanForPeripheralsWithServices:nil options:nil];
      }
    });
  }

  void start_ble_scan(const std::string& name_prefix,
                      std::function<void(const std::string&, const std::string&)> on_scanned) override {
    NSString* prefix_ns = [NSString stringWithUTF8String:name_prefix.c_str()];
    __block auto cb = std::move(on_scanned);
    dispatch_async(ble_queue_, ^{
      scan_prefix_ = prefix_ns;
      on_scanned_ = std::move(cb);
      if (manager_.state == CBManagerStatePoweredOn) {
        [manager_ scanForPeripheralsWithServices:nil options:scanOptions()];
      }
    });
  }

  static NSDictionary* scanOptions() {
    return @{CBCentralManagerScanOptionAllowDuplicatesKey: @YES};
  }

  void stop_ble_scan() override {
    dispatch_async(ble_queue_, ^{
      [manager_ stopScan];
      scan_prefix_ = nil;
      on_scanned_ = nullptr;
    });
  }

  void onScannedPeripheral(CBPeripheral* peripheral, NSDictionary* advertisementData) {
    if (!scan_prefix_ || !on_scanned_) return;

    NSString* name = advertisementData[CBAdvertisementDataLocalNameKey] ?: peripheral.name ?: @"";
    if (![name hasPrefix:scan_prefix_]) return;

    on_scanned_(peripheral.identifier.UUIDString.UTF8String, name.UTF8String);
  }

  void start_advertisement_listen(const std::string& uuid, ndx::OnAdvertisementCallback on_advertisement_data) override {
    NSString* uuid_ns = [NSString stringWithUTF8String:uuid.c_str()];
    __block auto cb = std::move(on_advertisement_data);
    dispatch_async(ble_queue_, ^{
      advertisement_target_id_ = uuid_ns;
      on_advertisement_data_ = std::move(cb);
      [manager_ scanForPeripheralsWithServices:nil options:advertisementScanOptions()];
    });
  }

  void stop_advertisement_listen() override {
    dispatch_async(ble_queue_, ^{
      [manager_ stopScan];
      advertisement_target_id_ = nil;
      on_advertisement_data_ = nullptr;
    });
  }

  void onDiscoveredAdvertisement(CBPeripheral* peripheral, NSDictionary* advertisementData, NSNumber* rssi) {
    if (!advertisement_target_id_) return;
    if (![peripheral.identifier.UUIDString isEqualToString:advertisement_target_id_]) return;

    Advertisement advertisement;
    advertisement.timestamp_sec = lsl_local_clock();

    NSString* local_name = advertisementData[CBAdvertisementDataLocalNameKey];
    if (local_name) advertisement.local_name = local_name.UTF8String;

    NSData* mfgData = advertisementData[CBAdvertisementDataManufacturerDataKey];
    if (mfgData) {
      const uint8_t* bytes = static_cast<const uint8_t*>(mfgData.bytes);
      advertisement.manufacturer_data.assign(bytes, bytes + mfgData.length);
      if (mfgData.length >= 2) {
        advertisement.company_id = static_cast<uint16_t>(bytes[0]) |
                                   static_cast<uint16_t>(bytes[1] << 8);
      }
    }

    for (CBUUID* uuid in advertisementData[CBAdvertisementDataServiceUUIDsKey]) {
      advertisement.service_uuids.push_back(uuid.UUIDString.UTF8String);
    }

    NSDictionary<CBUUID*, NSData*>* service_data = advertisementData[CBAdvertisementDataServiceDataKey];
    for (CBUUID* uuid in service_data) {
      NSData* value = service_data[uuid];
      const uint8_t* bytes = static_cast<const uint8_t*>(value.bytes);
      advertisement.service_data.push_back({uuid.UUIDString.UTF8String,
                                            std::vector<uint8_t>(bytes, bytes + value.length)});
    }

    if (rssi) advertisement.rssi = rssi.intValue;

    NSNumber* tx_power = advertisementData[CBAdvertisementDataTxPowerLevelKey];
    if (tx_power) advertisement.tx_power_level = tx_power.intValue;

    NSNumber* connectable = advertisementData[CBAdvertisementDataIsConnectable];
    advertisement.is_connectable = connectable ? connectable.boolValue : false;

    on_advertisement_data_(advertisement);
  }

  void scan_for_peripheral(const std::string& uuid, CharCallbacks callbacks, ndx::OnConnectedCallback on_connected,
                           ndx::OnDisconnectedCallback on_disconnected) override {
    NSString* uuid_ns = [NSString stringWithUTF8String:uuid.c_str()];
    __block auto cbs = std::move(callbacks);
    __block auto on_conn = std::move(on_connected);
    __block auto on_disconn = std::move(on_disconnected);
    dispatch_async(ble_queue_, ^{
      peripheral_target_id_ = uuid_ns;
      on_connected_ = std::move(on_conn);
      on_disconnected_ = std::move(on_disconn);
      for (auto& entry : cbs)
        char_callbacks_[entry.char_uuid] = std::move(entry.on_data);
      [manager_ scanForPeripheralsWithServices:nil options:advertisementScanOptions()];
    });
  }

  void add_char_callbacks(CharCallbacks callbacks) override {
    __block auto cbs = std::move(callbacks);
    dispatch_async(ble_queue_, ^{
      for (auto& entry : cbs)
        char_callbacks_[entry.char_uuid] = std::move(entry.on_data);
    });
  }

  int read_rssi() override {
    CBPeripheral* peripheral = delegate_.peripheral;
    dispatch_async(ble_queue_, ^{
      [peripheral readRSSI];
    });
    return rssi_;
  }

  void set_rssi_interval(int interval_ms, std::function<void(int)> on_rssi) override {
    on_rssi_ = std::move(on_rssi);
    rssi_timer_active_ = true;
    uint64_t generation = ++rssi_generation_;
    schedule_rssi_read(interval_ms, generation);
  }

  void stop_rssi_interval() override {
    rssi_timer_active_ = false;
    ++rssi_generation_;
  }

  void schedule_rssi_read(int interval_ms, uint64_t generation) {
    if (!rssi_timer_active_ || generation != rssi_generation_) return;
    dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(interval_ms) * NSEC_PER_MSEC),
      ble_queue_,
      ^{
        if (!rssi_timer_active_ || generation != rssi_generation_) return;
        if (delegate_.peripheral)
          [delegate_.peripheral readRSSI];
        schedule_rssi_read(interval_ms, generation);
      }
    );
  }

  void onStateUpdated() {
    if (state_sem_) dispatch_semaphore_signal(state_sem_);
    if (manager_.state == CBManagerStatePoweredOn && (discover_prefix_ || scan_prefix_)) {
      [manager_ scanForPeripheralsWithServices:nil options:scan_prefix_ ? scanOptions() : nil];
    }
  }

  void onRssi(int rssi) {
    rssi_ = rssi;
    if (on_rssi_) on_rssi_(rssi);
  }

  void onDiscoveredPeripheral(CBPeripheral* peripheral) {
    if (discover_prefix_) {
      NSString* name = peripheral.name ?: @"";
      if ([name hasPrefix:discover_prefix_]) {
        [manager_ stopScan];
        discover_prefix_ = nil;
        NSString* uuid_ns = peripheral.identifier.UUIDString;
        if (on_uuid_discovered_) {
          on_uuid_discovered_(uuid_ns.UTF8String);
          on_uuid_discovered_ = nullptr;
        }
      }
      return;
    }
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
    num_services_to_discover_ = peripheral.services.count;
    if (num_services_to_discover_ == 0) reportConnected(peripheral);

    for (CBService* service in peripheral.services) {
      [peripheral discoverCharacteristics:nil forService:service];
    }
  }

  // Reports each connection, including each reconnection, only once every
  // characteristic is discovered, so writes made right away are not dropped.
  void reportConnected(CBPeripheral* peripheral) {
    if (!on_connected_) return;
    Device p{
      .id = peripheral.identifier.UUIDString.UTF8String,
      .name = peripheral.name ? peripheral.name.UTF8String : ""
    };
    on_connected_(&p);
  }

  void onDiscoveredCharacteristics(CBPeripheral* peripheral, CBService* service) {
    for (CBCharacteristic* characteristic in service.characteristics) {
      std::string uuid = characteristic.UUID.UUIDString.UTF8String;
      characteristics_[uuid] = characteristic;
      if (characteristic.properties & CBCharacteristicPropertyNotify) {
        [peripheral setNotifyValue:YES forCharacteristic:characteristic];
      }
    }
    if (num_services_to_discover_ > 0 && --num_services_to_discover_ == 0) reportConnected(peripheral);
  }

  void write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len) override {
    auto it = characteristics_.find(char_uuid);
    if (it == characteristics_.end()) return;
    NSData* nsdata = [NSData dataWithBytes:data length:len];
    CBCharacteristic* characteristic = it->second;
    CBPeripheral* peripheral = delegate_.peripheral;
    dispatch_async(ble_queue_, ^{
      [peripheral writeValue:nsdata
           forCharacteristic:characteristic
                        type:CBCharacteristicWriteWithoutResponse];
    });
  }

  void onCharacteristicValue(CBCharacteristic* characteristic) {
    std::string uuid = characteristic.UUID.UUIDString.UTF8String;
    auto it = char_callbacks_.find(uuid);
    if (it == char_callbacks_.end()) return;

    NSData* data = characteristic.value;
    Packet packet;
    packet.timestamp_sec = lsl_local_clock();
    const uint8_t* bytes = static_cast<const uint8_t*>(data.bytes);
    packet.data.assign(bytes, bytes + data.length);
    it->second(packet);
  }

  // CoreBluetooth keeps the connection pending, with no timeout, until the
  // peripheral comes back in range or the connection is cancelled.
  void reconnect_peripheral(const std::string& uuid) override {
    NSString* uuid_ns = [NSString stringWithUTF8String:uuid.c_str()];
    dispatch_async(ble_queue_, ^{
      CBPeripheral* peripheral = delegate_.peripheral;
      if (![peripheral.identifier.UUIDString isEqualToString:uuid_ns]) return;
      [manager_ connectPeripheral:peripheral options:nil];
    });
  }

  void disconnect_peripheral(const std::string& uuid) override {
    if (!delegate_.peripheral) return;
    if (![delegate_.peripheral.identifier.UUIDString isEqualToString:[NSString stringWithUTF8String:uuid.c_str()]]) return;
    CBPeripheral* peripheral = delegate_.peripheral;
    CBCentralManager* manager = manager_;
    is_disconnecting_on_purpose_ = true;
    dispatch_async(ble_queue_, ^{
      [manager cancelPeripheralConnection:peripheral];
    });
  }

  void onDisconnectedPeripheral(CBPeripheral* peripheral) {
    if (delegate_.peripheral != peripheral) return;

    bool intentional = is_disconnecting_on_purpose_;
    is_disconnecting_on_purpose_ = false;

    // Keeps the peripheral after an unexpected disconnect, so it can be
    // reconnected, and its pending connection cancelled on stop.
    if (intentional) delegate_.peripheral = nil;
    characteristics_.clear();
    num_services_to_discover_ = 0;
    rssi_timer_active_ = false;
    ++rssi_generation_;

    if (on_disconnected_) on_disconnected_(intentional);
  }

private:
  NSDictionary* advertisementScanOptions() {
    if (advertisement_target_id_)
      return @{CBCentralManagerScanOptionAllowDuplicatesKey: @YES};
    return nil;
  }

  NSString* discover_prefix_ = nil;
  std::function<void(const std::string&)> on_uuid_discovered_;
  NSString* scan_prefix_ = nil;
  std::function<void(const std::string&, const std::string&)> on_scanned_;
  CoreBluetoothDelegate* delegate_;
  CBCentralManager* manager_;
  dispatch_queue_t ble_queue_ = nil;
  dispatch_semaphore_t state_sem_ = nil;
  NSString* peripheral_target_id_ = nil;
  NSString* advertisement_target_id_ = nil;
  std::unordered_map<std::string, std::function<void(const Packet&)>> char_callbacks_;
  OnAdvertisementCallback on_advertisement_data_;
  ndx::OnConnectedCallback on_connected_;
  ndx::OnDisconnectedCallback on_disconnected_;
  bool is_disconnecting_on_purpose_ = false;
  NSUInteger num_services_to_discover_ = 0;
  int rssi_ = 0;
  bool rssi_timer_active_ = false;
  uint64_t rssi_generation_ = 0;
  std::function<void(int)> on_rssi_;
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
  _provider->onScannedPeripheral(peripheral, advertisementData);
  _provider->onDiscoveredPeripheral(peripheral);
  _provider->onDiscoveredAdvertisement(peripheral, advertisementData, RSSI);
}

- (void)centralManager:(CBCentralManager*)central
  didConnectPeripheral:(CBPeripheral*)peripheral {
  _provider->onConnectedPeripheral(peripheral);
}

- (void)centralManager:(CBCentralManager*)central
didDisconnectPeripheral:(CBPeripheral*)peripheral
                  error:(NSError*)error {
  _provider->onDisconnectedPeripheral(peripheral);
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
