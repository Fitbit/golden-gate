//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CBUUID+ArbitraryData.m
//  BluetoothConnection
//
//  Created by Emanuel Jarnea on 13/11/2020.
//

#import "CBUUID+ArbitraryData.h"

typedef NS_ENUM(NSInteger, CBUUIDArbitraryDataExceptionCode) {
    CBUUIDArbitraryDataExceptionCodeInvalidData = 1
};

@implementation CBUUID (ArbitraryData)

+ (nullable instancetype)UUIDWithArbitraryData:(NSData * _Nonnull)data error:(NSError * _Nullable * _Nullable)error {
    @try {
        return [CBUUID UUIDWithData:data];
    } @catch (NSException *exception) {
        if (error != nil) {
            NSMutableDictionary *userInfo = [exception.userInfo ? exception.userInfo : @{} mutableCopy];
            userInfo[NSLocalizedDescriptionKey] = exception.reason;

            *error = [[NSError alloc] initWithDomain:exception.name code:CBUUIDArbitraryDataExceptionCodeInvalidData userInfo:userInfo];
        }

        return nil;
    }
}

@end
