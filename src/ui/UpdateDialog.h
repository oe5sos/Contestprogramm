#pragma once

#include "app/SelfUpdater.h"

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;
class QTextBrowser;

namespace Contestprogramm {

// Hilfe > Auf neueste Version aktualisieren: one window that walks the
// operator from "looking" through "version X is there" and the
// download to the restart, with a way out at every step until the
// files are being swapped.
class UpdateDialog : public QDialog {
    Q_OBJECT

public:
    explicit UpdateDialog(QWidget* parent = nullptr);

    // Starts the check as soon as the window is up.
    void showEvent(QShowEvent* event) override;
    // Escape/Schließen/Abbrechen: stops a check or download; ignored
    // while the files are being swapped.
    void reject() override;

private:
    enum class Step { Checking, UpToDate, Offer, Downloading, Installing, Done, Failed };
    void setStep(Step step, const QString& message);
    void onUpdateAvailable(const ReleaseInfo& info);
    void onDownloaded(const QString& packagePath, bool verified);
    void onFailed(const QString& message);
    void startDownload();

    SelfUpdater* m_updater = nullptr;
    ReleaseInfo m_offer;
    Step m_step = Step::Checking;
    bool m_started = false;
    QLabel* m_status = nullptr;
    QTextBrowser* m_notes = nullptr;
    QProgressBar* m_progress = nullptr;
    QPushButton* m_primary = nullptr;
    QPushButton* m_page = nullptr;
    QPushButton* m_close = nullptr;
};

} // namespace Contestprogramm
