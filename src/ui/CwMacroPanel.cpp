#include "ui/CwMacroPanel.h"

#include <QHBoxLayout>
#include <QPushButton>

#include <utility>

namespace Contestprogramm {

namespace {
constexpr int kMaxButtons = 6;
}

CwMacroPanel::CwMacroPanel(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QHBoxLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
}

void CwMacroPanel::setMacroTemplates(const QStringList& templates)
{
    m_templates = templates.mid(0, kMaxButtons);
    rebuildButtons();
}

void CwMacroPanel::activateMacro(int index)
{
    if (index < 0 || index >= m_templates.size()) {
        return;
    }
    emit macroActivated(m_templates.at(index));
}

void CwMacroPanel::rebuildButtons()
{
    for (QPushButton* button : std::as_const(m_buttons)) {
        m_layout->removeWidget(button);
        button->deleteLater();
    }
    m_buttons.clear();
    if (m_stopButton) {
        m_layout->removeWidget(m_stopButton);
        m_stopButton->deleteLater();
        m_stopButton = nullptr;
    }

    for (int i = 0; i < m_templates.size(); ++i) {
        const QString templateText = m_templates.at(i);
        // The label IS the shortcut: F1..F6 in MainWindow call
        // activateMacro(i), so the button reads like the key.
        auto* button = new QPushButton(QStringLiteral("F%1").arg(i + 1), this);
        button->setToolTip(templateText);
        connect(button, &QPushButton::clicked, this, [this, templateText]() {
            emit macroActivated(templateText);
        });
        m_layout->addWidget(button);
        m_buttons.append(button);
    }
    if (!m_templates.isEmpty()) {
        m_stopButton = new QPushButton(QStringLiteral("\u25a0 Esc"), this);
        m_stopButton->setToolTip(QStringLiteral("Tastung abbrechen (Esc)"));
        connect(m_stopButton, &QPushButton::clicked, this, &CwMacroPanel::stopRequested);
        m_layout->addWidget(m_stopButton);
    }
}

QString CwMacroPanel::substitute(const QString& templateText, const QString& callsign, const QString& exchange)
{
    QString result = templateText;
    result.replace(QStringLiteral("{call}"), callsign);
    result.replace(QStringLiteral("{exchange}"), exchange);
    return result;
}

} // namespace Contestprogramm
