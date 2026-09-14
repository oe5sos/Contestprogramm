// =================================================================
// src/core/HamlibInstaller.cpp  (Contestprogramm)
// =================================================================
//
// Contestprogramm-original -- ported verbatim from Longpath/NereusSDR's
// HamlibInstaller.cpp. See HamlibInstaller.h for what this will and
// will not run, and why.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-09-11 — Ported from Longpath/NereusSDR's HamlibInstaller for
//                 Contestprogramm. AI-assisted via Anthropic Claude
//                 Code, operator Ralph Martin Fischer.
// =================================================================

#include "core/HamlibInstaller.h"

#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

namespace Contestprogramm {

namespace {

// Run something and collect its output. Only ever used on rotctl and
// brew --version, both of which answer at once; the timeout is there so
// that a wedged binary cannot hang the dialog rather than because any
// of these is expected to be slow.
constexpr int kQueryTimeoutMs = 4000;

QString runCapture(const QString& program, const QStringList& args)
{
    if (program.isEmpty()) { return {}; }
    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(program, args);
    if (!p.waitForStarted(kQueryTimeoutMs)) { return {}; }
    if (!p.waitForFinished(kQueryTimeoutMs)) {
        p.kill();
        p.waitForFinished(500);
        return {};
    }
    return QString::fromUtf8(p.readAll());
}

QString findExecutable(const QString& name)
{
    const QString onPath = QStandardPaths::findExecutable(name);
    if (!onPath.isEmpty()) { return onPath; }

    // Same reasoning as RotctldProcess::findBinary(): a GUI application
    // launched from Finder gets a PATH without Homebrew in it, so the
    // directories have to be named.
    static const QStringList kDirs = {
        QStringLiteral("/opt/homebrew/bin"),   // Apple silicon Homebrew
        QStringLiteral("/usr/local/bin"),      // Intel Homebrew
        QStringLiteral("/opt/local/bin"),      // MacPorts
        QStringLiteral("/usr/bin"),
    };
    for (const QString& dir : kDirs) {
        const QString candidate = dir + QLatin1Char('/') + name;
        if (QFileInfo(candidate).isExecutable()) { return candidate; }
    }
    return {};
}

} // namespace

HamlibInstaller::HamlibInstaller(QObject* parent)
    : QObject(parent)
{
    m_proc.setProcessChannelMode(QProcess::MergedChannels);

    connect(&m_proc, &QProcess::readyRead, this, &HamlibInstaller::drain);
    connect(&m_proc, &QProcess::finished, this,
            [this](int code, QProcess::ExitStatus st) {
        drain();
        if (!m_carry.isEmpty()) {
            emit output(m_carry);
            m_carry.clear();
        }
        const bool ok = st == QProcess::NormalExit && code == 0
                        && isInstalled();
        if (ok) {
            emit finished(true, QStringLiteral("Hamlib %1 installed.")
                                    .arg(installedVersion()));
        } else if (st != QProcess::NormalExit) {
            emit finished(false, QStringLiteral("The install was stopped."));
        } else if (code == 0) {
            // Homebrew reported success and rotctld still is not there.
            // Saying "installed" here and then failing to connect would
            // send the operator looking at their cable.
            emit finished(false,
                QStringLiteral("Homebrew finished, but rotctld still "
                               "isn't where this program looks for it. "
                               "The output above says where it went."));
        } else {
            emit finished(false, QStringLiteral("Homebrew stopped with "
                                                "error %1 — see above.")
                                     .arg(code));
        }
    });
}

HamlibInstaller::~HamlibInstaller()
{
    if (m_proc.state() != QProcess::NotRunning) {
        // Killing a half-finished `brew install` leaves Homebrew to tidy
        // up after itself, which it is designed to do. Leaving it
        // running after its window has gone is worse: the operator has
        // no way to see it or stop it.
        m_proc.kill();
        m_proc.waitForFinished(2000);
    }
}

// ── Finding what is there ────────────────────────────────────────────

QString HamlibInstaller::findBrew()
{
    return findExecutable(QStringLiteral("brew"));
}

bool HamlibInstaller::isInstalled()
{
    return !findExecutable(QStringLiteral("rotctld")).isEmpty();
}

QString HamlibInstaller::parseVersion(const QString& versionOutput)
{
    // `rotctl --version` prints something like
    //   rotctl Hamlib 4.6.2  Sat Feb 22 09:11:03 2025 UTC
    // The date has numbers in it too, so anchor on Hamlib's own name
    // first and only fall back to the first version-shaped token.
    static const QRegularExpression named(
        QStringLiteral("Hamlib\\s+(\\d+\\.\\d+(?:\\.\\d+)?)"));
    const QRegularExpressionMatch m = named.match(versionOutput);
    if (m.hasMatch()) { return m.captured(1); }

    static const QRegularExpression any(
        QStringLiteral("\\b(\\d+\\.\\d+(?:\\.\\d+)?)\\b"));
    const QRegularExpressionMatch a = any.match(versionOutput);
    return a.hasMatch() ? a.captured(1) : QString{};
}

QString HamlibInstaller::installedVersion()
{
    const QString rotctl = findExecutable(QStringLiteral("rotctl"));
    if (rotctl.isEmpty()) { return {}; }
    return parseVersion(runCapture(rotctl, {QStringLiteral("--version")}));
}

QVector<HamlibRotorEntry> HamlibInstaller::parseModelList(
    const QString& listOutput)
{
    // The table looks like this, and the column widths have moved
    // between releases:
    //
    //   Rot #  Mfg          Model        Version     Status  Macro
    //       1  Hamlib       Dummy        20220102.0  Stable  ROT_MODEL_DUMMY
    //     404  DF9GR        ERC          20220109.0  Beta    ROT_MODEL_ERC
    //
    // So nothing is read by column position. A row is a leading integer
    // followed by fields separated by runs of two or more spaces, and
    // the status is found by looking for a word that is one of Hamlib's
    // four rather than by counting fields — a manufacturer with two
    // spaces in its name would otherwise shift everything along.
    QVector<HamlibRotorEntry> out;
    static const QRegularExpression row(
        QStringLiteral("^\\s*(\\d+)\\s\\s+(.*)$"));
    static const QRegularExpression gap(QStringLiteral("\\s{2,}"));

    const QStringList lines = listOutput.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const QRegularExpressionMatch m = row.match(line);
        if (!m.hasMatch()) { continue; }

        HamlibRotorEntry e;
        e.model = m.captured(1).toInt();
        if (e.model <= 0) { continue; }

        const QStringList fields =
            m.captured(2).split(gap, Qt::SkipEmptyParts);
        if (fields.size() < 2) { continue; }

        e.manufacturer = fields.at(0).trimmed();
        e.name         = fields.at(1).trimmed();
        for (const QString& f : fields) {
            const QString t = f.trimmed();
            if (t == QLatin1String("Stable") || t == QLatin1String("Beta")
                || t == QLatin1String("Alpha")
                || t == QLatin1String("Untested")) {
                e.status = t;
                break;
            }
        }
        out.append(e);
    }
    return out;
}

