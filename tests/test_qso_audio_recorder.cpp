// =================================================================
// tests/test_qso_audio_recorder.cpp  (Contestprogramm)
// =================================================================
//
// Der stundenlange Mitschnitt, ohne Tongeraet.
//
// Betreiber am 2026-10-22: „ziel wäre, dass das logprogramm das alles
// kann, unabhängig von longpath. sprich ich würde gerne stundenlang
// alles automatisch mitschneiden wollen."
//
// Die Abtastwerte kommen hier aus dem Prueflauf statt aus einer
// Soundkarte -- genau dafuer nimmt QsoAudioRecorder::feed() Werte und
// kein Geraet. Die CI hat kein Tongeraet; ein Prueflauf, der sich dort
// ueberspringt, prueft nichts.
//
// Was festgenagelt wird:
//   * die WAV-Datei ist LESBAR -- Kopf, Rate, Kanaele, Datenlaenge
//   * nach `rollMinutes` beginnt eine neue Datei, und die alte hat
//     richtige Groessenfelder (sonst kostet ein Absturz den Abend)
//   * der QSO-Index zeigt auf die Sekunde IM Mitschnitt
//   * die Platzwache haelt an und sagt, warum
//   * ohne laufende Aufnahme ist markQso() ein Leerlauf
// =================================================================

#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/QsoAudioRecorder.h"

using namespace Contestprogramm;

namespace {

quint32 le32(const QByteArray& b, int at)
{
    return static_cast<quint8>(b[at])
           | (static_cast<quint32>(static_cast<quint8>(b[at + 1])) << 8)
           | (static_cast<quint32>(static_cast<quint8>(b[at + 2])) << 16)
           | (static_cast<quint32>(static_cast<quint8>(b[at + 3])) << 24);
}

quint16 le16(const QByteArray& b, int at)
{
    return static_cast<quint16>(static_cast<quint8>(b[at])
                                | (static_cast<quint16>(static_cast<quint8>(b[at + 1])) << 8));
}

// Ein Block Stereo-Ton, beide Kanaele unterscheidbar: links ein
// steigender Wert, rechts sein Negativ. Damit laesst sich spaeter
// sagen, ob die Kanaele vertauscht wurden.
QVector<qint16> block(int frames, qint16 ab = 0)
{
    QVector<qint16> v(frames * 2);
    for (int i = 0; i < frames; ++i) {
        v[i * 2]     = static_cast<qint16>(ab + i);
        v[i * 2 + 1] = static_cast<qint16>(-(ab + i));
    }
    return v;
}

} // namespace

class TestQsoAudioRecorder : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QsoAudioRecorder::Settings einstellung(int rollMinutes = 0) const
    {
        QsoAudioRecorder::Settings s;
        s.folder          = m_dir.path();
        s.rollMinutes     = rollMinutes;
        s.minFreePercent  = 0;      // die Wache wird eigens geprueft
        return s;
    }

