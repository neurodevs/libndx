#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#include <cstdio>

@interface ScanDelegate : NSObject <CBCentralManagerDelegate>
@property (nonatomic, strong) CBCentralManager* manager;
@property (nonatomic, strong) NSMutableDictionary<NSString*, CBPeripheral*>* seen;
@property (nonatomic, strong) NSMutableDictionary<NSString*, NSNumber*>* rssiMap;
@property (nonatomic, strong) NSMutableDictionary<NSString*, NSDictionary*>* advDataMap;
@end

@implementation ScanDelegate

- (instancetype)init {
  self = [super init];
  self.seen       = [NSMutableDictionary dictionary];
  self.rssiMap    = [NSMutableDictionary dictionary];
  self.advDataMap = [NSMutableDictionary dictionary];
  return self;
}

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
  if (central.state == CBManagerStatePoweredOn) {
    printf("scanning for 10 seconds...\n");
    [central scanForPeripheralsWithServices:nil options:nil];
  }
}

- (void)centralManager:(CBCentralManager*)central
 didDiscoverPeripheral:(CBPeripheral*)peripheral
     advertisementData:(NSDictionary*)advertisementData
                  RSSI:(NSNumber*)RSSI {
  NSString* uuid = peripheral.identifier.UUIDString;
  self.seen[uuid]        = peripheral;
  self.rssiMap[uuid]     = RSSI;
  self.advDataMap[uuid]  = advertisementData;
}

- (void)printResults {
  printf("\nfound %lu peripheral(s):\n", (unsigned long)self.seen.count);
  for (NSString* uuid in self.seen) {
    NSDictionary* adv  = self.advDataMap[uuid];
    CBPeripheral*  p   = self.seen[uuid];

    NSString* name = adv[CBAdvertisementDataLocalNameKey]
                  ?: p.name
                  ?: @"";

    printf("  %s  \"%s\"  RSSI %d\n",
      uuid.UTF8String,
      name.UTF8String,
      self.rssiMap[uuid].intValue);
  }
}

@end

int main() {
  ScanDelegate* delegate = [[ScanDelegate alloc] init];
  CBCentralManager* manager = [[CBCentralManager alloc] initWithDelegate:delegate queue:nil];
  delegate.manager = manager;

  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:10.0]];
  [delegate printResults];
  return 0;
}
