#pragma once

#include <QAbstractItemView>
#include <QScroller>
#include <QScrollerProperties>

namespace mixxx {

/// Lets the user scroll an item view by swiping with a finger, with kinetic
/// scrolling.
///
/// On Android touch input arrives as mouse events, so a left mouse button
/// drag scrolls. On desktop only touch screen input scrolls, the mouse keeps
/// working as before (selection, drag & drop).
inline void enableTouchScrolling(QAbstractItemView* pView) {
    // Scroll smoothly instead of jumping row by row while swiping
    pView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    pView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    QWidget* pViewport = pView->viewport();
#ifdef __ANDROID__
    QScroller::grabGesture(pViewport, QScroller::LeftMouseButtonGesture);
#else
    QScroller::grabGesture(pViewport, QScroller::TouchGesture);
#endif
    QScroller* pScroller = QScroller::scroller(pViewport);
    QScrollerProperties properties = pScroller->scrollerProperties();
    properties.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0.5);
    properties.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.1);
    properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
            QVariant::fromValue(QScrollerProperties::OvershootAlwaysOff));
    pScroller->setScrollerProperties(properties);
}

} // namespace mixxx
