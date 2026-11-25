#include "widget/wjogwheel.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <cmath>

#include "library/coverartcache.h"
#include "mixer/basetrackplayer.h"
#include "moc_wjogwheel.cpp"
#include "skin/legacy/skincontext.h"
#include "track/track.h"
#include "util/fpclassify.h"
#include "util/math.h"
#include "waveform/visualplayposition.h"

namespace {

// 33 1/3 rpm
constexpr double kRotationsPerSecond = 100.0 / 3.0 / 60.0;

// Parts of the radius
constexpr double kRingWidth = 0.03;
constexpr double kRingRadius = 0.965;
constexpr double kPlatterRadius = 0.925;
// The outer ring that nudges instead of scratching
constexpr double kNudgeRadius = 0.80;
constexpr double kLabelRadius = 0.50;
constexpr int kStrobeDots = 60;

// Value of [ChannelN],jog per degree
constexpr double kNudgePerDegree = 0.15;

double normalizedDelta(double degrees) {
    while (degrees > 180) {
        degrees -= 360;
    }
    while (degrees < -180) {
        degrees += 360;
    }
    return degrees;
}

} // namespace

WJogWheel::WJogWheel(QWidget* pParent, const QString& group, BaseTrackPlayer* pPlayer)
        : WWidget(pParent),
          m_group(group),
          m_pVisualPlayPos(VisualPlayPosition::getVisualPlayPosition(group)),
          m_trackSamples(group, QStringLiteral("track_samples")),
          m_trackSampleRate(group, QStringLiteral("track_samplerate")),
          m_scratchPositionEnable(group, QStringLiteral("scratch_position_enable")),
          m_scratchPosition(group, QStringLiteral("scratch_position")),
          m_jog(group, QStringLiteral("jog")),
          m_color(QStringLiteral("#2ba6ff")),
          m_paintedAngle(-1),
          m_paintedPosition(-1),
          m_dragMode(DragMode::None),
          m_dragLastAngle(0),
          m_dragTotalAngle(0) {
    setFocusPolicy(Qt::NoFocus);
    connect(pPlayer, &BaseTrackPlayer::newTrackLoaded, this, &WJogWheel::slotTrackLoaded);
    connect(pPlayer, &BaseTrackPlayer::loadingTrack, this, &WJogWheel::slotLoadingTrack);
    CoverArtCache* pCache = CoverArtCache::instance();
    if (pCache) {
        connect(pCache, &CoverArtCache::coverFound, this, &WJogWheel::slotCoverFound);
    }
    setTrack(pPlayer->getLoadedTrack());
}

WJogWheel::~WJogWheel() {
    endDrag();
}

void WJogWheel::setup(const QDomNode& node, const SkinContext& context) {
    QString value;
    if (context.hasNodeSelectString(node, "Color", &value)) {
        m_color = QColor(value);
    }
    // Shown instead of the cover art if there is none
    m_label = context.selectString(node, "Text");
}

void WJogWheel::slotTrackLoaded(TrackPointer pTrack) {
    setTrack(pTrack);
}

void WJogWheel::slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack) {
    Q_UNUSED(pNewTrack);
    Q_UNUSED(pOldTrack);
    setTrack(TrackPointer());
}

void WJogWheel::setTrack(TrackPointer pTrack) {
    endDrag();
    if (m_pTrack) {
        disconnect(m_pTrack.get(),
                &Track::coverArtUpdated,
                this,
                &WJogWheel::slotCoverArtUpdated);
    }
    m_pTrack = pTrack;
    m_cover = QPixmap();
    m_labelPixmap = QPixmap();
    if (m_pTrack) {
        connect(m_pTrack.get(),
                &Track::coverArtUpdated,
                this,
                &WJogWheel::slotCoverArtUpdated);
        slotCoverArtUpdated();
    }
    update();
}

void WJogWheel::slotCoverArtUpdated() {
    if (m_pTrack) {
        CoverArtCache::requestTrackCover(this, m_pTrack);
    }
}

void WJogWheel::slotCoverFound(
        const QObject* pRequester,
        const CoverInfo& coverInfo,
        const QPixmap& pixmap) {
    if (pRequester != this || !m_pTrack ||
            m_pTrack->getLocation() != coverInfo.trackLocation) {
        return;
    }
    m_cover = pixmap;
    m_labelPixmap = QPixmap();
    update();
}

