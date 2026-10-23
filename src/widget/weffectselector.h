#pragma once

#include <QComboBox>

#include "effects/defs.h"
#include "widget/wbasewidget.h"

class EffectsManager;
class QDomNode;
class SkinContext;

class WEffectSelector : public QComboBox, public WBaseWidget {
    Q_OBJECT
  public:
    WEffectSelector(QWidget* pParent, EffectsManager* pEffectsManager);

    void setup(const QDomNode& node, const SkinContext& context);

    void showPopup() override;
    void hidePopup() override;

  signals:
    void presetListVisibleChanged(bool visible);

  private slots:
    void slotEffectUpdated();
    void slotEffectSelected(int newIndex);
    void slotPresetListShowRequest(bool show);
    void populate();
    bool event(QEvent* pEvent) override;
#ifdef __ANDROID__
    void mousePressEvent(QMouseEvent* pEvent) override;
    void mouseReleaseEvent(QMouseEvent* pEvent) override;
#endif

  private:
    EffectsManager* m_pEffectsManager;
    VisibleEffectsListPointer m_pVisibleEffectsList;
    EffectSlotPointer m_pEffectSlot;
};
