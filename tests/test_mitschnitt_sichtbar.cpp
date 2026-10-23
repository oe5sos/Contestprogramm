// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/test_mitschnitt_sichtbar.cpp  (Contestprogramm)
// =================================================================
//
// Zwei Fragen des Betreibers am 2026-10-22, beide vor dem laufenden
// Programm gestellt:
//
//   „wie kann ich hier einen ordner anlegen bzw suchen"
//   „wo starte und beende ich die aufnahme. sollte zu sehen sein."
//
// Im Ordnerfeld stand „desktop" — ein Name, kein Pfad. QDir::mkpath
// haette daraus „/desktop" gemacht, und daran scheitert jede Aufnahme
// still. Einen Waehler gab es nicht.
//
// Und der Haken in den Einstellungen war beides: Schalter UND
// einziger Hinweis. Er steht hinter zwei Klicks und acht Gruppen. Ein
// Mitschnitt, von dem man nicht sieht, ob er laeuft, ist keiner.
//
// Modification history (Contestprogramm):
//   2026-10-22 — Original von Martin Fischer (OE5SOS), KI-gestuetzt
//                 ueber Anthropic Claude.
// =================================================================

#include <QtTest>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "ui/MainWindow.h"
#include "ui/SettingsDialog.h"

#include <memory>

using namespace Contestprogramm;

class TestMitschnittSichtbar : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<AppController> baueSteuerung(QTemporaryDir& dir,
                                                 const QString& datei)
    {
        auto c = std::make_unique<AppController>();
        if (!c->openDatabase(dir.filePath(datei))) { return nullptr; }
        ContestSettings s = c->settings();
        s.ownCallsign      = QStringLiteral("OE5SOS");
        s.ownGrid          = QStringLiteral("JN67VV");
        s.activeContestId  = QStringLiteral("IARU_R1_VHF_UHF");
        s.rigctldHost.clear();
        s.rotor1Enabled    = false;
        s.rotor2Enabled    = false;
        s.esmEnabled       = false;
        c->setSettings(s);
        return c;
    }

