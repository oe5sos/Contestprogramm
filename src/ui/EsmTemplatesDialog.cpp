#include "ui/EsmTemplatesDialog.h"

#include "ui/StyleKit.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace Contestprogramm {

EsmTemplatesDialog::EsmTemplatesDialog(const EsmTemplates& templates, QWidget* parent)
    : QDialog(parent)
    , m_cqEdit(new QLineEdit(templates.cq, this))
    , m_runExchangeEdit(new QLineEdit(templates.runExchange, this))
    , m_tuEdit(new QLineEdit(templates.tu, this))
    , m_myCallEdit(new QLineEdit(templates.myCall, this))
    , m_spExchangeEdit(new QLineEdit(templates.spExchange, this))
{
    setWindowTitle(QStringLiteral("Contestprogramm - ESM-Texte (Enter sendet)"));
    setModal(true);

    auto* hint = new QLabel(QStringLiteral("Platzhalter: {call} = Gegenstation, {exchange} = eigener Exchange, "
                                           "{mycall} = eigenes Rufzeichen. Nur CW (Tastung über rigctld)."),
                            this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary()));

    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(QStringLiteral("Run, Call leer -- CQ:"), m_cqEdit);
    form->addRow(QStringLiteral("Run, Call getippt -- Exchange:"), m_runExchangeEdit);
    form->addRow(QStringLiteral("Run, Exchange komplett -- TU + loggen:"), m_tuEdit);
    form->addRow(QStringLiteral("S&&P, Call leer/getippt -- eigener Call:"), m_myCallEdit);
    form->addRow(QStringLiteral("S&&P, Exchange komplett -- Exchange + loggen:"), m_spExchangeEdit);
    for (QLineEdit* edit : {m_cqEdit, m_runExchangeEdit, m_tuEdit, m_myCallEdit, m_spExchangeEdit}) {
        edit->setFont(Style::monoFont(edit->font(), Style::kFontBody));
        edit->setMinimumWidth(320);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(hint);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

EsmTemplates EsmTemplatesDialog::templates() const
{
    EsmTemplates t;
    t.cq = m_cqEdit->text().trimmed();
    t.runExchange = m_runExchangeEdit->text().trimmed();
    t.tu = m_tuEdit->text().trimmed();
    t.myCall = m_myCallEdit->text().trimmed();
    t.spExchange = m_spExchangeEdit->text().trimmed();
    return t;
}

} // namespace Contestprogramm
