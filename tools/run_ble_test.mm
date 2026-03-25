#include <lsl_c.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstdio>

static NSString* const DEVICE_ID = @"CA6A61B7-B7A8-AF24-3C9E-04A6A5012554";

static CBUUID* uuid(NSString* s) { return [CBUUID UUIDWithString:s]; }

static CBUUID* const CONTROL      = uuid(@"273E0001-4C4D-454D-96BE-F03BAC821358");
static CBUUID* const EEG_TP9      = uuid(@"273E0003-4C4D-454D-96BE-F03BAC821358");
static CBUUID* const EEG_AF7      = uuid(@"273E0004-4C4D-454D-96BE-F03BAC821358");
static CBUUID* const EEG_AF8      = uuid(@"273E0005-4C4D-454D-96BE-F03BAC821358");
static CBUUID* const EEG_TP10     = uuid(@"273E0006-4C4D-454D-96BE-F03BAC821358");
static CBUUID* const EEG_AUX      = uuid(@"273E0007-4C4D-454D-96BE-F03BAC821358");
static CBUUID* const PPG_AMBIENT  = uuid(@"273E000F-4C4D-454D-96BE-F03BAC821358");
static CBUUID* const PPG_IR       = uuid(@"273E0010-4C4D-454D-96BE-F03BAC821358");
static CBUUID* const PPG_RED      = uuid(@"273E0011-4C4D-454D-96BE-F03BAC821358");

// ----------------------------------------------------------------------------
// Packet unpacking
// EEG: 2-byte big-endian packet index + 12 x 12-bit samples packed MSB-first
// ----------------------------------------------------------------------------
static uint16_t unpackEeg(const uint8_t* bytes, size_t len, double out[12]) {
  if (len < 20) return 0;
  uint16_t index = ((uint16_t)bytes[0] << 8) | bytes[1];
  const uint8_t* d = bytes + 2;
  for (int i = 0; i < 12; i++) {
    uint16_t raw;
    if (i % 2 == 0) {
      int b = (i / 2) * 3;
      raw = ((uint16_t)d[b] << 4) | (d[b + 1] >> 4);
    } else {
      int b = ((i - 1) / 2) * 3 + 1;
      raw = (((uint16_t)d[b] & 0xF) << 8) | d[b + 1];
    }
    out[i] = 0.48828125 * ((double)raw - 2048.0);
  }
  return index;
}

// PPG: 2-byte big-endian packet index + 6 x 24-bit samples packed MSB-first
static uint16_t unpackPpg(const uint8_t* bytes, size_t len, uint32_t out[6]) {
  if (len < 20) return 0;
  uint16_t index = ((uint16_t)bytes[0] << 8) | bytes[1];
  for (int i = 0; i < 6; i++) {
    const uint8_t* d = bytes + 2 + i * 3;
    out[i] = ((uint32_t)d[0] << 16) | ((uint32_t)d[1] << 8) | d[2];
  }
  return index;
}

// ----------------------------------------------------------------------------
// Recursive Least Squares timestamp correction (same algorithm as MuseLSL2)
//
// Model: arrival_time - t0 ≈ rate * sample_index
// Estimates 'rate' (sample period) online using RLS.
// t0 is fixed at stream start.
// ----------------------------------------------------------------------------
struct RLS {
  double t0;
  double rate;  // sample period estimate
  double P;     // estimation error covariance

  RLS() : t0(0.0), rate(0.0), P(1e-4) {}

  void init(double sample_period) {
    t0   = lsl_local_clock();
    rate = sample_period;
    P    = 1e-4;
  }

  // t_source:   cumulative sample index of the last sample in the chunk
  // t_receiver: absolute LSL clock time of earliest channel arrival
  void update(double t_source, double t_receiver) {
    double tr    = t_receiver - t0;
    double denom = 1.0 - P * t_source * t_source;
    if (std::abs(denom) < 1e-15) return;
    P    = P - (P * P * t_source * t_source) / denom;
    rate = rate + P * t_source * (tr - t_source * rate);
  }

