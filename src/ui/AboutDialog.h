#pragma once

#include <QDialog>
#include <QString>

namespace Contestprogramm {

// Hilfe > Über Contestprogramm…: version, commit and build date (from
// the generated BuildInfo.h), the Qt it runs on, and where the data
// lives -- database, backups, second backup folder -- with a button to
// show the data folder. The first thing to ask for when something is
// reported: "which build, which folder?"
class AboutDialog : public QDialog {
    Q_OBJECT

public:
    struct Facts {
        QString databasePath;
        QString backupDirectory;
        QString mirrorDirectory;
    };
    explicit AboutDialog(const Facts& facts, QWidget* parent = nullptr);

    // "Contestprogramm 0.1.0 (e1f6f93a1b+, 2026-09-21)"
    static QString versionLine();
    // The whole text as shown, for tests.
    QString text() const { return m_text; }

private:
    QString m_text;
};

} // namespace Contestprogramm
