//
//  UIApplication+BSMobileProvision.h
//
//  Created by kaolin fire on 2013-06-24.
//  Copyright (c) 2013 The Blindsight Corporation. All rights reserved.
//  Released under the BSD 2-Clause License (see LICENSE)

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, UIApplicationReleaseMode) {
    UIApplicationReleaseUnknown,
    UIApplicationReleaseSim,
    UIApplicationReleaseDev,
    UIApplicationReleaseAdHoc,
    UIApplicationReleaseAppStore,
    UIApplicationReleaseEnterprise,
};

@interface UIApplication (BSMobileProvision)

-(UIApplicationReleaseMode) releaseMode;

@end