  double timestamp(uint64_t i) const { return rate * (double)i + t0; }
};

// ----------------------------------------------------------------------------
// Delegate
// ----------------------------------------------------------------------------
@interface MuseDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
@property (nonatomic, strong) CBCentralManager*         manager;
@property (nonatomic, strong) CBPeripheral*             peripheral;
@property (nonatomic, strong) CBCharacteristic*         controlChar;
@property (nonatomic, assign) int                       pendingServices;
@property (nonatomic, assign) int                       pendingSubscriptions;
@property (nonatomic, strong) NSMutableArray<NSData*>*  commandQueue;
@end

@implementation MuseDelegate {
  // EEG state
  RLS      _eegRLS;
  double   _eegTimestamps[5];   // arrival time per channel (0=TP9..4=AUX)
  double   _eegData[5][12];
  BOOL     _eegReceived[5];
  uint16_t _lastEegTm;
  uint64_t _eegSampleIndex;
  BOOL     _eegFirstChunk;
  double   _prevEegLastTs;      // corrected timestamp of last sample in previous chunk

  // PPG state (fixed interpolation, no RLS — same as MuseLSL2)
  double   _ppgT0;
  double   _ppgRate;
  double   _ppgTimestamps[3];
  uint32_t _ppgData[3][6];
  BOOL     _ppgReceived[3];
  uint16_t _lastPpgTm;
  uint64_t _ppgSampleIndex;
  BOOL     _ppgFirstChunk;
  double   _prevPpgArrival;
}

- (instancetype)init {
  self = [super init];
  for (int i = 0; i < 5; i++) _eegReceived[i] = NO;
  for (int i = 0; i < 3; i++) _ppgReceived[i] = NO;
  _eegFirstChunk  = YES;
  _ppgFirstChunk  = YES;
  _eegSampleIndex = 0;
  _ppgSampleIndex = 0;
  _lastEegTm      = 0;
  _lastPpgTm      = 0;
  _prevEegLastTs = 0;
  _prevPpgArrival = 0;
  return self;
}

- (void)centralManagerDidUpdateState:(CBCentralManager*)central {
  if (central.state == CBManagerStatePoweredOn) {
    NSLog(@"scanning...");
    [central scanForPeripheralsWithServices:nil options:nil];
  }
}

- (void)centralManager:(CBCentralManager*)central
 didDiscoverPeripheral:(CBPeripheral*)peripheral
     advertisementData:(NSDictionary*)advertisementData
                  RSSI:(NSNumber*)RSSI {
  if ([peripheral.identifier.UUIDString isEqualToString:DEVICE_ID]) {
    NSLog(@"found: %@", peripheral.name);
    self.peripheral = peripheral;
    [central stopScan];
    [central connectPeripheral:peripheral options:nil];
  }
}

- (void)centralManager:(CBCentralManager*)central
  didConnectPeripheral:(CBPeripheral*)peripheral {
  NSLog(@"connected");
  peripheral.delegate = self;
  [peripheral discoverServices:nil];
}

- (void)peripheral:(CBPeripheral*)peripheral
didDiscoverServices:(NSError*)error {
  self.pendingServices = (int)peripheral.services.count;
  for (CBService* service in peripheral.services) {
    [peripheral discoverCharacteristics:nil forService:service];
  }
}

- (void)peripheral:(CBPeripheral*)peripheral
didDiscoverCharacteristicsForService:(CBService*)service
             error:(NSError*)error {
  for (CBCharacteristic* characteristic in service.characteristics) {
    if ([characteristic.UUID isEqual:CONTROL]) {
      self.controlChar = characteristic;
    }
    if (characteristic.properties & CBCharacteristicPropertyNotify) {
      self.pendingSubscriptions++;
      [peripheral setNotifyValue:YES forCharacteristic:characteristic];
    }
  }
  if (--self.pendingServices == 0) {
    NSLog(@"all characteristics discovered, waiting for %d subscriptions",
          self.pendingSubscriptions);
  }
}

