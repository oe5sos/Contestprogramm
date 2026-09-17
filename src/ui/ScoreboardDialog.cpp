#include "ui/ScoreboardDialog.h"

#include "app/ContestSettings.h"
#include "ui/StyleKit.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Contestprogramm {

ScoreboardDialog::ScoreboardDialog(const ContestSettings& settings, QWidget* parent)
    : QDialog(parent)
    , m_enabledCheck(new QCheckBox(QStringLiteral("Stand regelmäßig senden"), this))
    , m_urlEdit(new QLineEdit(settings.scoreboardUrl, this))
    , m_userEdit(new QLineEdit(settings.scoreboardUsername, this))
    , m_passwordEdit(new QLineEdit(settings.scoreboardPassword, this))
    , m_contestNameEdit(new QLineEdit(settings.scoreboardContestName, this))
    , m_intervalSpin(new QSpinBox(this))
{
    setWindowTitle(QStringLiteral("Contestprogramm - Online-Scoreboard"));
    setModal(true);

    m_enabledCheck->setChecked(settings.scoreboardEnabled);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_intervalSpin->setRange(1, 60);
    m_intervalSpin->setSuffix(QStringLiteral(" min"));
    m_intervalSpin->setValue(settings.scoreboardIntervalMinutes);
    m_urlEdit->setPlaceholderText(QStringLiteral("https://…/post/"));
    m_contestNameEdit->setPlaceholderText(QStringLiteral("Contest-Kennung des Scoreboards, z.B. IARU-R1-VHF"));

    auto* hint = new QLabel(QStringLiteral("Sendet alle paar Minuten den Punktestand als Contest-Online-Score-XML "
                                           "(dasselbe Format wie N1MM+/DXLog.net) per HTTP POST mit Basic-Auth. "
                                           "Hat keine Wirkung auf den Contest selbst."),
                            this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary()));

    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(QString(), m_enabledCheck);
    form->addRow(QStringLiteral("URL:"), m_urlEdit);
    form->addRow(QStringLiteral("Benutzer:"), m_userEdit);
    form->addRow(QStringLiteral("Passwort:"), m_passwordEdit);
    form->addRow(QStringLiteral("Contest-Kennung:"), m_contestNameEdit);
    form->addRow(QStringLiteral("Intervall:"), m_intervalSpin);
    m_urlEdit->setMinimumWidth(360);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void ScoreboardDialog::applyTo(ContestSettings& settings) const
{
    settings.scoreboardEnabled = m_enabledCheck->isChecked();
    settings.scoreboardUrl = m_urlEdit->text().trimmed();
    settings.scoreboardUsername = m_userEdit->text().trimmed();
    settings.scoreboardPassword = m_passwordEdit->text();
    settings.scoreboardContestName = m_contestNameEdit->text().trimmed();
    settings.scoreboardIntervalMinutes = m_intervalSpin->value();
}

} // namespace Contestprogramm
