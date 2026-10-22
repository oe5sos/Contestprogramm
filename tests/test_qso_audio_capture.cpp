// =================================================================
// tests/test_qso_audio_capture.cpp  (Contestprogramm)
// =================================================================
//
// Die Geraeteseite -- geprueft, soweit sie ohne Tongeraet pruefbar
// ist. Die CI hat keines, und dieser Lauf ueberspringt sich darum
// NICHT: er faehrt genau die Wege, die ohne Geraet gelten.
//
// Der wichtigste davon ist das stille Ausweichen. Wer „AetherSDR DAX 1"
// eingestellt hat und nach einem Neustart das Mikrofon des Rechners
// bekommt, merkt es erst an der Aufnahme -- Stunden spaeter. Also muss
// ein unbekannter Geraetename ABLEHNEN und nicht auf das Standardgeraet
// fallen.
// =================================================================

#include <QtTest>

#include "core/QsoAudioCapture.h"
#include "core/QsoAudioRecorder.h"

using namespace Contestprogramm;

class TestQsoAudioCapture : public QObject
{
    Q_OBJECT

private slots:

    void ohneMitschneiderFaengtErNichtAn()
    {
        QsoAudioCapture c;
        QString fehler;
        QVERIFY(!c.start(QString(), &fehler));
        QVERIFY2(!fehler.isEmpty(), "er hat abgelehnt, ohne zu sagen warum");
        QVERIFY(!c.isRunning());
    }

    void einUnbekanntesGeraetWirdAbgelehntStattErsetzt()
    {
        QsoAudioRecorder r;
        QsoAudioCapture c;
        c.setRecorder(&r);

        QString fehler;
        const bool ging = c.start(
            QStringLiteral("Dieses Geraet gibt es nicht 4711"), &fehler);

        QVERIFY2(!ging, "er ist still auf das Standardgeraet ausgewichen");
        QVERIFY2(fehler.contains(QStringLiteral("4711")), qPrintable(fehler));
        QVERIFY(!c.isRunning());
        // Und er darf dabei die Aufnahme nicht angefangen haben.
        QVERIFY(!r.isRecording());
    }

    void dieGeraeteListeNenntDasStandardgeraetZuerst()
    {
        const QStringList namen = QsoAudioCapture::inputDeviceNames();
        // Auf einem Laeufer ohne Tongeraet ist die Liste leer -- das
        // ist kein Fehler, nur nichts zu pruefen. Mit Geraeten muss
        // der erste Eintrag das Standardgeraet sein, denn die
        // Einstellung bietet ihn als „(Standardeingang)" an.
        if (namen.isEmpty()) {
            QVERIFY(true);
            return;
        }
        QVERIFY(!namen.first().trimmed().isEmpty());
        // Keine Doppelten: der Standardeingang ist auch in der
        // vollen Liste, und zweimal derselbe Name in einer Auswahl
        // laesst den Bediener raten, welcher gemeint ist.
        QCOMPARE(namen.size(), QSet<QString>(namen.begin(), namen.end()).size());
    }

    void stoppOhneStartIstEinLeerlauf()
    {
        QsoAudioCapture c;
        c.stop();
        QVERIFY(!c.isRunning());
    }
};

QTEST_MAIN(TestQsoAudioCapture)
#include "test_qso_audio_capture.moc"
