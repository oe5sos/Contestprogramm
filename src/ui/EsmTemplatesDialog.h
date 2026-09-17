#pragma once

#include "core/EsmPlanner.h"

#include <QDialog>

class QLineEdit;

namespace Contestprogramm {

// The five ESM texts (see core/EsmPlanner.h), editable in one small
// window off the Datei menu -- kept out of SettingsDialog on purpose:
// these are keyed on the air, and an operator wants to fix a text
// between two QSOs without wading through rotor ports.
class EsmTemplatesDialog : public QDialog {
    Q_OBJECT

public:
    explicit EsmTemplatesDialog(const EsmTemplates& templates, QWidget* parent = nullptr);

    EsmTemplates templates() const;

private:
    QLineEdit* m_cqEdit;
    QLineEdit* m_runExchangeEdit;
    QLineEdit* m_tuEdit;
    QLineEdit* m_myCallEdit;
    QLineEdit* m_spExchangeEdit;
};

} // namespace Contestprogramm