double WJogWheel::playPosition() const {
    const double position = m_pVisualPlayPos->getEnginePlayPos();
    return util_isfinite(position) ? position : 0.0;
}

double WJogWheel::trackSeconds() const {
    const double samples = m_trackSamples.get();
    const double sampleRate = m_trackSampleRate.get();
    if (!m_pTrack || samples <= 0 || sampleRate <= 0) {
        return 0.0;
    }
    return samples / 2 / sampleRate;
}

void WJogWheel::slotRenderTick() {
    if (!isVisible()) {
        return;
    }
    const double position = playPosition();
    const double angle = position * trackSeconds() * kRotationsPerSecond * 360;
    // Do not repaint for invisible changes, e.g. while the deck is stopped
    if (std::abs(angle - m_paintedAngle) < 0.2 &&
            std::abs(position - m_paintedPosition) < 0.0005) {
        return;
    }
    update();
}

QRectF WJogWheel::discRect() const {
    const double size = math_min(width(), height());
    return QRectF((width() - size) / 2.0, (height() - size) / 2.0, size, size);
}

double WJogWheel::angleOfPoint(const QPointF& point) const {
    const QPointF delta = point - discRect().center();
    return qRadiansToDegrees(std::atan2(delta.x(), -delta.y()));
}

void WJogWheel::updatePixmaps() {
    const double dpr = devicePixelRatioF();
    const QRectF rect = discRect();
    const int size = static_cast<int>(rect.width() * dpr);
    if (size <= 0) {
        return;
    }
    if (m_discPixmap.width() != size) {
        // Platter and record, which look the same at every angle
        m_discPixmap = QPixmap(size, size);
        m_discPixmap.fill(Qt::transparent);
        QPainter painter(&m_discPixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(size / 2.0, size / 2.0);
        const double radius = size / 2.0;
        const auto circle = [&painter, radius](double part, const QBrush& brush) {
            painter.setBrush(brush);
            painter.drawEllipse(QPointF(), radius * part, radius * part);
        };
        painter.setPen(Qt::NoPen);
        // Well of the position ring
        circle(0.998, QColor(0x0e, 0x0e, 0x0f));
        // Beveled edge and brushed metal of the platter
        QLinearGradient bevel(0, -radius, 0, radius);
        bevel.setColorAt(0.0, QColor(0x73, 0x77, 0x7e));
        bevel.setColorAt(0.5, QColor(0x32, 0x34, 0x37));
        bevel.setColorAt(1.0, QColor(0x18, 0x19, 0x1b));
        circle(kPlatterRadius + 0.008, bevel);
        QConicalGradient metal(QPointF(), 35);
        for (int n = 0; n <= 8; ++n) {
            metal.setColorAt(n / 8.0,
                    n % 2 ? QColor(0x28, 0x29, 0x2c) : QColor(0x45, 0x48, 0x4b));
        }
        circle(kPlatterRadius - 0.004, metal);
        // Shadow of the record on the platter
        QRadialGradient shadow(QPointF(), radius * (kNudgeRadius + 0.045));
        shadow.setColorAt(kNudgeRadius / (kNudgeRadius + 0.045), QColor(0, 0, 0, 170));
        shadow.setColorAt(1.0, QColor(0, 0, 0, 0));
        circle(kNudgeRadius + 0.045, shadow);
        // Record
        QRadialGradient vinyl(QPointF(), radius * kNudgeRadius);
        vinyl.setColorAt(0.0, QColor(0x22, 0x23, 0x25));
        vinyl.setColorAt(0.93, QColor(0x12, 0x13, 0x14));
        vinyl.setColorAt(0.975, QColor(0x29, 0x2b, 0x2d));
        vinyl.setColorAt(1.0, QColor(0x0f, 0x10, 0x11));
        circle(kNudgeRadius, vinyl);
        painter.setBrush(Qt::NoBrush);
        int groove = 0;
        for (double r = kLabelRadius + 0.02; r < kNudgeRadius - 0.03; r += 0.0125) {
            // The gaps between songs stand out
            const int alpha = groove++ % 7 == 0 ? 34 : 13;
            painter.setPen(QPen(QColor(255, 255, 255, alpha), math_max(0.8, radius * 0.003)));
            painter.drawEllipse(QPointF(), radius * r, radius * r);
        }
        painter.setPen(Qt::NoPen);
        // Light reflections
        QConicalGradient gloss(QPointF(), 45);
        for (const double start : {0.0, 0.5}) {
            gloss.setColorAt(start, QColor(255, 255, 255, 0));
            gloss.setColorAt(start + 0.07, QColor(255, 255, 255, 46));
            gloss.setColorAt(start + 0.15, QColor(255, 255, 255, 0));
            gloss.setColorAt(start + 0.27, QColor(255, 255, 255, 0));
            gloss.setColorAt(start + 0.32, QColor(255, 255, 255, 18));
            gloss.setColorAt(start + 0.38, QColor(255, 255, 255, 0));
        }
        gloss.setColorAt(1.0, QColor(255, 255, 255, 0));
        circle(kNudgeRadius - 0.02, gloss);
    }
    const int labelSize = static_cast<int>(size * kLabelRadius);
    if (labelSize > 0 && m_labelPixmap.width() != labelSize) {
        m_labelPixmap = QPixmap(labelSize, labelSize);
        m_labelPixmap.fill(Qt::transparent);
        QPainter painter(&m_labelPixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QPointF center(labelSize / 2.0, labelSize / 2.0);
        const double radius = labelSize / 2.0;
        // Brush instead of clipping, which is not antialiased
        painter.setPen(Qt::NoPen);
        if (!m_cover.isNull()) {
            const QPixmap scaled = m_cover.scaled(labelSize,
                    labelSize,
                    Qt::KeepAspectRatioByExpanding,
                    Qt::SmoothTransformation);
            QBrush brush(scaled);
            brush.setTransform(QTransform::fromTranslate(
                    (labelSize - scaled.width()) / 2.0,
                    (labelSize - scaled.height()) / 2.0));
            painter.setBrush(brush);
            painter.drawEllipse(center, radius - 1, radius - 1);
        } else {
            QLinearGradient gradient(0, 0, labelSize, labelSize);
            gradient.setColorAt(0.0, m_pTrack ? m_color.lighter(112) : QColor(0x42, 0x45, 0x48));
            gradient.setColorAt(1.0, m_pTrack ? m_color.darker(260) : QColor(0x21, 0x23, 0x24));
            painter.setBrush(gradient);
            painter.drawEllipse(center, radius - 1, radius - 1);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255, 255, 255, 40), math_max(1.0, radius * 0.012)));
            painter.drawEllipse(center, radius * 0.86, radius * 0.86);
            painter.drawEllipse(center, radius * 0.34, radius * 0.34);
            QFont labelFont = font();
            labelFont.setPixelSize(math_max(1, labelSize / 5));
            labelFont.setBold(true);
            painter.setFont(labelFont);
            painter.setPen(QColor(255, 255, 255, 225));
            // Above the spindle
            painter.drawText(QRectF(0, radius * 0.14, labelSize, radius * 0.52),
                    Qt::AlignCenter,
                    m_label);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(0, 0, 0, 190), math_max(1.5, radius * 0.035)));
        painter.drawEllipse(center, radius * 0.975, radius * 0.975);
        painter.setPen(QPen(QColor(255, 255, 255, 60), math_max(1.0, radius * 0.010)));
        painter.drawEllipse(center, radius * 0.945, radius * 0.945);
        // Spindle
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 150));
        painter.drawEllipse(center, radius * 0.11, radius * 0.11);
        QRadialGradient spindle(center - QPointF(radius * 0.025, radius * 0.025), radius * 0.09);
        spindle.setColorAt(0.0, QColor(0xf7, 0xf7, 0xf7));
        spindle.setColorAt(1.0, QColor(0x73, 0x77, 0x7e));
        painter.setBrush(spindle);
        painter.drawEllipse(center, radius * 0.07, radius * 0.07);
    }
}

