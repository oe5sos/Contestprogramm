#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;
class QSpinBox;

namespace Contestprogramm {

struct ContestSettings;

// Datei > Online-Scoreboard...: the board's URL, account and cadence,
// plus the contest id the board knows the contest by. A small window
// of its own rather than a SettingsDialog group -- see
// EsmTemplatesDialog for the same reasoning.
class ScoreboardDialog : public QDialog {
    Q_OBJECT

public:
    explicit ScoreboardDialog(const ContestSettings& settings, QWidget* parent = nullptr);

    // Writes the edited values into `settings` (only the scoreboard fields).
    void applyTo(ContestSettings& settings) const;

private:
    QCheckBox* m_enabledCheck;
    QLineEdit* m_urlEdit;
    QLineEdit* m_userEdit;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_contestNameEdit;
    QSpinBox* m_intervalSpin;
};

} // namespace Contestprogramm
