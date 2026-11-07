// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/test_cmdq_ohne_destruktor.cpp  (Contestprogramm)
// =================================================================
//
// Die letzte Sicherung und das Ende von rigctld standen im Destruktor
// von AppController. Bei Cmd+Q läuft der nicht.
//
// Gemessen am 2026-11-07 mit einer kleinen Qt-6.11-Sonde unter macOS:
//
//   Weg                 closeEvent  exec() zurück  Destruktor  atexit
//   Cmd+Q               ja          nein           nein        ja
//   quit()              ja          nein           nein        ja
//   roter Punkt         ja          ja             ja          ja
//
// Das exit() kommt noch innerhalb von closeEvent(). Hier wird darum nur
// das Fenster geschlossen und AppController bewusst NICHT zerstört --
// genau so viel, wie der Betrieb bei Cmd+Q erlebt.
//
// Modification history (Contestprogramm):
//   2026-11-07 — Original von Martin Fischer (OE5SOS), KI-gestützt
//                 über Anthropic Claude.
// =================================================================

#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTcpServer>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/LogBackup.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"

#include <memory>

using namespace Contestprogramm;

namespace {

QsoRecord einQso(const QString& rufzeichen)
{
    QsoRecord r;
    r.callsign = rufzeichen;
    r.band = QStringLiteral("144");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QStringLiteral("2026-11-07T08:00:00Z");
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

quint16 freierPort()
{
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}

std::unique_ptr<AppController> baueSteuerung(QTemporaryDir& dir)
{
    auto c = std::make_unique<AppController>();
    if (!c->openDatabase(dir.filePath(QStringLiteral("cmdq.sqlite")))) { return nullptr; }
    ContestSettings s = c->settings();
    s.ownCallsign   = QStringLiteral("OE5SOS");
    s.ownGrid       = QStringLiteral("JN67UT");
    s.rigctldHost.clear();
    s.rotor1Enabled = false;
    s.rotor2Enabled = false;
    c->setSettings(s);
    return c;
}

} // namespace

class TestCmdQOhneDestruktor : public QObject
{
    Q_OBJECT

private slots:
    void dasSchliessenSchreibtDieLetzteSicherung();
    void dasSchliessenBeendetRigctld();
    void zweimalSchliessenSchadetNicht();
};

// ── Die QSOs der letzten Minute ─────────────────────────────────────
void TestCmdQOhneDestruktor::dasSchliessenSchreibtDieLetzteSicherung()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir);
    QVERIFY(c);
    MainWindow fenster(*c);

    QVERIFY(c->logBackup());
    const QString ordner = c->logBackup()->directory();
    const int vorher = LogBackup::listBackups(ordner).size();

    QsoRecord qso = einQso(QStringLiteral("DL1ABC"));
    QVERIFY(c->database().insertQso(qso));

    fenster.close();

    QVERIFY2(LogBackup::listBackups(ordner).size() > vorher,
             "nach dem Schliessen fehlt die letzte Sicherung -- bei Cmd+Q "
             "wuerde der Destruktor sie nicht mehr schreiben");
}

// ── rigctld hält sonst den Anschluss des Funkgeräts ─────────────────
void TestCmdQOhneDestruktor::dasSchliessenBeendetRigctld()
{
#ifdef Q_OS_WIN
    QSKIP("Der Stellvertreter-rigctld ist ein Shell-Skript; unter Windows startet QProcess es nicht.");
#else
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile skript(dir.filePath(QStringLiteral("rigctld")));
    QVERIFY(skript.open(QIODevice::WriteOnly | QIODevice::Text));
    skript.write("#!/bin/sh\nexec sleep 60\n");
    skript.close();
    QVERIFY(skript.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    const QByteArray altePfade = qgetenv("PATH");
    qputenv("PATH", (dir.path() + QLatin1Char(':') + QString::fromLocal8Bit(altePfade)).toLocal8Bit());

    auto c = baueSteuerung(dir);
    QVERIFY(c);
    MainWindow fenster(*c);

    ContestSettings s = c->settings();
    s.rigctldHost        = QStringLiteral("127.0.0.1");
    s.rigctldPort        = freierPort();
    s.rigctldHamlibModel = 2031;
    s.rigctldDevice      = QStringLiteral("/dev/cu.fake");
    s.rigctldBaud        = 115200;
    c->setSettings(s);

    // Voraussetzung: ohne laufenden Daemon sagte die Prüfung nichts.
    QTRY_VERIFY_WITH_TIMEOUT(c->rigctldRunning(), 8000);

    fenster.close();

    QVERIFY2(!c->rigctldRunning(),
             "rigctld laeuft nach dem Schliessen weiter -- bei Cmd+Q bliebe "
             "er als Waise zurueck und hielte den seriellen Anschluss");

    qputenv("PATH", altePfade);
#endif
}

// ── Schliessen, dann der Destruktor auf dem roten Punkt ─────────────
void TestCmdQOhneDestruktor::zweimalSchliessenSchadetNicht()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto c = baueSteuerung(dir);
    QVERIFY(c);

    QsoRecord qso = einQso(QStringLiteral("DL2ABC"));
    QVERIFY(c->database().insertQso(qso));

    c->vorDemBeenden();
    const QString ordner = c->logBackup()->directory();
    const int nachDemErsten = LogBackup::listBackups(ordner).size();
    QVERIFY(nachDemErsten > 0);

    c->vorDemBeenden();
    QCOMPARE(LogBackup::listBackups(ordner).size(), nachDemErsten);
}

QTEST_MAIN(TestCmdQOhneDestruktor)
#include "test_cmdq_ohne_destruktor.moc"
