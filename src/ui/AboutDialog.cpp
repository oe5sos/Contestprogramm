#include "ui/AboutDialog.h"

#include "BuildInfo.h"
#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QtGlobal>

namespace Contestprogramm {

QString AboutDialog::versionLine()
{
    return QStringLiteral("Contestprogramm %1 (%2, %3)")
        .arg(QStringLiteral(CONTESTPROGRAMM_VERSION), QStringLiteral(CONTESTPROGRAMM_GIT_HASH),
             QStringLiteral(CONTESTPROGRAMM_BUILD_DATE));
}

AboutDialog::AboutDialog(const Facts& facts, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Über Contestprogramm"));
    auto* header = new PanelHeaderBar(QStringLiteral("Über Contestprogramm"), this);

    const QString dash = Style::unknownDash();
    const auto row = [](const QString& caption, const QString& value) {
        return QStringLiteral("<tr><td style=\"color:%1; padding-right:14px;\">%2</td><td>%3</td></tr>")
            .arg(Style::kTextSecondary(), caption, value.toHtmlEscaped());
    };
    m_text = QStringLiteral("<p style=\"font-size:%1px;\"><b>%2</b></p>").arg(Style::kFontBody).arg(versionLine())
        + QStringLiteral("<p>Contest-Logger für VHF/UHF, Ralph Martin Fischer, OE5SOS.<br/>"
                         "Entwickelt mit Anthropic Claude Code; siehe README und NOTICE.</p>")
        + QStringLiteral("<table>")
        + row(QStringLiteral("Qt"), QString::fromLatin1(qVersion()))
        + row(QStringLiteral("Datenbank"), facts.databasePath.isEmpty() ? dash : facts.databasePath)
        + row(QStringLiteral("Sicherungen"), facts.backupDirectory.isEmpty() ? dash : facts.backupDirectory)
        + row(QStringLiteral("Zweite Sicherung"), facts.mirrorDirectory.isEmpty() ? QStringLiteral("keine") : facts.mirrorDirectory)
        + QStringLiteral("</table>");

    auto* label = new QLabel(m_text, this);
    label->setTextFormat(Qt::RichText);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);

    auto* showFolder = new QPushButton(QStringLiteral("Datenordner zeigen"), this);
    showFolder->setEnabled(!facts.databasePath.isEmpty());
    connect(showFolder, &QPushButton::clicked, this, [facts] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(facts.databasePath).absolutePath()));
    });
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("Schließen"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    auto* body = new QVBoxLayout();
    body->setContentsMargins(14, 12, 14, 12);
    body->setSpacing(10);
    body->addWidget(label);
    auto* footer = new QHBoxLayout();
    footer->addWidget(showFolder);
    footer->addStretch();
    footer->addWidget(buttons);
    body->addLayout(footer);
    layout->addLayout(body);
    resize(620, 300);
}

} // namespace Contestprogramm
