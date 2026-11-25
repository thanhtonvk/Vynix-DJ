#pragma once

#include <QColor>
#include <QPixmap>
#include <QSharedPointer>
#include <QString>

#include "control/pollingcontrolproxy.h"
#include "library/coverart.h"
#include "track/track_decl.h"
#include "widget/wwidget.h"

class BaseTrackPlayer;
class QDomNode;
class SkinContext;
class VisualPlayPosition;

/// Turntable for touch skins: a record with the cover art that turns with the
/// deck, surrounded by a ring that shows the position in the track.
///
/// Dragging the record scratches, dragging the outer ring nudges the deck
/// (pitch bend). Unlike WSpinny it is drawn without OpenGL.
class WJogWheel : public WWidget {
    Q_OBJECT
  public:
    WJogWheel(QWidget* pParent, const QString& group, BaseTrackPlayer* pPlayer);
    ~WJogWheel() override;

    void setup(const QDomNode& node, const SkinContext& context);

  public slots:
    void slotRenderTick();

  protected:
    void paintEvent(QPaintEvent* pEvent) override;
    void resizeEvent(QResizeEvent* pEvent) override;
    void hideEvent(QHideEvent* pEvent) override;
    void mousePressEvent(QMouseEvent* pEvent) override;
    void mouseMoveEvent(QMouseEvent* pEvent) override;
    void mouseReleaseEvent(QMouseEvent* pEvent) override;

  private slots:
    void slotTrackLoaded(TrackPointer pTrack);
    void slotLoadingTrack(TrackPointer pNewTrack, TrackPointer pOldTrack);
    void slotCoverArtUpdated();
    void slotCoverFound(
            const QObject* pRequester,
            const CoverInfo& coverInfo,
            const QPixmap& pixmap);

  private:
    enum class DragMode {
        None,
        Scratch,
        Nudge,
    };

    double playPosition() const;
    /// Length of the track in seconds, 0 if there is none
    double trackSeconds() const;
    void setTrack(TrackPointer pTrack);
    void endDrag();
    /// Clockwise angle in degrees of a point in the widget, 0 is on top
    double angleOfPoint(const QPointF& point) const;
    QRectF discRect() const;
    void updatePixmaps();

    const QString m_group;
    QSharedPointer<VisualPlayPosition> m_pVisualPlayPos;
    PollingControlProxy m_trackSamples;
    PollingControlProxy m_trackSampleRate;
    PollingControlProxy m_scratchPositionEnable;
    PollingControlProxy m_scratchPosition;
    PollingControlProxy m_jog;

    TrackPointer m_pTrack;
    QPixmap m_cover;
    QColor m_color;
    QString m_label;

    // Prerendered parts, in device pixels
    QPixmap m_discPixmap;
    QPixmap m_labelPixmap;

    double m_paintedAngle;
    double m_paintedPosition;

    DragMode m_dragMode;
    double m_dragLastAngle;
    double m_dragTotalAngle;
};