QVector<HamlibRotorEntry> HamlibInstaller::supportedModels()
{
    const QString rotctl = findExecutable(QStringLiteral("rotctl"));
    if (rotctl.isEmpty()) { return {}; }
    return parseModelList(runCapture(rotctl, {QStringLiteral("--list")}));
}

// ── Installing ───────────────────────────────────────────────────────

QStringList HamlibInstaller::installCommand()
{
#ifdef Q_OS_MACOS
    const QString brew = findBrew();
    if (brew.isEmpty()) { return {}; }
    return {brew, QStringLiteral("install"), QStringLiteral("hamlib")};
#else
    // Every other platform's package manager needs root, and this
    // program is not going to ask for a password to install an optional
    // extra. manualInstructions() says what to type.
    return {};
#endif
}

bool HamlibInstaller::canInstallAutomatically()
{
    return !installCommand().isEmpty();
}

QString HamlibInstaller::manualInstructions()
{
#ifdef Q_OS_MACOS
    if (findBrew().isEmpty()) {
        return QStringLiteral(
            "Homebrew isn't installed. With Homebrew (brew.sh) this "
            "program can install Hamlib for you; without it, Hamlib has "
            "ready-made macOS builds at hamlib.github.io.");
    }
    return QStringLiteral("brew install hamlib");
#elif defined(Q_OS_LINUX)
    return QStringLiteral(
        "Install Hamlib with your package manager, for example:\n"
        "    sudo apt install libhamlib-utils        (Debian, Ubuntu, "
        "Raspberry Pi OS)\n"
        "    sudo dnf install hamlib                 (Fedora)\n"
        "    sudo pacman -S hamlib                   (Arch)\n"
        "The package you need is the one containing rotctld.");
#elif defined(Q_OS_WIN)
    return QStringLiteral(
        "Download the Hamlib package for Windows from hamlib.github.io, "
        "unpack it, and add its bin folder to your PATH. rotctld.exe is "
        "the part this program needs.");
#else
    return QStringLiteral("Install Hamlib so that rotctld is on your PATH.");
#endif
}

