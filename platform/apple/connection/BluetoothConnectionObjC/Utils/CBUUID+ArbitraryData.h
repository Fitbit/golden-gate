//
//  CBUUID+ArbitraryData.h
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 13/11/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

#import <CoreBluetooth/CoreBluetooth.h>

NS_ASSUME_NONNULL_BEGIN

@interface CBUUID (ArbitraryData)

/**
 Attempts to create a CBUUID using arbitrary data. If the data is not valid, nil is
 returned and the error is written in the provided error argument.

 @note Apple's UUIDWithData throws an exception that can not be caught in Swift.

 @param data The data to create the CBUUID from.
 @param error If the data is invalid, upon return it contains an instance of NSError
 that describes the failure.
 @return A CBUUID if the data was valid or nil otherwise.
*/
+ (nullable instancetype)UUIDWithArbitraryData:(NSData * _Nonnull)data error:(NSError * _Nullable * _Nullable)error;

@end

NS_ASSUME_NONNULL_END