void WJogWheel::paintEvent(QPaintEvent* pEvent) {
    Q_UNUSED(pEvent);
    updatePixmaps();
    const double position = playPosition();
    const double angle = position * trackSeconds() * kRotationsPerSecond * 360;
    m_paintedAngle = angle;
    m_paintedPosition = position;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const QRectF rect = discRect();
    const double radius = rect.width() / 2;
    painter.drawPixmap(rect, m_discPixmap, QRectF(m_discPixmap.rect()));

    // Rotating parts: strobe dots, label with the cover art, position marker
    painter.save();
    painter.translate(rect.center());
    painter.rotate(std::fmod(angle, 360));
    const double dotRadius = radius * (kPlatterRadius + kNudgeRadius) / 2;
    QPointF dots[kStrobeDots];
    for (int i = 0; i < kStrobeDots; ++i) {
        const double rad = qDegreesToRadians(i * 360.0 / kStrobeDots);
        dots[i] = QPointF(std::sin(rad) * dotRadius, -std::cos(rad) * dotRadius);
    }
    painter.setPen(QPen(QColor(255, 255, 255, 70), radius * 0.018, Qt::SolidLine, Qt::RoundCap));
    painter.drawPoints(dots, kStrobeDots);
    const double labelRadius = radius * kLabelRadius;
    painter.drawPixmap(QRectF(-labelRadius, -labelRadius, 2 * labelRadius, 2 * labelRadius),
            m_labelPixmap,
            QRectF(m_labelPixmap.rect()));
    const QColor markerColor = m_pTrack ? QColor(Qt::white) : QColor(0x6d, 0x71, 0x78);
    painter.setPen(QPen(markerColor, math_max(2.0, radius * 0.022), Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(0, -labelRadius * 1.1), QPointF(0, -radius * (kNudgeRadius - 0.05)));
    painter.setPen(QPen(m_pTrack ? m_color : markerColor,
            math_max(3.0, radius * 0.04),
            Qt::SolidLine,
            Qt::RoundCap));
    painter.drawLine(QPointF(0, -radius * (kNudgeRadius + 0.045)),
            QPointF(0, -radius * (kPlatterRadius - 0.045)));
    painter.restore();

    // Position in the track
    const double ringWidth = radius * kRingWidth;
    const double ringRadius = radius * kRingRadius;
    const QRectF ring(rect.center() - QPointF(ringRadius, ringRadius),
            QSizeF(2 * ringRadius, 2 * ringRadius));
    painter.setPen(QPen(QColor(0x2c, 0x2e, 0x30), ringWidth, Qt::SolidLine, Qt::FlatCap));
    painter.drawArc(ring, 0, 360 * 16);
    if (m_pTrack) {
        const int span = static_cast<int>(-math_clamp(position, 0.0, 1.0) * 360 * 16);
        // Glow
        QColor glow = m_color;
        glow.setAlpha(60);
        painter.setPen(QPen(glow, ringWidth * 2.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(ring, 90 * 16, span);
        painter.setPen(QPen(m_color, ringWidth, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(ring, 90 * 16, span);
    }
}

void WJogWheel::resizeEvent(QResizeEvent* pEvent) {
    m_discPixmap = QPixmap();
    m_labelPixmap = QPixmap();
    WWidget::resizeEvent(pEvent);
}

void WJogWheel::hideEvent(QHideEvent* pEvent) {
    endDrag();
    WWidget::hideEvent(pEvent);
}

void WJogWheel::mousePressEvent(QMouseEvent* pEvent) {
    if (pEvent->button() != Qt::LeftButton || !m_pTrack) {
        return;
    }
    endDrag();
    const QPointF delta = pEvent->position() - discRect().center();
    const double distance = std::hypot(delta.x(), delta.y()) / (discRect().width() / 2);
    if (distance > 1.0) {
        return;
    }
    m_dragLastAngle = angleOfPoint(pEvent->position());
    m_dragTotalAngle = 0;
    if (distance > kNudgeRadius) {
        m_dragMode = DragMode::Nudge;
        return;
    }
    m_dragMode = DragMode::Scratch;
    m_scratchPosition.set(0.0);
    m_scratchPositionEnable.set(1.0);
}

void WJogWheel::mouseMoveEvent(QMouseEvent* pEvent) {
    if (m_dragMode == DragMode::None) {
        return;
    }
    const double angle = angleOfPoint(pEvent->position());
    const double delta = normalizedDelta(angle - m_dragLastAngle);
    m_dragLastAngle = angle;
    if (m_dragMode == DragMode::Nudge) {
        m_jog.set(m_jog.get() + delta * kNudgePerDegree);
        return;
    }
    // One turn of the record is 1.8 seconds of the track. The position is
    // relative to the one where the drag started, in samples.
    m_dragTotalAngle += delta;
    const double seconds = m_dragTotalAngle / 360 / kRotationsPerSecond;
    m_scratchPosition.set(seconds * m_trackSampleRate.get() * 2);
}

void WJogWheel::mouseReleaseEvent(QMouseEvent* pEvent) {
    Q_UNUSED(pEvent);
    endDrag();
}

void WJogWheel::endDrag() {
    if (m_dragMode == DragMode::Scratch) {
        m_scratchPositionEnable.set(0.0);
    }
    m_dragMode = DragMode::None;
}
