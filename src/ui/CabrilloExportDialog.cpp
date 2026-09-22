#include "ui/CabrilloExportDialog.h"

#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {

QComboBox* choiceCombo(const QStringList& choices, const QString& current, const QString& objectName,
                       QWidget* parent)
{
    auto* combo = new QComboBox(parent);
    combo->setObjectName(objectName);
    combo->addItems(choices);
    const int index = combo->findText(current);
    // Ein gespeicherter Wert, den diese Fassung nicht anbietet, bleibt
    // stehen statt still auf den ersten Eintrag zu springen.
    if (index >= 0) {
        combo->setCurrentIndex(index);
    } else if (!current.isEmpty()) {
        combo->addItem(current);
        combo->setCurrentIndex(combo->count() - 1);
    }
    return combo;
}

QLabel* caption(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setFont(Style::capsFont(label->font()));
    label->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextScale()));
    return label;
}

} // namespace

CabrilloExportDialog::CabrilloExportDialog(const CabrilloCategories& initial, QWidget* parent)
    : QDialog(parent)
    , m_operator(choiceCombo(CabrilloCategories::operatorChoices(), initial.operatorCategory,
                             QStringLiteral("cabrilloOperator"), this))
    , m_assisted(choiceCombo(CabrilloCategories::assistedChoices(), initial.assisted,
                             QStringLiteral("cabrilloAssisted"), this))
    , m_power(choiceCombo(CabrilloCategories::powerChoices(), initial.power, QStringLiteral("cabrilloPower"), this))
    , m_transmitter(choiceCombo(CabrilloCategories::transmitterChoices(), initial.transmitter,
                                QStringLiteral("cabrilloTransmitter"), this))
    , m_station(choiceCombo(CabrilloCategories::stationChoices(), initial.station,
                            QStringLiteral("cabrilloStation"), this))
    , m_club(new QLineEdit(initial.club, this))
    , m_email(new QLineEdit(initial.email, this))
{
    setWindowTitle(QStringLiteral("Contestprogramm - Cabrillo exportieren"));
    setModal(true);
    m_club->setObjectName(QStringLiteral("cabrilloClub"));
    m_email->setObjectName(QStringLiteral("cabrilloEmail"));
    m_club->setPlaceholderText(QStringLiteral("leer: die Zeile entfällt"));
    m_email->setPlaceholderText(QStringLiteral("leer: die Zeile entfällt"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Datei wählen..."));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Abbrechen"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Wie die anderen echten Fenster: nur die Kopfzeile bekommt eigenes
    // Chrom, kein abgerundeter Rahmen auf dem Fenster selbst.
    auto* header = new PanelHeaderBar(QStringLiteral("Cabrillo exportieren"), this);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);

    auto* form = new QFormLayout();
    form->setContentsMargins(12, 10, 12, 6);
    form->setSpacing(8);
    form->addRow(caption(QStringLiteral("Bediener"), this), m_operator);
    form->addRow(caption(QStringLiteral("Hilfsmittel"), this), m_assisted);
    form->addRow(caption(QStringLiteral("Leistung"), this), m_power);
    form->addRow(caption(QStringLiteral("Sender"), this), m_transmitter);
    form->addRow(caption(QStringLiteral("Station"), this), m_station);
    form->addRow(caption(QStringLiteral("Club"), this), m_club);
    form->addRow(caption(QStringLiteral("E-Mail"), this), m_email);
    layout->addLayout(form);

    auto* note = new QLabel(QStringLiteral("Band und Betriebsart stehen nicht hier — die liest das Programm aus "
                                           "den geloggten QSOs."),
                            this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextTertiary()));
    auto* footer = new QVBoxLayout();
    footer->setContentsMargins(12, 0, 12, 12);
    footer->addWidget(note);
    footer->addWidget(buttons);
    layout->addLayout(footer);

    resize(420, 380);
}

CabrilloCategories CabrilloExportDialog::categories() const
{
    CabrilloCategories c;
    c.operatorCategory = m_operator->currentText();
    c.assisted = m_assisted->currentText();
    c.power = m_power->currentText();
    c.transmitter = m_transmitter->currentText();
    c.station = m_station->currentText();
    c.club = m_club->text().trimmed();
    c.email = m_email->text().trimmed();
    return c;
}

} // namespace Contestprogramm
