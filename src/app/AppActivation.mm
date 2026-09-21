#include "app/AppActivation.h"

#import <AppKit/AppKit.h>

namespace Contestprogramm {

void activateThisApplication()
{
    [NSApp activateIgnoringOtherApps:YES];
}

} // namespace Contestprogramm
