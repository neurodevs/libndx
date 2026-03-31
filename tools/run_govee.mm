#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

static NSString* const DEVICE_ID = @"179F4A82-A2DF-C241-DB2A-1DF990779106";

// ----------------------------------------------------------------------------
// Delegate
// ----------------------------------------------------------------------------
@interface GoveeDelegate : NSObject <CBCentralManagerDelegate>
@property (nonatomic, strong) CBCentralManager* manager;
@property (nonatomic, assign) double  tempC;
@property (nonatomic, assign) double  humidity;
@property (nonatomic, assign) int     battery;
@property (nonatomic, assign) BOOL    hasData;
@end

@implementation GoveeDelegate

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
  if (central.state == CBManagerStatePoweredOn) {
    NSLog(@"scanning for %@...", DEVICE_ID);
    // AllowDuplicates so we keep receiving advertisement updates
    NSDictionary* opts = @{CBCentralManagerScanOptionAllowDuplicatesKey: @YES};
    [central scanForPeripheralsWithServices:nil options:opts];
  }
}

- (void)centralManager:(CBCentralManager*)central
 didDiscoverPeripheral:(CBPeripheral*)peripheral
     advertisementData:(NSDictionary*)advertisementData
                  RSSI:(NSNumber*)RSSI {
  if (![peripheral.identifier.UUIDString isEqualToString:DEVICE_ID]) return;

  // Govee H5074 manufacturer data layout (little-endian):
  //   [0-1] manufacturer ID (0x88, 0xEC)
  //   [2]   padding (0x00)
  //   [3-4] temperature * 100  (signed int16 LE, °C)
  //   [5-6] humidity * 100     (uint16 LE, %)
  //   [7]   battery percentage
  NSData* mfgData = advertisementData[CBAdvertisementDataManufacturerDataKey];
  if (!mfgData || mfgData.length < 8) return;

  const uint8_t* d = (const uint8_t*)mfgData.bytes;
  // Ignore Apple iBeacon packets (manufacturer ID 0x004C) — the device also
  // broadcasts an INTELLI_ROCKS iBeacon which shares the same peripheral UUID
  if (d[0] != 0x88 || d[1] != 0xEC) return;

  double newTemp     = (int16_t)((uint16_t)d[3] | ((uint16_t)d[4] << 8)) / 100.0;
  double newHumidity = ((uint16_t)d[5] | ((uint16_t)d[6] << 8)) / 100.0;
  int    newBattery  = d[7];

  if (self.hasData &&
      newTemp     == self.tempC &&
      newHumidity == self.humidity &&
      newBattery  == self.battery) return;

  self.tempC    = newTemp;
  self.humidity = newHumidity;
  self.battery  = newBattery;
  self.hasData  = YES;

  double tempF = self.tempC * 9.0 / 5.0 + 32.0;
  printf("temp: %.2f°C / %.2f°F   humidity: %.1f%%   battery: %d%%\n",
         self.tempC, tempF, self.humidity, self.battery);
}

@end

int main() {
  GoveeDelegate* delegate = [[GoveeDelegate alloc] init];
  CBCentralManager* manager = [[CBCentralManager alloc] initWithDelegate:delegate queue:nil];
  delegate.manager = manager;

  [[NSRunLoop currentRunLoop] run];
  return 0;
}