bool HamlibInstaller::isRunning() const
{
    return m_proc.state() != QProcess::NotRunning;
}

bool HamlibInstaller::install(QString* error)
{
    if (isRunning()) {
        if (error) { *error = QStringLiteral("An install is already running."); }
        return false;
    }

    const QStringList cmd = installCommand();
    if (cmd.isEmpty()) {
        if (error) { *error = manualInstructions(); }
        return false;
    }

    // Belt and braces. The command is built two functions up and cannot
    // contain this, but an escalation that arrives by accident is worth
    // refusing rather than trusting the earlier code to stay correct.
    for (const QString& part : cmd) {
        if (part.contains(QLatin1String("sudo"))
            || part.contains(QLatin1String("doas"))) {
            if (error) {
                *error = QStringLiteral("Refusing to run an install that "
                                        "asks for administrator rights.");
            }
            return false;
        }
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Don't make the operator wait for a full tap refresh before the
    // thing they asked for starts, and don't send Homebrew's analytics
    // on their behalf.
    env.insert(QStringLiteral("HOMEBREW_NO_AUTO_UPDATE"), QStringLiteral("1"));
    env.insert(QStringLiteral("HOMEBREW_NO_ANALYTICS"), QStringLiteral("1"));
    env.insert(QStringLiteral("HOMEBREW_NO_ENV_HINTS"), QStringLiteral("1"));
    m_proc.setProcessEnvironment(env);

    m_carry.clear();
    emit output(QStringLiteral("$ %1").arg(cmd.join(QLatin1Char(' '))));
    m_proc.start(cmd.first(), cmd.mid(1));
    if (!m_proc.waitForStarted(kQueryTimeoutMs)) {
        if (error) {
            *error = QStringLiteral("Couldn't start Homebrew: %1")
                         .arg(m_proc.errorString());
        }
        return false;
    }
    return true;
}

void HamlibInstaller::cancel()
{
    if (isRunning()) { m_proc.terminate(); }
}

void HamlibInstaller::drain()
{
    m_carry += QString::fromUtf8(m_proc.readAll());
    // Homebrew redraws its progress line with a carriage return, which
    // would otherwise arrive as one enormous line.
    m_carry.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    int nl = m_carry.indexOf(QLatin1Char('\n'));
    while (nl >= 0) {
        const QString line = m_carry.left(nl);
        m_carry.remove(0, nl + 1);
        if (!line.trimmed().isEmpty()) { emit output(line); }
        nl = m_carry.indexOf(QLatin1Char('\n'));
    }
}

} // namespace Contestprogramm
