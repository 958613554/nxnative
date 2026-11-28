// Shared Apple CAMetalLayer HDR/SDR configuration (front_mac.m, front_ios.m; front.h front_layer_hdr):
//   - SDR (hdr == 0): BGRA8Unorm in kCGColorSpaceSRGB, wantsExtendedDynamicRangeContent = NO
//   - HDR (hdr != 0): RGBA16Float in kCGColorSpaceExtendedLinearSRGB, wantsExtendedDynamicRangeContent = YES
// All three properties commit together in one CATransaction with actions disabled, and colorspace is set
// after pixelFormat because Core Animation can assign a format default when pixelFormat changes.
// Neither mode ever sets colorspace to NULL (which disables colour matching per CAMetalLayer.h).
#pragma once
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CATransaction.h>
#import <Metal/Metal.h>
#include <stdio.h>

static inline void front_hdr_layer_apply(CAMetalLayer *layer, int hdr) {
    if (!layer) return;
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    layer.wantsExtendedDynamicRangeContent = hdr != 0;
    layer.pixelFormat = hdr ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm;
    CGColorSpaceRef cs = CGColorSpaceCreateWithName(hdr ? kCGColorSpaceExtendedLinearSRGB : kCGColorSpaceSRGB);
    layer.colorspace = cs;
    CGColorSpaceRelease(cs);
    [CATransaction commit];
}

static inline const char *front_hdr_space_name(CGColorSpaceRef cs, char *buf, size_t n) {
    CFStringRef s = cs ? CGColorSpaceCopyName(cs) : NULL;
    if (!cs) snprintf(buf, n, "none (not colour-matched)");
    else if (!s || !CFStringGetCString(s, buf, (CFIndex)n, kCFStringEncodingUTF8)) snprintf(buf, n, "an unnamed colour space");
    if (s) CFRelease(s);
    return buf;
}