- (void)peripheral:(CBPeripheral*)peripheral
didUpdateNotificationStateForCharacteristic:(CBCharacteristic*)characteristic
             error:(NSError*)error {
  if (error) NSLog(@"subscription error %@: %@", characteristic.UUID, error);
  if (--self.pendingSubscriptions == 0) {
    NSLog(@"all subscriptions active, sending start commands");
    _eegRLS.init(1.0 / 256.0);
    _ppgT0   = lsl_local_clock();
    _ppgRate = 1.0 / 64.0;
    self.commandQueue = [@[
      [self bufferForCommand:@"h"],
      [self bufferForCommand:@"p50"],
      [self bufferForCommand:@"s"],
      [self bufferForCommand:@"d"],
    ] mutableCopy];
    [self sendNextCommand];
  }
}

- (void)sendNextCommand {
  if (self.commandQueue.count == 0) {
    NSLog(@"streaming started");
    return;
  }
  NSData* cmd = self.commandQueue.firstObject;
  [self.commandQueue removeObjectAtIndex:0];
  [self.peripheral writeValue:cmd
             forCharacteristic:self.controlChar
                          type:CBCharacteristicWriteWithoutResponse];
  [self sendNextCommand];
}

- (NSData*)bufferForCommand:(NSString*)cmd {
  NSString* str = [NSString stringWithFormat:@"X%@\n", cmd];
  NSMutableData* buf = [[str dataUsingEncoding:NSUTF8StringEncoding] mutableCopy];
  uint8_t len = (uint8_t)(buf.length - 1);
  [buf replaceBytesInRange:NSMakeRange(0, 1) withBytes:&len];
  return buf;
}

// Returns channel index (0-4) for EEG UUIDs, -1 otherwise
- (int)eegChannelIndex:(CBUUID*)u {
  if ([u isEqual:EEG_TP9])  return 0;
  if ([u isEqual:EEG_AF7])  return 1;
  if ([u isEqual:EEG_AF8])  return 2;
  if ([u isEqual:EEG_TP10]) return 3;
  if ([u isEqual:EEG_AUX])  return 4;
  return -1;
}

// Returns channel index (0-2) for PPG UUIDs, -1 otherwise
- (int)ppgChannelIndex:(CBUUID*)u {
  if ([u isEqual:PPG_AMBIENT]) return 0;
  if ([u isEqual:PPG_IR])      return 1;
  if ([u isEqual:PPG_RED])     return 2;
  return -1;
}

- (void)peripheral:(CBPeripheral*)peripheral
didUpdateValueForCharacteristic:(CBCharacteristic*)characteristic
             error:(NSError*)error {
  CBUUID* u = characteristic.UUID;
  NSData* data = characteristic.value;
  if (data.length < 20) return;

  const uint8_t* bytes = (const uint8_t*)data.bytes;
  double now = lsl_local_clock();

  int eegCh = [self eegChannelIndex:u];
  int ppgCh = [self ppgChannelIndex:u];

  if (eegCh >= 0) {
    [self handleEegChannel:eegCh bytes:bytes length:data.length timestamp:now];
  } else if (ppgCh >= 0) {
    [self handlePpgChannel:ppgCh bytes:bytes length:data.length timestamp:now];
  }
}

