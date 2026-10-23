#include "effects/effectknobparameterslot.h"

#include "control/controleffectknob.h"
#include "control/controlobject.h"
#include "control/controlpushbutton.h"
#include "controllers/softtakeover.h"
#include "effects/effectparameter.h"
#include "moc_effectknobparameterslot.cpp"
#include "util/math.h"

EffectKnobParameterSlot::EffectKnobParameterSlot(
        const QString& group, const unsigned int iParameterSlotNumber)
        : EffectParameterSlotBase(
                  group, iParameterSlotNumber, EffectParameterType::Knob) {
    QString itemPrefix = formatItemPrefix(iParameterSlotNumber);

    m_pControlValue = std::make_unique<ControlEffectKnob>(
            ConfigKey(m_group, itemPrefix));
    connect(m_pControlValue.get(),
            &ControlObject::valueChanged,
            this,
            &EffectKnobParameterSlot::slotValueChanged);

    m_pControlLoaded = std::make_unique<ControlObject>(
            ConfigKey(m_group, itemPrefix + QString("_loaded")));
    m_pControlLoaded->setReadOnly();

    m_pControlType = std::make_unique<ControlObject>(
            ConfigKey(m_group, itemPrefix + QString("_type")));
    m_pControlType->setReadOnly();

    m_pControlLinkType = std::make_unique<ControlPushButton>(
            ConfigKey(m_group, itemPrefix + QString("_link_type")));
    m_pControlLinkType->setBehavior(mixxx::control::ButtonMode::Toggle,
            static_cast<int>(EffectManifestParameter::LinkType::NumLinkTypes));
    m_pControlLinkType->connectValueChangeRequest(
            this, &EffectKnobParameterSlot::slotLinkTypeChanging);

    m_pControlLinkInverse = std::make_unique<ControlPushButton>(
            ConfigKey(m_group, itemPrefix + QString("_link_inverse")));
    m_pControlLinkInverse->setButtonMode(mixxx::control::ButtonMode::Toggle);
    connect(m_pControlLinkInverse.get(),
            &ControlObject::valueChanged,
            this,
            &EffectKnobParameterSlot::slotLinkInverseChanged);

    m_pControlUnits = std::make_unique<ControlObject>(
            ConfigKey(m_group, itemPrefix + QStringLiteral("_units")));
    m_pControlUnits->setReadOnly();

    m_pControlBeatPeriod = std::make_unique<ControlObject>(
            ConfigKey(m_group, itemPrefix + QStringLiteral("_beat_period")));
    connect(m_pControlBeatPeriod.get(),
            &ControlObject::valueChanged,
            this,
            &EffectKnobParameterSlot::slotBeatPeriodChanged);
    // Follow changes from the knob, controllers and the meta knob
    connect(m_pControlValue.get(),
            &ControlObject::valueChanged,
            this,
            [this]() {
                updateBeatPeriod();
            });
    clear();
}

EffectKnobParameterSlot::~EffectKnobParameterSlot() = default;

void EffectKnobParameterSlot::loadParameter(EffectParameterPointer pEffectParameter) {
    clear();

    VERIFY_OR_DEBUG_ASSERT(pEffectParameter->manifest()->parameterType() ==
            EffectParameterType::Knob) {
        return;
    }

    m_pEffectParameter = pEffectParameter;

    if (m_pEffectParameter) {
        m_pManifestParameter = m_pEffectParameter->manifest();

        EffectManifestParameter::ValueScaler type = m_pManifestParameter->valueScaler();
        m_pControlValue->setBehaviour(type,
                m_pManifestParameter->getMinimum(),
                m_pManifestParameter->getMaximum());
        m_pControlValue->setDefaultValue(m_pManifestParameter->getDefault());
        m_pControlValue->set(m_pEffectParameter->getValue());
        // TODO(rryan) expose this from EffectParameter
        m_pControlType->forceSet(static_cast<double>(type));
        // Default loaded parameters to loaded and unlinked
        m_pControlLoaded->forceSet(1.0);

        m_pControlLinkType->set(
                static_cast<double>(pEffectParameter->linkType()));
        m_pControlLinkInverse->set(
                static_cast<double>(pEffectParameter->linkInversion()));
        m_pControlUnits->forceSet(static_cast<double>(m_pManifestParameter->unitsHint()));
        updateBeatPeriod();
    }

    emit updated();
}

