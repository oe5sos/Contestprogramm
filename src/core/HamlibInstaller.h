#pragma once

// =================================================================
// src/core/HamlibInstaller.h  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original -- not a Thetis port. Ported verbatim
// (behavior, safety guards and constants unchanged) from
// Longpath/NereusSDR's own src/core/HamlibInstaller.h/.cpp, which is
// itself NereusSDR-original -- see that file's own header for the
// full rationale. Only the namespace changed, `Longpath` ->
// `Contestprogramm`.
//
// Find Hamlib, say what is there, and install it if the operator asks.
//
// The rotor link needs rotctld, which comes from Hamlib, which is not
// part of this program. Until now the answer to "Hamlib is not
// installed" was a sentence telling the operator to open a terminal.
// That is the point where a feature stops being used.
//
// What this will and will not do, and why the line is where it is:
//
//   It runs `brew install hamlib`, and only that. Homebrew is the
//   operator's own package manager, installing from their own configured
//   taps; the command is shown in full before it runs and its output is
//   shown while it runs. Nothing is downloaded by this program, and no
//   URL is fetched.
//
//   It never uses sudo, and there is a test that says so. A program that
//   asks for an administrator password to install an optional extra has
//   asked for far more than it needs, and an operator who types the
//   password into a dialog like that has learned a habit worth not
//   teaching. Homebrew installs into a prefix the user already owns.
//
//   Where there is no Homebrew — Linux, or macOS without it — this shows
//   the exact command for the platform and stops. Those need sudo, so
//   they are the operator's to run.
//
// The other half is knowing what is installed. Hamlib's model numbers
// move between releases, so `rotctl --list` is read and the operator's
// chosen number checked against it. Picking 404 on a Hamlib too old to
// have the ERC driver otherwise fails as "rotctld exited", which reads
// as a broken cable rather than as a wrong number.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-11 — Ported from Longpath/NereusSDR's HamlibInstaller for
//                 Contestprogramm. AI-assisted via Anthropic Claude
//                 Code, operator Ralph Martin Fischer.
// =================================================================

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Contestprogramm {

// One row of `rotctl --list`.
struct HamlibRotorEntry {
    int     model{0};
    QString manufacturer;
    QString name;
    QString status;        // Stable / Beta / Alpha / Untested
};

class HamlibInstaller : public QObject {
    Q_OBJECT
public:
    explicit HamlibInstaller(QObject* parent = nullptr);
    ~HamlibInstaller() override;

    // ── Finding what is there ────────────────────────────────────────

    // Homebrew's own binary, or empty. Same problem as rotctld: a GUI
    // launched from Finder has no /opt/homebrew/bin on its PATH.
    static QString findBrew();

    // True when Hamlib is present and usable.
    static bool isInstalled();

    // "4.6.2", or empty if rotctl cannot be run. Parsed from
    // `rotctl --version`.
    static QString installedVersion();
    static QString parseVersion(const QString& versionOutput);

    // Every rotator the installed Hamlib knows, from `rotctl --list`.
    // Empty when Hamlib is missing — an empty list is not an assertion
    // that no rotators exist, and callers must not treat it as one.
    static QVector<HamlibRotorEntry> supportedModels();
    static QVector<HamlibRotorEntry> parseModelList(const QString& listOutput);

    // ── Installing ───────────────────────────────────────────────────

    // The command, built where it can be read and tested. Empty when
    // there is nothing this program is willing to run unattended.
    static QStringList installCommand();

    // What to tell an operator who has no Homebrew: the command for
    // their platform, to run themselves.
    static QString manualInstructions();

    // True when installCommand() has something to run.
    static bool canInstallAutomatically();

    bool isRunning() const;

    // Start the install. Returns false immediately if there is nothing
    // to run; everything else arrives through the signals.
    bool install(QString* error);
    void cancel();

signals:
    // A line of Homebrew's output, stdout and stderr together in the
    // order they arrived. Shown live: `brew install` can take minutes,
    // and a dialog that sits silent for minutes looks hung.
    void output(const QString& line);
    void finished(bool ok, const QString& message);

private:
    void drain();

    QProcess m_proc;
    QString  m_carry;      // partial line held over between reads
};

} // namespace Contestprogramm