private slots:
    // ── Das Ordnerfeld hat einen Knopf ──────────────────────────────
    //
    // Geprueft wird, dass er DA ist und neben dem Feld sitzt. Was er
    // oeffnet, ist Qts eigener Ordnerwaehler (mit „Neuer Ordner"); den
    // aufzumachen hiesse, einen Prueflauf an einem systemeigenen
    // Fenster haengen zu lassen, und das haelt kein Laufer durch.
    void dasOrdnerfeldHatEinenKnopf()
    {
        ContestSettings s;
        SettingsDialog dlg(s, {});

        auto* feld = dlg.findChild<QLineEdit*>(
            QStringLiteral("audioRecordFolderEdit"));
        QVERIFY2(feld, "das Ordnerfeld fehlt");

        // Und der Haken „Aktiv" ist WEG: ein und aus macht die
        // Statuszeile. Ein Haken, der nach jedem Start aus ist, luegt
        // ueber seinen eigenen Zweck.
        for (QCheckBox* h : dlg.findChildren<QCheckBox*>()) {
            QVERIFY2(h->text() != QStringLiteral("Aktiv")
                         || h->parentWidget() != feld->parentWidget(),
                     "im Mitschnitt-Kasten steht noch ein Haken");
        }

        auto* knopf = dlg.findChild<QPushButton*>(
            QStringLiteral("audioRecordFolderButton"));
        QVERIFY2(knopf, "neben dem Ordnerfeld fehlt der Waehler -- und "
                        "dann muss man den Pfad wieder tippen");
        QVERIFY(knopf->isEnabled());

        // Beide in derselben Zeile: der Knopf nuetzt nichts, wenn er
        // an einer anderen Stelle des Fensters steht als das Feld, das
        // er fuellt.
        QCOMPARE(knopf->parentWidget(), feld->parentWidget());
    }

    // ── Nach einem Neustart ist der Mitschnitt AUS ──────────────────
    //
    // Betreiber am 2026-10-22: „mitschnitt sollte nicht automatisch
    // starten, ein und aus auf meinen befehl."
    //
    // Alles andere bleibt gespeichert -- Geraet, Ordner, Dateilaenge,
    // Platzgrenze. Nur der Schalter selbst faengt jedes Mal bei aus
    // an.
    void nachEinemNeustartIstDerMitschnittAus()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString datei = dir.filePath(QStringLiteral("neustart.sqlite"));

        {
            auto c = std::make_unique<AppController>();
            QVERIFY(c->openDatabase(datei));
            ContestSettings s = c->settings();
            s.audioRecordFolder  = dir.path();
            s.audioRecordDevice  = QStringLiteral("Irgendein Mikrofon");
            s.audioRecordEnabled = true;
            s.audioRecordRollMinutes = 42;
            c->setSettings(s);
            QVERIFY(c->settings().audioRecordEnabled);
        }

        // Dieselbe Datenbank noch einmal aufmachen -- der Neustart.
        auto c = std::make_unique<AppController>();
        QVERIFY(c->openDatabase(datei));
        const ContestSettings s = c->settings();

        QVERIFY2(!s.audioRecordEnabled,
                 "der Mitschnitt darf nach einem Neustart nicht von "
                 "selbst wieder laufen");

        // Und die Gegenprobe dazu: der REST muss sehr wohl bleiben,
        // sonst haette man die Einstellungen bloss vergessen.
        QCOMPARE(s.audioRecordFolder, dir.path());
        QCOMPARE(s.audioRecordDevice, QStringLiteral("Irgendein Mikrofon"));
        QCOMPARE(s.audioRecordRollMinutes, 42);
    }

    // ── Der Mitschnitt steht in der Statuszeile ─────────────────────
    void derMitschnittStehtInDerStatuszeile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto c = baueSteuerung(dir, QStringLiteral("mitschnitt.sqlite"));
        QVERIFY(c);

        MainWindow fenster(*c);
        auto* knopf = fenster.findChild<QPushButton*>(
            QStringLiteral("audioRecordButton"));
        QVERIFY2(knopf, "der Mitschnitt ist nirgends zu sehen");

        // Ohne Ordner sagt er das, statt einen Schalter anzubieten,
        // der ins Leere ginge.
        QVERIFY2(knopf->text().contains(QStringLiteral("Ordner")),
                 qPrintable(QStringLiteral("stattdessen: %1").arg(knopf->text())));

        // Mit Ordner, aber aus.
        ContestSettings s = c->settings();
        s.audioRecordFolder  = dir.path();
        s.audioRecordEnabled = false;
        c->setSettings(s);
        // Ueber den echten Zeitgeber (1 s), nicht ueber einen
        // Prueflauf-Eingang: so ist mitgeprueft, dass die Anzeige von
        // selbst nachzieht und nicht erst, wenn jemand sie anstoesst.
        QTRY_VERIFY_WITH_TIMEOUT(
            knopf->text().contains(QStringLiteral("aus")), 4000);
    }

    // ── Ein Druck schaltet denselben Schalter wie der Haken ─────────
    //
    // Nicht einen zweiten daneben: zwei Schalter fuer eine Sache
    // werden frueher oder spaeter uneins.
    void einDruckSchaltetDenselbenSchalter()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto c = baueSteuerung(dir, QStringLiteral("druck.sqlite"));
        QVERIFY(c);

        ContestSettings s = c->settings();
        s.audioRecordFolder  = dir.path();
        s.audioRecordEnabled = false;
        c->setSettings(s);

        MainWindow fenster(*c);
        auto* knopf = fenster.findChild<QPushButton*>(
            QStringLiteral("audioRecordButton"));
        QVERIFY(knopf);

        knopf->click();
        QVERIFY2(c->settings().audioRecordEnabled,
                 "ein Druck muss den Mitschnitt einschalten");

        knopf->click();
        QVERIFY2(!c->settings().audioRecordEnabled,
                 "und der naechste wieder aus");
    }

    // ── Ohne Ordner schaltet der Druck NICHT ein ────────────────────
    //
    // Gegenprobe zum Lauf darueber: sonst stuende „Mitschnitt laeuft"
    // da, waehrend nichts geschrieben wird.
    void ohneOrdnerSchaltetDerDruckNichtEin()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto c = baueSteuerung(dir, QStringLiteral("ohneordner.sqlite"));
        QVERIFY(c);

        ContestSettings s = c->settings();
        s.audioRecordFolder.clear();
        s.audioRecordEnabled = false;
        c->setSettings(s);

        MainWindow fenster(*c);
        auto* knopf = fenster.findChild<QPushButton*>(
            QStringLiteral("audioRecordButton"));
        QVERIFY(knopf);

        knopf->click();
        QVERIFY2(!c->settings().audioRecordEnabled,
                 "ohne Ordner darf der Druck nichts einschalten");
    }
};

QTEST_MAIN(TestMitschnittSichtbar)
#include "test_mitschnitt_sichtbar.moc"
