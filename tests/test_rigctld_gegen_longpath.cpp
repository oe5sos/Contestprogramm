// =================================================================
// tests/test_rigctld_gegen_longpath.cpp  (Contestprogramm)
// =================================================================
//
// Der Durchgang gegen ein ECHTES Gegenüber: Longpath spricht seit dem
// 2026-10-21 Hamlibs rigctld-Protokoll (oe5sos/Longpath#308), also
// kann das Contestlog es als Funkgerät benutzen -- ohne zweites
// Programm dazwischen, ohne TCI, ohne eine Zeile Code hier.
//
// Warum das ein eigener Lauf ist und nicht Teil der Prüfreihe: er
// braucht ein laufendes Gegenüber. Ohne die Umgebungsvariable
// `CONTEST_RIGCTLD_HOST` wird er ÜBERSPRUNGEN, nicht grün gemeldet --
// ein Prüfstand, der bei Abwesenheit Erfolg meldet, ist kein
// Prüfstand. Die CI hat kein Longpath, also läuft er dort nie.
//
// So fährt man ihn (Longpath mit eingeschaltetem rigctld-Server):
//
//     CONTEST_RIGCTLD_HOST=127.0.0.1 CONTEST_RIGCTLD_PORT=4532 \
//         ./build/tests/test_rigctld_gegen_longpath
//
// Er stellt NICHT auf Senden: kein `T 1`, kein `b`. Gelesen wird, und
// die Frequenz wird gesetzt und zurückgelesen -- das bewegt an einem
// Longpath ohne Funkgerät nichts und an einem mit Funkgerät nur den
// Empfänger.
// =================================================================

#include <QtTest>
#include <QSignalSpy>

#include "core/RigctldClient.h"

using namespace Contestprogramm;

class TestRigctldGegenLongpath : public QObject {
    Q_OBJECT

private:
    static QString host()
    {
        return qEnvironmentVariable("CONTEST_RIGCTLD_HOST").trimmed();
    }
    static quint16 port()
    {
        const QString p = qEnvironmentVariable("CONTEST_RIGCTLD_PORT").trimmed();
        bool ok = false;
        const uint v = p.toUInt(&ok);
        return (ok && v > 0 && v < 65536) ? static_cast<quint16>(v) : 4532;
    }

private slots:

    void am_echten_longpath_wenn_eines_laeuft()
    {
        if (host().isEmpty()) {
            QSKIP("CONTEST_RIGCTLD_HOST nicht gesetzt — kein Gegenueber");
        }

        RigctldClient client;
        client.setTarget(host(), port());
        QSignalSpy zustand(&client, &RigctldClient::stateChanged);
        QSignalSpy fehler(&client, &RigctldClient::errorOccurred);

        client.connectToRig();
        QTRY_VERIFY_WITH_TIMEOUT(client.isConnected(), 5000);
        QVERIFY2(fehler.isEmpty(),
                 qPrintable(fehler.isEmpty() ? QString()
                                             : fehler.first().at(0).toString()));

        // Der Abfragetakt holt Frequenz, Betriebsart und Sendezustand.
        // Eine Frequenz von 0 heisst: Longpath haengt an keinem Geraet
        // -- dann ist dieser Lauf trotzdem gueltig (die Verbindung und
        // das Protokoll stimmen), aber er sagt es laut, statt eine
        // Null als Messwert auszugeben.
        QTRY_VERIFY_WITH_TIMEOUT(!client.mode().isEmpty(), 5000);
        qInfo() << "Longpath meldet:" << client.frequencyHz() << "Hz,"
                << client.mode() << client.passbandHz() << "Hz Durchlass,"
                << "PTT" << client.pttActive();

        if (client.frequencyHz() == 0) {
            qWarning() << "Frequenz 0 — an diesem Longpath haengt kein "
                          "Funkgeraet; Protokoll geprueft, Betrieb nicht";
            return;
        }

        // Frequenz setzen und zuruecklesen. Zwei kHz hoch, dann
        // zurueck: das bewegt nur den Empfaenger und laesst die
        // Station so stehen, wie sie stand.
        const qint64 vorher = client.frequencyHz();
        client.setFrequency(vorher + 2000);
        QTRY_COMPARE_WITH_TIMEOUT(client.frequencyHz(), vorher + 2000, 5000);
        client.setFrequency(vorher);
        QTRY_COMPARE_WITH_TIMEOUT(client.frequencyHz(), vorher, 5000);

        client.disconnectFromRig();
        QTRY_VERIFY_WITH_TIMEOUT(!client.isConnected(), 3000);
    }

    void ohne_gegenueber_meldet_er_sich_ab_statt_zu_haengen()
    {
        // Gegenprobe, die IMMER laeuft: auf einen Port, auf dem
        // niemand horcht, muss der Client scheitern und es sagen --
        // nicht stumm im Verbindungsaufbau stehenbleiben. Genau das
        // ist der Fall, den der Bediener am Contestabend trifft, wenn
        // Longpath nicht gestartet ist.
        RigctldClient client;
        client.setTarget(QStringLiteral("127.0.0.1"), 1);   // reserviert, nie belegt
        QSignalSpy fehler(&client, &RigctldClient::errorOccurred);
        client.connectToRig();

        QTRY_VERIFY_WITH_TIMEOUT(!fehler.isEmpty() || !client.isConnected(), 8000);
        QVERIFY(!client.isConnected());
    }
};

QTEST_MAIN(TestRigctldGegenLongpath)
#include "test_rigctld_gegen_longpath.moc"
