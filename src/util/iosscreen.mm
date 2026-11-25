#include "util/iosscreen.h"

#import <UIKit/UIKit.h>

#include <algorithm>

namespace mixxx {

double iosScreenShortSide() {
    const CGSize size = UIScreen.mainScreen.bounds.size;
    return std::min(size.width, size.height);
}

} // namespace mixxx
