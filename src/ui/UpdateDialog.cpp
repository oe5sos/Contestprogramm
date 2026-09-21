#include "ui/UpdateDialog.h"

#include "BuildInfo.h"
#include "ui/PanelHeaderBar.h"
#include "ui/StyleKit.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

namespace Contestprogramm {

namespace {
const QString kRepository = QStringLiteral("oe5sos/Contestprogramm");
const QString kReleasesPage = QStringLiteral("https://github.com/oe5sos/Contestprogramm/releases");
} // namespace

UpdateDialog::UpdateDialog(QWidget* parent)
    : QDialog(parent)
    , m_updater(new SelfUpdater(this))
{
    setWindowTitle(QStringLiteral("Auf neueste Version aktualisieren"));
    m_updater->setRepository(kRepository);
    m_updater->setCurrentVersion(QVersionNumber::fromString(QStringLiteral(CONTESTPROGRAMM_VERSION)));
    m_updater->setTarget(UpdateTarget::detect());

    auto* header = new PanelHeaderBar(QStringLiteral("Auf neueste Version aktualisieren"), this);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::RichText);
    m_status->setOpenExternalLinks(true);

    m_notes = new QTextBrowser(this);
    m_notes->setOpenExternalLinks(true);
    m_notes->setMinimumHeight(140);
    m_notes->hide();

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 0);
    m_progress->setTextVisible(false);

    m_primary = new QPushButton(this);
    m_primary->setDefault(true);
    m_primary->hide();
    connect(m_primary, &QPushButton::clicked, this, &UpdateDialog::startDownload);
    m_page = new QPushButton(QStringLiteral("Release-Seite öffnen"), this);
    connect(m_page, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(m_offer.pageUrl.isValid() ? m_offer.pageUrl : QUrl(kReleasesPage));
    });
    m_close = new QPushButton(QStringLiteral("Schließen"), this);
    connect(m_close, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_updater, &SelfUpdater::updateAvailable, this, &UpdateDialog::onUpdateAvailable);
    connect(m_updater, &SelfUpdater::upToDate, this, [this](const QVersionNumber& current) {
        setStep(Step::UpToDate,
                QStringLiteral("<b>Contestprogramm %1 ist aktuell.</b><br/>Es gibt keine neuere Version auf GitHub.")
                    .arg(current.toString()));
    });
    connect(m_updater, &SelfUpdater::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (total > 0) {
            m_progress->setRange(0, 1000);
            m_progress->setValue(static_cast<int>(received * 1000 / total));
        }
        m_status->setText(QStringLiteral("Lade %1 herunter … %2 von %3")
                              .arg(m_offer.assetName, QLocale().formattedDataSize(received, 1, QLocale::DataSizeSIFormat),
                                   total > 0 ? QLocale().formattedDataSize(total, 1, QLocale::DataSizeSIFormat)
                                             : QStringLiteral("?")));
    });
    connect(m_updater, &SelfUpdater::downloaded, this, &UpdateDialog::onDownloaded);
    connect(m_updater, &SelfUpdater::failed, this, &UpdateDialog::onFailed);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    auto* body = new QVBoxLayout();
    body->setContentsMargins(14, 12, 14, 12);
    body->setSpacing(10);
    body->addWidget(m_status);
    body->addWidget(m_notes, 1);
    body->addWidget(m_progress);
    auto* footer = new QHBoxLayout();
    footer->addWidget(m_page);
    footer->addStretch();
    footer->addWidget(m_close);
    footer->addWidget(m_primary);
    body->addLayout(footer);
    layout->addLayout(body);
    resize(560, 260);
}

void UpdateDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (m_started) {
        return;
    }
    m_started = true;
    setStep(Step::Checking, QStringLiteral("Suche nach der neuesten Version auf GitHub …"));
    m_updater->check();
}

void UpdateDialog::reject()
{
    if (m_step == Step::Installing || m_step == Step::Done) {
        return; // the files are being swapped -- no way out now
    }
    m_updater->cancel();
    QDialog::reject();
}

void UpdateDialog::setStep(Step step, const QString& message)
{
    m_step = step;
    m_status->setText(message);
    const bool busy = step == Step::Checking || step == Step::Downloading || step == Step::Installing;
    m_progress->setVisible(busy);
    if (step == Step::Checking || step == Step::Installing) {
        m_progress->setRange(0, 0);
    }
    m_notes->setVisible(step == Step::Offer && !m_offer.notes.trimmed().isEmpty());
    m_close->setText(step == Step::Downloading ? QStringLiteral("Abbrechen") : QStringLiteral("Schließen"));
    m_close->setEnabled(step != Step::Installing && step != Step::Done);
    m_primary->setVisible(step == Step::Offer);
    if (step == Step::Offer) {
        m_primary->setText(QStringLiteral("Herunterladen und installieren"));
        m_primary->setFocus();
    }
}

void UpdateDialog::onUpdateAvailable(const ReleaseInfo& info)
{
    m_offer = info;
    const UpdateTarget& target = m_updater->target();
    QString text = QStringLiteral("<b>Version %1 ist da</b> (installiert: %2).")
                       .arg(info.version.toString(), QStringLiteral(CONTESTPROGRAMM_VERSION));
    if (!target.usable()) {
        text += QStringLiteral("<br/>Selbst aktualisieren kann sich diese Kopie nicht: %1.<br/>"
                               "Das Paket gibt es auf der Release-Seite.")
                    .arg(target.reason);
        m_notes->setMarkdown(info.notes);
        setStep(Step::Failed, text);
        m_notes->setVisible(!info.notes.trimmed().isEmpty());
        return;
    }
    text += QStringLiteral("<br/>%1, %2 – wird heruntergeladen, geprüft und eingesetzt; danach startet "
                           "Contestprogramm neu.")
                .arg(info.assetName, QLocale().formattedDataSize(info.assetSize, 1, QLocale::DataSizeSIFormat));
    m_notes->setMarkdown(info.notes);
    setStep(Step::Offer, text);
    resize(width(), m_notes->isVisible() ? 420 : 260);
}

void UpdateDialog::startDownload()
{
    m_progress->setRange(0, 0);
    setStep(Step::Downloading, QStringLiteral("Lade %1 herunter …").arg(m_offer.assetName));
    m_updater->download(m_offer);
}

void UpdateDialog::onDownloaded(const QString& packagePath, bool verified)
{
    setStep(Step::Installing,
            verified ? QStringLiteral("Prüfsumme stimmt. Setze die neue Version ein …")
                     : QStringLiteral("Setze die neue Version ein … (das Release trägt keine Prüfsumme)"));
    // Let the label paint before the copy blocks the thread.
    QTimer::singleShot(50, this, [this, packagePath] {
        QString error;
        if (!m_updater->install(packagePath, &error)) {
            onFailed(error);
            return;
        }
        setStep(Step::Done, QStringLiteral("<b>Version %1 ist eingesetzt.</b><br/>Contestprogramm startet neu …")
                                .arg(m_offer.version.toString()));
        QTimer::singleShot(1500, this, [this] { m_updater->restart(); });
    });
}

void UpdateDialog::onFailed(const QString& message)
{
    setStep(Step::Failed, QStringLiteral("<b>Das hat nicht geklappt.</b><br/>%1").arg(message.toHtmlEscaped()));
}

} // namespace Contestprogramm
