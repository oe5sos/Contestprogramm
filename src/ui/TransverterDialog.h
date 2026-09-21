#pragma once

#include "core/Transverter.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;

namespace Contestprogramm {

// Datei > Transverter…: which band the rig shows and which band is on
// the antenna, with the LO offset derived from the pair (editable),
// and the switch. See core/Transverter.h. Its own small dialog rather
// than a row in the settings dialog: the operator does not know the
// transverter yet (2026-09-21) and will set it up at the site.
class TransverterDialog : public QDialog {
    Q_OBJECT

public:
    explicit TransverterDialog(const TransverterSetup& initial, QWidget* parent = nullptr);

    TransverterSetup setup() const;

    // Tests: the offset the dialog currently shows, Hz.
    qint64 offsetHz() const;
    void selectBands(const QString& ifBand, const QString& rfBand);

private:
    void refreshOffsetFromBands();
    void refreshSummary();

    QCheckBox* m_enabledCheck = nullptr;
    QComboBox* m_ifBandCombo = nullptr;
    QComboBox* m_rfBandCombo = nullptr;
    QDoubleSpinBox* m_offsetSpin = nullptr;
    QLabel* m_summaryLabel = nullptr;
    bool m_offsetEdited = false;
};

} // namespace Contestprogramm