void EffectKnobParameterSlot::clear() {
    if (m_pEffectParameter) {
        m_pEffectParameter = nullptr;
        m_pManifestParameter.clear();
    }

    m_pControlLoaded->forceSet(0.0);
    m_pControlValue->set(0.0);
    m_pControlValue->setDefaultValue(0.0);
    m_pControlType->forceSet(0.0);
    m_pControlLinkType->setAndConfirm(
            static_cast<double>(EffectManifestParameter::LinkType::None));
    m_metaknobSoftTakeover.setThreshold(SoftTakeover::kDefaultTakeoverThreshold);
    m_pControlLinkInverse->set(0.0);
    m_pControlUnits->forceSet(0.0);
    m_pControlBeatPeriod->set(0.0);
    emit updated();
}

bool EffectKnobParameterSlot::hasBeatPeriod() const {
    return m_pEffectParameter && m_pManifestParameter &&
            m_pManifestParameter->unitsHint() == EffectManifestParameter::UnitsHint::Beats;
}

bool EffectKnobParameterSlot::beatPeriodIsInverse() const {
    // Most Beats-typed parameters are periods (Echo/Glitch delay time,
    // Phaser LFO period), but the Tremolo rate is given in cycles per beat.
    return m_pManifestParameter && m_pManifestParameter->id() == QStringLiteral("rate");
}

void EffectKnobParameterSlot::updateBeatPeriod() {
    if (!hasBeatPeriod()) {
        m_pControlBeatPeriod->set(0.0);
        return;
    }
    const double value = m_pEffectParameter->getValue();
    if (beatPeriodIsInverse()) {
        m_pControlBeatPeriod->set(value > 0 ? 1.0 / value : 0.0);
    } else {
        m_pControlBeatPeriod->set(value);
    }
}

void EffectKnobParameterSlot::slotBeatPeriodChanged(double periodBeats) {
    if (!hasBeatPeriod()) {
        m_pControlBeatPeriod->set(0.0);
        return;
    }
    double value = periodBeats;
    if (beatPeriodIsInverse()) {
        value = periodBeats > 0 ? 1.0 / periodBeats : m_pManifestParameter->getMaximum();
    }
    value = math_clamp(value,
            m_pManifestParameter->getMinimum(),
            m_pManifestParameter->getMaximum());
    m_pEffectParameter->setValue(value);
    m_pControlValue->set(m_pEffectParameter->getValue());
    emit valueChanged(m_pEffectParameter->getValue());
    // Reflect the value the effect actually uses, e.g. after clamping
    updateBeatPeriod();
}

void EffectKnobParameterSlot::setParameter(double value) {
    m_pControlValue->setParameterFrom(value, this);
}

void EffectKnobParameterSlot::slotLinkTypeChanging(double v) {
    m_metaknobSoftTakeover.ignoreNext();
    EffectManifestParameter::LinkType newType =
            static_cast<EffectManifestParameter::LinkType>(
                    static_cast<int>(v));
    if (newType == EffectManifestParameter::LinkType::LinkedLeft ||
            newType == EffectManifestParameter::LinkType::LinkedRight ||
            newType == EffectManifestParameter::LinkType::LinkedLeftRight) {
        double neutral = m_pManifestParameter->neutralPointOnScale();
        if (neutral > 0.0 && neutral < 1.0) {
            // Knob is already a split knob, meaning it has a positive and
            // negative effect if it's twisted above the neutral point or
            // below the neutral point.
            // Toggle back to 0
            newType = EffectManifestParameter::LinkType::None;
        }
    }
    if (newType == EffectManifestParameter::LinkType::LinkedLeft ||
            newType == EffectManifestParameter::LinkType::LinkedRight) {
        m_metaknobSoftTakeover.setThreshold(
                SoftTakeover::kDefaultTakeoverThreshold * 2.0);
    } else {
        m_metaknobSoftTakeover.setThreshold(SoftTakeover::kDefaultTakeoverThreshold);
    }
    m_pControlLinkType->setAndConfirm(static_cast<double>(newType));
    m_pEffectParameter->setLinkType(newType);
}

