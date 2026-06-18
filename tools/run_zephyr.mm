#import <Foundation/Foundation.h>
#import <IOBluetooth/IOBluetooth.h>
#include <cstdio>

static NSString* const NAME_PREFIX    = @"BH BHT";
static NSString* const DEVICE_ADDRESS = @"a0-e6-f8-a5-60-bf";
static const BluetoothRFCOMMChannelID RFCOMM_CHANNEL = 1;

// ----------------------------------------------------------------------------
// RFCOMM delegate — receives serial data
// ----------------------------------------------------------------------------
@interface ZephyrChannelDelegate : NSObject <IOBluetoothRFCOMMChannelDelegate>
@end

@implementation ZephyrChannelDelegate

- (void)rfcommChannelData:(IOBluetoothRFCOMMChannel*)channel
                     data:(void*)dataPointer
                   length:(size_t)dataLength {
  const uint8_t* bytes = (const uint8_t*)dataPointer;

  printf("hex [%zu]:", dataLength);
  for (size_t i = 0; i < dataLength; i++) printf(" %02X", bytes[i]);
  printf("\n");

  static NSMutableData* buf = nil;
  if (!buf) buf = [NSMutableData data];
  [buf appendBytes:bytes length:dataLength];

  while (true) {
    const uint8_t* b = (const uint8_t*)buf.bytes;
    NSUInteger len = buf.length;
    NSUInteger nl = NSNotFound;
    for (NSUInteger i = 0; i < len; i++) {
      if (b[i] == '\n') { nl = i; break; }
    }
    if (nl == NSNotFound) break;
    NSString* line = [[NSString alloc] initWithBytes:b length:nl encoding:NSUTF8StringEncoding];
    printf("ascii: %s\n", line.UTF8String ?: "(invalid UTF-8)");
    [buf replaceBytesInRange:NSMakeRange(0, nl + 1) withBytes:nil length:0];
  }
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel*)channel {
  printf("RFCOMM channel closed\n");
  exit(0);
}

@end

// ----------------------------------------------------------------------------
// Connection manager — explicit connect then RFCOMM open
// ----------------------------------------------------------------------------
@interface ZephyrConnector : NSObject <IOBluetoothDeviceInquiryDelegate>
- (void)connectTo:(IOBluetoothDevice*)device;
@property (nonatomic, assign) BOOL found;
@end

@implementation ZephyrConnector {
  ZephyrChannelDelegate* _channelDelegate;
  IOBluetoothRFCOMMChannel* _channel;
}

- (instancetype)init {
  self = [super init];
  _channelDelegate = [[ZephyrChannelDelegate alloc] init];
  return self;
}

- (void)connectTo:(IOBluetoothDevice*)device {
  printf("SDP services:\n");
  for (IOBluetoothSDPServiceRecord* record in device.services) {
    BluetoothRFCOMMChannelID cid = 0;
    if ([record getRFCOMMChannelID:&cid] == kIOReturnSuccess)
      printf("  %s -> channel %d\n", ([record getServiceName] ?: @"(unnamed)").UTF8String, cid);
  }

  // Run sync open on a background thread — async silently stalls without a
  // prior baseband connection, and sync forces the full connect sequence.
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    printf("Connecting baseband...\n");
    IOReturn r = [device openConnection];
    if (r != kIOReturnSuccess) {
      printf("openConnection failed: 0x%08X\n", r);
      exit(1);
    }
    printf("Baseband connected. Opening RFCOMM channel %d...\n", RFCOMM_CHANNEL);

    IOBluetoothRFCOMMChannel* __autoreleasing ch = nil;
    r = [device openRFCOMMChannelSync:&ch
                          withChannelID:RFCOMM_CHANNEL
                               delegate:_channelDelegate];
    _channel = ch;

    if (r != kIOReturnSuccess) {
      printf("openRFCOMMChannelSync failed: 0x%08X\n", r);
      exit(1);
    }
    printf("RFCOMM channel open — receiving data...\n");
  });
}

// Inquiry delegate
- (void)deviceInquiryDeviceFound:(IOBluetoothDeviceInquiry*)sender
                           device:(IOBluetoothDevice*)device {
  if (NAME_PREFIX && ![device.nameOrAddress hasPrefix:NAME_PREFIX]) return;
  _found = YES;
  printf("Found: %s  (%s)\n", device.nameOrAddress.UTF8String, device.addressString.UTF8String);
  [sender stop];
  [self connectTo:device];
}

- (void)deviceInquiryComplete:(IOBluetoothDeviceInquiry*)sender
                         error:(IOReturn)error
                       aborted:(BOOL)aborted {
  if (!_found) {
    printf("Scan complete — device not found (prefix: \"%s\")\n", NAME_PREFIX.UTF8String);
    exit(1);
  }
}

@end

// ----------------------------------------------------------------------------
static IOBluetoothDevice* findPairedDevice() {
  for (IOBluetoothDevice* d in [IOBluetoothDevice pairedDevices]) {
    if ([(d.nameOrAddress ?: @"") hasPrefix:NAME_PREFIX]) return d;
  }
  return nil;
}

int main() {
  ZephyrConnector* connector = [[ZephyrConnector alloc] init];

  if (DEVICE_ADDRESS) {
    IOBluetoothDevice* device = [IOBluetoothDevice deviceWithAddressString:DEVICE_ADDRESS];
    if (!device) { printf("No device for address %s\n", DEVICE_ADDRESS.UTF8String); return 1; }
    [connector connectTo:device];
  } else if (IOBluetoothDevice* paired = findPairedDevice()) {
    printf("Found paired: %s  (%s)\n",
           paired.nameOrAddress.UTF8String, paired.addressString.UTF8String);
    [connector connectTo:paired];
  } else {
    printf("Scanning (device must be in discoverable mode)...\n");
    IOBluetoothDeviceInquiry* inquiry = [IOBluetoothDeviceInquiry inquiryWithDelegate:connector];
    inquiry.updateNewDeviceNames = YES;
    [inquiry start];
  }

  [[NSRunLoop currentRunLoop] run];
  return 0;
}
