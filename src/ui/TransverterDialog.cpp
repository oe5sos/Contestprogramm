#include "ui/TransverterDialog.h"

#include "core/BandUtils.h"
#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

namespace Contestprogramm {

TransverterDialog::TransverterDialog(const TransverterSetup& initial, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Contestprogramm - Transverter"));
    auto* header = new PanelHeaderBar(QStringLiteral("Transverter"), this);

    auto* intro = new QLabel(
        QStringLiteral("Das Funkgerät meldet über CAT nur seine Zwischenfrequenz. Mit einem Transverter "
                       "rechnet das Programm daraus die Frequenz auf der Antenne: Band im Log, Rotor, "
                       "Bandmap und QSY stimmen dann. Der Schalter „Transverter“ in der Kopfzeile "
                       "sagt, ob er gerade dran ist."),
        this);
    intro->setWordWrap(true);
    intro->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary()));

    m_enabledCheck = new QCheckBox(QStringLiteral("Transverter verwenden (Schalter, auch in der Kopfzeile)"), this);
    m_enabledCheck->setChecked(initial.enabled);

    m_ifBandCombo = new QComboBox(this);
    m_rfBandCombo = new QComboBox(this);
    for (const QString& band : knownBands()) {
        m_ifBandCombo->addItem(QStringLiteral("%1 MHz").arg(band), band);
        m_rfBandCombo->addItem(QStringLiteral("%1 MHz").arg(band), band);
    }
    m_offsetSpin = new QDoubleSpinBox(this);
    m_offsetSpin->setRange(-20000.0, 20000.0);
    m_offsetSpin->setDecimals(3);
    m_offsetSpin->setSuffix(QStringLiteral(" MHz"));
    m_offsetSpin->setLocale(QLocale::c());
    m_offsetSpin->setToolTip(QStringLiteral("Lokaloszillator: Antenne = Zwischenfrequenz + Offset. Wird aus den Bändern "
                                            "vorgeschlagen (1296 − 144 = 1152 MHz) und lässt sich überschreiben."));

    selectBands(initial.ifBand.isEmpty() ? QStringLiteral("144") : initial.ifBand,
                initial.rfBand.isEmpty() ? QStringLiteral("1296") : initial.rfBand);
    if (initial.configured()) {
        m_offsetSpin->setValue(initial.offsetHz / 1e6);
        m_offsetEdited = initial.offsetHz != TransverterSetup::defaultOffsetHz(initial.ifBand, initial.rfBand);
    } else {
        refreshOffsetFromBands();
    }

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kAmberText()));

    connect(m_ifBandCombo, &QComboBox::currentIndexChanged, this, [this] {
        m_offsetEdited = false;
        refreshOffsetFromBands();
    });
    connect(m_rfBandCombo, &QComboBox::currentIndexChanged, this, [this] {
        m_offsetEdited = false;
        refreshOffsetFromBands();
    });
    connect(m_offsetSpin, &QDoubleSpinBox::valueChanged, this, [this] {
        m_offsetEdited = true;
        refreshSummary();
    });
    connect(m_enabledCheck, &QCheckBox::toggled, this, [this] { refreshSummary(); });

    auto* form = new QFormLayout();
    form->addRow(QStringLiteral("Funkgerät zeigt (ZF):"), m_ifBandCombo);
    form->addRow(QStringLiteral("Antenne (Band):"), m_rfBandCombo);
    form->addRow(QStringLiteral("Offset:"), m_offsetSpin);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Abbrechen"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    auto* body = new QVBoxLayout();
    body->setContentsMargins(14, 12, 14, 12);
    body->setSpacing(10);
    body->addWidget(intro);
    body->addWidget(m_enabledCheck);
    body->addLayout(form);
    body->addWidget(m_summaryLabel);
    body->addWidget(buttons);
    layout->addLayout(body);
    resize(560, 340);
    refreshSummary();
}

void TransverterDialog::selectBands(const QString& ifBand, const QString& rfBand)
{
    const int ifIndex = m_ifBandCombo->findData(ifBand);
    const int rfIndex = m_rfBandCombo->findData(rfBand);
    if (ifIndex >= 0) {
        m_ifBandCombo->setCurrentIndex(ifIndex);
    }
    if (rfIndex >= 0) {
        m_rfBandCombo->setCurrentIndex(rfIndex);
    }
}

void TransverterDialog::refreshOffsetFromBands()
{
    if (m_offsetEdited) {
        return;
    }
    const QSignalBlocker blocker(m_offsetSpin);
    m_offsetSpin->setValue(
        TransverterSetup::defaultOffsetHz(m_ifBandCombo->currentData().toString(), m_rfBandCombo->currentData().toString()) / 1e6);
    refreshSummary();
}

void TransverterDialog::refreshSummary()
{
    if (!m_summaryLabel) {
        return;
    }
    const TransverterSetup current = setup();
    if (!current.configured()) {
        m_summaryLabel->setText(QStringLiteral("Zwei verschiedene Bänder wählen."));
        return;
    }
    // A worked example, so the direction of the offset is never in doubt.
    const qint64 exampleRig = bandBaseHz(current.ifBand) + 300000;
    m_summaryLabel->setText(QStringLiteral("%1 — Gerät auf %2 MHz heißt %3 MHz auf der Antenne%4")
                                .arg(current.describe())
                                .arg(exampleRig / 1e6, 0, 'f', 3)
                                .arg((exampleRig + current.offsetHz) / 1e6, 0, 'f', 3)
                                .arg(current.enabled ? QString() : QStringLiteral(" (derzeit aus)")));
}

TransverterSetup TransverterDialog::setup() const
{
    TransverterSetup result;
    result.enabled = m_enabledCheck->isChecked();
    result.ifBand = m_ifBandCombo->currentData().toString();
    result.rfBand = m_rfBandCombo->currentData().toString();
    result.offsetHz = qRound64(m_offsetSpin->value() * 1e6);
    if (result.ifBand == result.rfBand) {
        result.offsetHz = 0; // not configured
    }
    return result;
}

qint64 TransverterDialog::offsetHz() const
{
    return qRound64(m_offsetSpin->value() * 1e6);
}

} // namespace Contestprogramm