void EffectKnobParameterSlot::slotLinkInverseChanged(double v) {
    Q_UNUSED(v);
    m_metaknobSoftTakeover.ignoreNext();
    m_pEffectParameter->setLinkInversion(
            static_cast<EffectManifestParameter::LinkInversion>(
                    static_cast<int>(v)));
}

void EffectKnobParameterSlot::onEffectMetaParameterChanged(double parameter, bool force) {
    m_dChainParameter = parameter;
    if (m_pEffectParameter == nullptr) {
        return;
    }
    // Intermediate cast to integer is needed for VC++.
    EffectManifestParameter::LinkType type =
            static_cast<EffectManifestParameter::LinkType>(
                    static_cast<int>(m_pControlLinkType->get()));

    bool inverse = m_pControlLinkInverse->toBool();
    double neutral = m_pManifestParameter->neutralPointOnScale();

    switch (type) {
    case EffectManifestParameter::LinkType::Linked:
        if (parameter < 0.0 || parameter > 1.0) {
            return;
        }
        if (neutral > 0.0 && neutral < 1.0) {
            if (inverse) {
                // the neutral position must stick where it is
                neutral = 1.0 - neutral;
            }
            // Knob is already a split knob
            // Match to center position of meta knob
            if (parameter <= 0.5) {
                parameter /= 0.5;
                parameter *= neutral;
            } else {
                parameter -= 0.5;
                parameter /= 0.5;
                parameter *= 1 - neutral;
                parameter += neutral;
            }
        }
        break;
    case EffectManifestParameter::LinkType::LinkedLeft:
        if (parameter >= 0.5 && parameter <= 1.0) {
            parameter = 1;
        } else if (parameter >= 0.0 && parameter <= 0.5) {
            parameter *= 2;
        } else {
            return;
        }
        break;
    case EffectManifestParameter::LinkType::LinkedRight:
        if (parameter >= 0.5 && parameter <= 1.0) {
            parameter -= 0.5;
            parameter *= 2;
        } else if (parameter >= 0.0 && parameter < 0.5) {
            parameter = 0.0;
        } else {
            return;
        }
        break;
    case EffectManifestParameter::LinkType::LinkedLeftRight:
        if (parameter >= 0.5 && parameter <= 1.0) {
            parameter -= 0.5;
            parameter *= 2;
        } else if (parameter >= 0.0 && parameter < 0.5) {
            parameter *= 2;
            parameter = 1.0 - parameter;
        } else {
            return;
        }
        break;
    case EffectManifestParameter::LinkType::None:
    default:
        return;
    }

    if (inverse) {
        parameter = 1.0 - parameter;
    }

    // qDebug() << "onEffectMetaParameterChanged" << debugString() << parameter
    // << "force?" << force;
    if (force) {
        m_pControlValue->setParameterFrom(parameter, nullptr);
        // This ensures that softtakover is in sync for following updates
        m_metaknobSoftTakeover.ignore(*m_pControlValue, parameter);
    } else if (!m_metaknobSoftTakeover.ignore(*m_pControlValue, parameter)) {
        m_pControlValue->setParameterFrom(parameter, nullptr);
    }
}

void EffectKnobParameterSlot::syncSofttakeover() {
    double parameter = m_pControlValue->getParameter();
    m_metaknobSoftTakeover.ignore(*m_pControlValue, parameter);
}

double EffectKnobParameterSlot::getValueParameter() const {
    return m_pControlValue->getParameter();
}
