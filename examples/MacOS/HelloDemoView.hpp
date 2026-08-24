#pragma once

#import <MetalKit/MetalKit.h>

@interface MosaicHelloDemoView : MTKView <MTKViewDelegate, NSTextInputClient>
- (BOOL)handleApplicationScrollWheelEvent:(NSEvent *)event;
@end