- (void)handleEegChannel:(int)ch
                   bytes:(const uint8_t*)bytes
                  length:(size_t)length
               timestamp:(double)now {
  double samples[12];
  uint16_t tm = unpackEeg(bytes, length, samples);

  _eegTimestamps[ch] = now;
  for (int i = 0; i < 12; i++) _eegData[ch][i] = samples[i];
  _eegReceived[ch] = YES;

  // Process chunk once all 5 channels received
  BOOL allReceived = YES;
  for (int i = 0; i < 5; i++) if (!_eegReceived[i]) { allReceived = NO; break; }
  if (!allReceived) return;

  if (_eegFirstChunk) {
    _lastEegTm    = tm - 1;
    _eegFirstChunk = NO;
  }

  // Detect missing packets and advance sample index accordingly
  int16_t delta = (int16_t)(tm - _lastEegTm);
  if (delta != 1) {
    if (delta > 1)
      printf("EEG: missing %d packets (tm=%u, last=%u)\n", delta - 1, tm, _lastEegTm);
    _eegSampleIndex += (uint64_t)(delta - 1) * 12;
  }
  _lastEegTm = tm;

  // Earliest arrival = best estimate of when the chunk was actually sampled
  double earliest = *std::min_element(_eegTimestamps, _eegTimestamps + 5);

  // RLS update: regress (last sample index in chunk, earliest arrival)
  double lastIdx = (double)(_eegSampleIndex + 11);
  _eegRLS.update(lastIdx, earliest);

  // Corrected timestamps for all 12 samples
  double corrected[12];
  for (int i = 0; i < 12; i++)
    corrected[i] = _eegRLS.timestamp(_eegSampleIndex + i);

  // Print 12 inter-sample intervals: boundary + 11 within-chunk
  if (_prevEegLastTs > 0) {
    printf("  %f\n", (corrected[0] - _prevEegLastTs) * 1000.0);
    for (int i = 1; i < 12; i++)
      printf("  %f\n", (corrected[i] - corrected[i - 1]) * 1000.0);
  }
  _prevEegLastTs = corrected[11];

  _eegSampleIndex += 12;
  for (int i = 0; i < 5; i++) _eegReceived[i] = NO;
}

- (void)handlePpgChannel:(int)ch
                   bytes:(const uint8_t*)bytes
                  length:(size_t)length
               timestamp:(double)now {
  uint32_t samples[6];
  uint16_t tm = unpackPpg(bytes, length, samples);

  _ppgTimestamps[ch] = now;
  for (int i = 0; i < 6; i++) _ppgData[ch][i] = samples[i];
  _ppgReceived[ch] = YES;

  BOOL allReceived = YES;
  for (int i = 0; i < 3; i++) if (!_ppgReceived[i]) { allReceived = NO; break; }
  if (!allReceived) return;

  if (_ppgFirstChunk) {
    _lastPpgTm     = tm - 1;
    _ppgFirstChunk = NO;
  }

  int16_t delta = (int16_t)(tm - _lastPpgTm);
  if (delta != 1 && delta > 1)
    printf("PPG: missing %d packets (tm=%u, last=%u)\n", delta - 1, tm, _lastPpgTm);
  _lastPpgTm = tm;

  double earliest = *std::min_element(_ppgTimestamps, _ppgTimestamps + 3);

  // PPG timestamps: fixed interpolation (no RLS, same as MuseLSL2)
  double corrected[6];
  for (int i = 0; i < 6; i++)
    corrected[i] = _ppgRate * (double)(_ppgSampleIndex + i) + _ppgT0;

  if (_prevPpgArrival > 0) {
    double rawInterval       = (earliest - _prevPpgArrival) * 1000.0;
    double correctedInterval = (corrected[0] - (_ppgRate * (double)(_ppgSampleIndex - 6) + _ppgT0)) * 1000.0;
    printf("PPG  raw: %6.2f ms  corrected: %6.3f ms  (expected 93.750)\n",
           rawInterval, correctedInterval);
  }
  _prevPpgArrival = earliest;

  _ppgSampleIndex += 6;
  for (int i = 0; i < 3; i++) _ppgReceived[i] = NO;
}

@end

int main() {
  MuseDelegate* delegate = [[MuseDelegate alloc] init];
  CBCentralManager* manager = [[CBCentralManager alloc] initWithDelegate:delegate queue:nil];
  delegate.manager = manager;

  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:30.0]];
  return 0;
}
