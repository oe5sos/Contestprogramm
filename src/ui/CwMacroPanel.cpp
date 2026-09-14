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

void CwMacroPanel::rebuildButtons()
{
    for (QPushButton* button : std::as_const(m_buttons)) {
        m_layout->removeWidget(button);
        button->deleteLater();
    }
    m_buttons.clear();

    for (int i = 0; i < m_templates.size(); ++i) {
        const QString templateText = m_templates.at(i);
        auto* button = new QPushButton(QStringLiteral("F%1").arg(i + 1), this);
        button->setToolTip(templateText);
        connect(button, &QPushButton::clicked, this, [this, templateText]() {
            emit macroActivated(templateText);
        });
        m_layout->addWidget(button);
        m_buttons.append(button);
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