private slots:

    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void cleanup() { QsoAudioRecorder::setFreePercentOverrideForTest(-1); }

    void dieDateiIstLesbar()
    {
        QsoAudioRecorder r;
        r.setSettings(einstellung());
        QVERIFY(r.setFormat(48000, 2));

        QString fehler;
        QVERIFY2(r.start(&fehler), qPrintable(fehler));

        const QVector<qint16> b = block(4800);        // 0,1 s
        r.feed(b.constData(), 4800);
        const QString pfad = r.currentFile();
        r.stop();

        QFile f(pfad);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray roh = f.readAll();
        f.close();

        QCOMPARE(roh.left(4), QByteArray("RIFF"));
        QCOMPARE(roh.mid(8, 4), QByteArray("WAVE"));
        QCOMPARE(le16(roh, 20), quint16(1));          // PCM
        QCOMPARE(le16(roh, 22), quint16(2));          // Kanaele
        QCOMPARE(le32(roh, 24), quint32(48000));      // Rate
        QCOMPARE(le16(roh, 34), quint16(16));         // Bits

        // Die beiden Groessenfelder MUSS der Abschluss nachtragen --
        // sonst steht der Ton in der Datei und kein Spieler zeigt ihn.
        const quint32 datenBytes = 4800u * 2u * 2u;
        QCOMPARE(le32(roh, 40), datenBytes);
        QCOMPARE(le32(roh, 4), 36u + datenBytes);
        QCOMPARE(roh.size(), static_cast<int>(44 + datenBytes));

        // Und der Ton selbst, nicht nur seine Laenge: erster Rahmen
        // links 0 / rechts 0, zweiter links 1 / rechts -1.
        QCOMPARE(le16(roh, 44), quint16(0));
        QCOMPARE(static_cast<qint16>(le16(roh, 48)), qint16(1));
        QCOMPARE(static_cast<qint16>(le16(roh, 50)), qint16(-1));
    }

    void nachDerFristBeginntEineNeueDatei()
    {
        QsoAudioRecorder r;
        r.setSettings(einstellung(/*rollMinutes=*/1));
        // Eine kleine Rate, damit eine „Minute" im Prueflauf 100
        // Rahmen sind und nicht 2,88 Millionen.
        QVERIFY(r.setFormat(100, 2));

        QSignalSpy gewechselt(&r, &QsoAudioRecorder::fileRolled);
        QString fehler;
        QVERIFY2(r.start(&fehler), qPrintable(fehler));
        const QString erste = r.currentFile();

        const QVector<qint16> b = block(6000);   // 60 s bei 100 Hz
        r.feed(b.constData(), 6000);

        QCOMPARE(gewechselt.count(), 1);
        QVERIFY2(r.currentFile() != erste, qPrintable(r.currentFile()));

        // Die ABGESCHLOSSENE erste Datei muss stimmen -- das ist der
        // halbe Sinn des Wechsels: ein Absturz danach kostet nur die
        // laufende Stunde.
        QFile f(erste);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray roh = f.readAll();
        f.close();
        QCOMPARE(le32(roh, 40), 6000u * 2u * 2u);

        r.stop();
    }

    void derIndexZeigtAufDieSekundeImMitschnitt()
    {
        QsoAudioRecorder r;
        r.setSettings(einstellung());
        QVERIFY(r.setFormat(100, 2));
        QString fehler;
        QVERIFY2(r.start(&fehler), qPrintable(fehler));

        const QVector<qint16> b = block(1000);   // 10 s bei 100 Hz
        r.feed(b.constData(), 1000);
        r.markQso(QStringLiteral("OE5SOS"), QStringLiteral("144"),
                  QStringLiteral("USB"),
                  QDateTime(QDate(2026, 10, 22), QTime(5, 0), QTimeZone::UTC));

        r.feed(b.constData(), 1000);             // weitere 10 s
        r.markQso(QStringLiteral("DL1ABC"), QStringLiteral("432"),
                  QStringLiteral("CW"),
                  QDateTime(QDate(2026, 10, 22), QTime(5, 1), QTimeZone::UTC));

        const QString idx = r.currentIndexFile();
        r.stop();

        QFile f(idx);
        QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(idx));
        const QStringList zeilen =
            QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        f.close();

        QCOMPARE(zeilen.size(), 2);

        const QStringList a = zeilen.at(0).split(QLatin1Char('\t'));
        QCOMPARE(a.value(0), QStringLiteral("10"));
        QCOMPARE(a.value(1), QStringLiteral("0:00:10"));
        QCOMPARE(a.value(2), QStringLiteral("OE5SOS"));
        QCOMPARE(a.value(3), QStringLiteral("144"));
        QCOMPARE(a.value(4), QStringLiteral("USB"));

        const QStringList z = zeilen.at(1).split(QLatin1Char('\t'));
        QCOMPARE(z.value(0), QStringLiteral("20"));
        QCOMPARE(z.value(2), QStringLiteral("DL1ABC"));
    }

    void dieZeitKommtAusDenRahmenNichtAusDerUhr()
    {
        // Setzt das Tongeraet aus, laeuft die Uhr weiter -- eine
        // Indexzeile nach der Uhr zeigte dann auf eine Stelle, die es
        // in der Datei nicht gibt. Hier wird eine Sekunde Ton
        // gefuettert und danach gewartet; die Marke muss bei 1 stehen,
        // nicht bei 1 plus Wartezeit.
        QsoAudioRecorder r;
        r.setSettings(einstellung());
        QVERIFY(r.setFormat(100, 2));
        QString fehler;
        QVERIFY2(r.start(&fehler), qPrintable(fehler));

        const QVector<qint16> b = block(100);    // 1 s
        r.feed(b.constData(), 100);
        QTest::qWait(1200);                      // Uhr laeuft, Ton nicht
        r.markQso(QStringLiteral("OE1XYZ"), QStringLiteral("144"),
                  QStringLiteral("USB"), QDateTime::currentDateTimeUtc());

        const QString idx = r.currentIndexFile();
        r.stop();

        QFile f(idx);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString zeile = QString::fromUtf8(f.readAll()).trimmed();
        f.close();
        QCOMPARE(zeile.section(QLatin1Char('\t'), 0, 0), QStringLiteral("1"));
    }

    void diePlatzwacheHaeltAnUndSagtWarum()
    {
        QsoAudioRecorder r;
        QsoAudioRecorder::Settings s = einstellung();
        s.minFreePercent = 10;
        r.setSettings(s);
        QVERIFY(r.setFormat(100, 2));

        QsoAudioRecorder::setFreePercentOverrideForTest(3);   // Platte fast voll
        QString fehler;
        QVERIFY2(!r.start(&fehler), "er hat trotz voller Platte angefangen");
        QVERIFY2(fehler.contains(QStringLiteral("10")), qPrintable(fehler));

        // Und umgekehrt: mit Platz laeuft er.
        QsoAudioRecorder::setFreePercentOverrideForTest(50);
        QVERIFY2(r.start(&fehler), qPrintable(fehler));
        r.stop();
    }

    void ohneAufnahmeIstDieMarkeEinLeerlauf()
    {
        // Das Logprogramm soll markQso() rufen koennen, ohne zu wissen,
        // ob mitgeschnitten wird -- sonst steht die Abfrage an jeder
        // Rufstelle.
        QsoAudioRecorder r;
        r.setSettings(einstellung());
        r.markQso(QStringLiteral("OE5SOS"), QStringLiteral("144"),
                  QStringLiteral("USB"), QDateTime::currentDateTimeUtc());
        QVERIFY(!r.isRecording());
        QVERIFY(r.currentIndexFile().isEmpty());
    }

    void einFormatwechselWaehrendDerAufnahmeWirdAbgelehnt()
    {
        // Mitten in einer WAV-Datei die Rate zu wechseln macht sie
        // unlesbar: der Kopf nennt genau eine.
        QsoAudioRecorder r;
        r.setSettings(einstellung());
        QVERIFY(r.setFormat(48000, 2));
        QString fehler;
        QVERIFY2(r.start(&fehler), qPrintable(fehler));
        QVERIFY2(!r.setFormat(8000, 1), "der Formatwechsel ging durch");
        QCOMPARE(r.sampleRateHz(), 48000);
        r.stop();
    }

    void derSpeicherWaechstNichtMitDerDauer()
    {
        // Der ganze Sinn des stundenlangen Betriebs: geschrieben wird
        // blockweise, nicht gesammelt. Zwei Stunden Ton bei kleiner
        // Rate, und die Datei muss genau so gross sein wie die Rahmen
        // -- haette die Klasse gesammelt, waere sie hier schon am
        // Speicher gescheitert.
        QsoAudioRecorder r;
        r.setSettings(einstellung());
        QVERIFY(r.setFormat(1000, 2));
        QString fehler;
        QVERIFY2(r.start(&fehler), qPrintable(fehler));

        const QVector<qint16> b = block(1000);          // 1 s
        for (int i = 0; i < 7200; ++i) {                // 2 Stunden
            r.feed(b.constData(), 1000);
        }
        const QString pfad = r.currentFile();
        QCOMPARE(r.recordedFrames(), qint64(7'200'000));
        QVERIFY(qAbs(r.recordedSeconds() - 7200.0) < 1e-9);
        r.stop();

        QCOMPARE(QFileInfo(pfad).size(),
                 qint64(44 + 7'200'000LL * 2 * 2));
    }
};

QTEST_MAIN(TestQsoAudioRecorder)
#include "test_qso_audio_recorder.moc"
