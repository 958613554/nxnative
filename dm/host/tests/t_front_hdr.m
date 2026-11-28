// Check Apple CAMetalLayer SDR <-> HDR configuration (../front_hdr_apple.h):
//   - SDR setup labels BGRA8Unorm as kCGColorSpaceSRGB with EDR disabled.
//   - HDR setup switches to RGBA16Float with EDR enabled and kCGColorSpaceExtendedLinearSRGB.
//   - Returning to SDR restores BGRA8Unorm, EDR disabled, and kCGColorSpaceSRGB (never NULL).
//   - Applying either mode overwrites an unmatched (NULL) or Display P3 color space.
#import "../front_hdr_apple.h"
#include <stdio.h>
#include <string.h>

static int checks, fails;
#define CHECK(c, msg) do { checks++; if (!(c)) { fails++; fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); } } while (0)

static int cs_is(CGColorSpaceRef cs, CFStringRef name) {
    if (!cs) return 0;
    CGColorSpaceRef want = CGColorSpaceCreateWithName(name);
    int eq = want && CFEqual(cs, want);
    if (want) CGColorSpaceRelease(want);
    return eq;
}

int main(void) {
    @autoreleasepool {
        CAMetalLayer *layer = [CAMetalLayer layer];
        layer.device = MTLCreateSystemDefaultDevice();
        CHECK(layer != nil, "CAMetalLayer created");

        // Initial SDR configuration
        front_hdr_layer_apply(layer, 0);
        CHECK(layer.pixelFormat == MTLPixelFormatBGRA8Unorm, "initial SDR pixelFormat is BGRA8Unorm");
        CHECK(layer.wantsExtendedDynamicRangeContent == NO, "initial SDR disables EDR");
        CHECK(layer.colorspace != NULL, "initial SDR colorspace is non-NULL");
        CHECK(cs_is(layer.colorspace, kCGColorSpaceSRGB), "initial SDR colorspace is kCGColorSpaceSRGB");

        // SDR -> HDR
        front_hdr_layer_apply(layer, 1);
        CHECK(layer.pixelFormat == MTLPixelFormatRGBA16Float, "HDR pixelFormat is RGBA16Float");
        CHECK(layer.wantsExtendedDynamicRangeContent == YES, "HDR enables EDR");
        CHECK(layer.colorspace != NULL, "HDR colorspace is non-NULL");
        CHECK(cs_is(layer.colorspace, kCGColorSpaceExtendedLinearSRGB), "HDR colorspace is kCGColorSpaceExtendedLinearSRGB");

        // HDR -> SDR round trip (must restore kCGColorSpaceSRGB, never NULL)
        front_hdr_layer_apply(layer, 0);
        CHECK(layer.pixelFormat == MTLPixelFormatBGRA8Unorm, "return to SDR pixelFormat is BGRA8Unorm");
        CHECK(layer.wantsExtendedDynamicRangeContent == NO, "return to SDR disables EDR");
        CHECK(layer.colorspace != NULL, "return to SDR colorspace is non-NULL");
        CHECK(cs_is(layer.colorspace, kCGColorSpaceSRGB), "return to SDR colorspace is kCGColorSpaceSRGB");

        // Second round trip and idempotent re-apply
        front_hdr_layer_apply(layer, 1);
        front_hdr_layer_apply(layer, 1);
        CHECK(layer.pixelFormat == MTLPixelFormatRGBA16Float, "repeated HDR keeps RGBA16Float");
        CHECK(layer.wantsExtendedDynamicRangeContent == YES, "repeated HDR keeps EDR enabled");
        CHECK(cs_is(layer.colorspace, kCGColorSpaceExtendedLinearSRGB), "repeated HDR keeps kCGColorSpaceExtendedLinearSRGB");

        front_hdr_layer_apply(layer, 0);
        front_hdr_layer_apply(layer, 0);
        CHECK(layer.pixelFormat == MTLPixelFormatBGRA8Unorm, "repeated SDR keeps BGRA8Unorm");
        CHECK(layer.wantsExtendedDynamicRangeContent == NO, "repeated SDR keeps EDR disabled");
        CHECK(cs_is(layer.colorspace, kCGColorSpaceSRGB), "repeated SDR keeps kCGColorSpaceSRGB");

        // Recovery from Display P3 (old #3216 state) and NULL (unmatched state)
        CGColorSpaceRef p3 = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearDisplayP3);
        layer.pixelFormat = MTLPixelFormatRGBA16Float;
        layer.wantsExtendedDynamicRangeContent = YES;
        layer.colorspace = p3;
        CGColorSpaceRelease(p3);
        front_hdr_layer_apply(layer, 0);
        CHECK(layer.pixelFormat == MTLPixelFormatBGRA8Unorm, "recovery from ExtLinP3 to SDR sets BGRA8Unorm");
        CHECK(cs_is(layer.colorspace, kCGColorSpaceSRGB), "recovery from ExtLinP3 to SDR restores kCGColorSpaceSRGB");

        layer.colorspace = NULL;
        front_hdr_layer_apply(layer, 0);
        CHECK(cs_is(layer.colorspace, kCGColorSpaceSRGB), "recovery from NULL to SDR restores kCGColorSpaceSRGB");

        layer.colorspace = NULL;
        front_hdr_layer_apply(layer, 1);
        CHECK(cs_is(layer.colorspace, kCGColorSpaceExtendedLinearSRGB), "recovery from NULL to HDR sets kCGColorSpaceExtendedLinearSRGB");

        // NULL layer guard and log-name formatting
        front_hdr_layer_apply(nil, 0);
        front_hdr_layer_apply(nil, 1);
        char buf[96];
        CHECK(!strcmp(front_hdr_space_name(NULL, buf, sizeof buf), "none (not colour-matched)"), "NULL space log name");
        CGColorSpaceRef srgb = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CHECK(!strcmp(front_hdr_space_name(srgb, buf, sizeof buf), "kCGColorSpaceSRGB"), "sRGB space log name");
        CGColorSpaceRelease(srgb);
        CGColorSpaceRef ext = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
        CHECK(!strcmp(front_hdr_space_name(ext, buf, sizeof buf), "kCGColorSpaceExtendedLinearSRGB"), "extended linear sRGB space log name");
        CGColorSpaceRelease(ext);

        printf("front hdr layer: %d checks, %d failed\n", checks, fails);
        return fails != 0;
    }
}
