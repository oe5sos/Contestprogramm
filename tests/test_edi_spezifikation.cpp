// =================================================================
// tests/test_edi_spezifikation.cpp  (Contestprogramm)
// =================================================================
//
// Die abgegebene EDI-Datei Feld fuer Feld gegen die Spezifikation,
// nicht gegen unsere eigene Vorstellung davon:
//
//   IARU DOCUMENT: REG1TEST (IARU Region 1 Committee C.5), Issue 1,1
//   "Electronic Data Interchange - EDI-file format for contests in
//    Region 1 above 30 MHz"
//
// Daraus die drei Stellen, die hier gemessen werden (woertlich):
//
//   "Characters -- Used characters are in accordance with the 7-bit
//    ASCII alphabet and only characters with the following decimal
//    number are allowed 10, 13, 32-127."
//
//   "Line length -- If line length is already specified it must not be
//    exceeded, other lines must not exceed a length of 75 characters."
//
//   und die Tabelle der QSO-Spalten (Maximum length):
//    Date 6, Time 4, Call 3..14, Mode 1, Sent-RST 0/2/3,
//    Sent QSO number 0/3/4, Received-RST 0/2/3,
//    Received QSO number 0/3/4, Received Exchange 0..6,
//    Received WWL 0/4/6, QSO points 1..6, New-Exchange 1,
//    New-WWL 1, New-DXCC 1, Duplicate-QSO 1
//
// Dazu aus dem Kopfteil: "PWWLo ... Maximum length is six characters"
// und "PExch ... Maximum length is six characters".
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-10-16 — New. AI-assisted via Anthropic Claude Code, operator
//                 Ralph Martin Fischer.
// =================================================================

#include <QtTest>

#include <QCoreApplication>
#include <QTemporaryDir>

#include "app/ContestSettings.h"
#include "core/AsciiText.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/EdiExporter.h"
#include "data/QsoRecord.h"

using namespace Contestprogramm;

namespace {

const char* kIaruJson = R"JSON(
{
  "id": "IARU_R1_VHF_UHF",
  "name": "IARU Region 1 VHF/UHF Contest",
  "bands": ["144", "432"],
  "dupe_scope": ["callsign", "band", "mode"],
  "exchange_fields": [
    { "key": "rst",    "label": "RST",  "type": "rst" },
    { "key": "serial", "label": "Nr.",  "type": "int", "auto_increment": true },
    { "key": "grid",   "label": "Grid", "type": "grid6" }
  ]
}
)JSON";

ContestSettings feuerkogel()
{
    ContestSettings settings;
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.ownElevationM = 1592.0;
    settings.antennaHeightM = 10.0;
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    return settings;
}

// Eine Station, wie sie in Oesterreich wirklich eingetragen wird: die
// Umlaute stehen hier nicht zur Zierde, sondern weil Martins Land,
// Strasse und Antennenbeschreibung sie tragen.
EdiStationInfo stationMitUmlauten()
{
    EdiStationInfo station;
    station.section = QStringLiteral("SINGLE");
    station.club = QStringLiteral("ADL 505");
    station.locationLine1 = QStringLiteral("Feuerkogel, Oberösterreich");
    station.name = QStringLiteral("Martin Fischer");
    station.street = QStringLiteral("Musterstraße 1");
    station.postalCode = QStringLiteral("4810");
    station.city = QStringLiteral("Gmunden");
    station.country = QStringLiteral("Österreich");
    station.email = QStringLiteral("oe5sos@example.org");
    station.txEquipment = QStringLiteral("Transverter über K3");
    station.powerWatts = 100;
    station.antenna = QStringLiteral("2 x 12 el. Yagi, 10 m über Grund");
    return station;
}

QsoRecord makeQso(const QString& call, const QString& band, const QString& mode, const QString& time,
                  int serialSent, int serialRcvd, const QString& grid, double km)
{
    const QString rst = mode == QStringLiteral("CW") ? QStringLiteral("599") : QStringLiteral("59");
    QsoRecord r;
    r.callsign = call;
    r.band = band;
    r.mode = mode;
    r.timestampUtc = QStringLiteral("2026-10-03T%1:00Z").arg(time);
    r.gridSquare = grid;
    r.distanceKm = km;
    r.serialSent = serialSent;
    r.serialRcvd = serialRcvd;
    r.rstSent = rst;
    r.rstRcvd = rst;
    r.exchangeSent = QStringLiteral("%1 %2 JN67UT").arg(rst).arg(serialSent, 3, 10, QLatin1Char('0'));
    r.exchangeRcvd = QStringLiteral("%1 %2 %3").arg(rst).arg(serialRcvd, 3, 10, QLatin1Char('0')).arg(grid);
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

// --- die Spezifikation als Pruefung ------------------------------------

void checkCharacterSet(const QString& file)
{
    for (int i = 0; i < file.size(); ++i) {
        const ushort code = file.at(i).unicode();
        const bool allowed = code == 10 || code == 13 || (code >= 32 && code <= 127);
        if (!allowed) {
            QFAIL(qPrintable(QStringLiteral("Zeichen U+%1 an Stelle %2 steht nicht im 7-Bit-ASCII-Alphabet "
                                            "(erlaubt sind 10, 13, 32-127): \"%3\"")
                                 .arg(code, 4, 16, QLatin1Char('0'))
                                 .arg(i)
                                 .arg(file.mid(qMax(0, i - 20), 40))));
        }
    }
}

void checkLineLengths(const QString& file)
{
    const QStringList lines = file.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        if (line.size() > 75) {
            QFAIL(qPrintable(QStringLiteral("Zeile ist %1 Zeichen lang, erlaubt sind 75: \"%2\"")
                                 .arg(line.size())
                                 .arg(line)));
        }
    }
}

// Erlaubte Laengen je Spalte; -1 heisst "bis zu", eine Liste heisst
// "genau eine davon".
void checkQsoColumns(const QString& file)
{
    const QStringList lines = file.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts);
    int marker = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines.at(i).startsWith(QStringLiteral("[QSORecords;"))) { marker = i; }
    }
    QVERIFY2(marker >= 0, "Kein [QSORecords;n]-Kennzeichen in der Datei");

    for (int i = marker + 1; i < lines.size(); ++i) {
        const QString line = lines.at(i);
        const QStringList f = line.split(QLatin1Char(';'));
        QCOMPARE(f.size(), 15);

        auto fail = [&](const QString& what) {
            QFAIL(qPrintable(QStringLiteral("%1 -- Zeile: \"%2\"").arg(what, line)));
        };
        auto exactly = [&](int index, const QString& name, const QList<int>& allowed) {
            if (!allowed.contains(f.at(index).size())) {
                QStringList texts;
                for (int a : allowed) { texts << QString::number(a); }
                fail(QStringLiteral("%1 \"%2\" ist %3 Zeichen lang, erlaubt sind %4")
                         .arg(name, f.at(index))
                         .arg(f.at(index).size())
                         .arg(texts.join(QStringLiteral(" oder "))));
            }
        };
        auto upTo = [&](int index, const QString& name, int max) {
            if (f.at(index).size() > max) {
                fail(QStringLiteral("%1 \"%2\" ist %3 Zeichen lang, erlaubt sind hoechstens %4")
                         .arg(name, f.at(index))
                         .arg(f.at(index).size())
                         .arg(max));
            }
        };

        exactly(0, QStringLiteral("Datum"), {6});
        exactly(1, QStringLiteral("Zeit"), {4});
        // "Call = 3 to 14 characters"
        if (f.at(2).size() < 3 || f.at(2).size() > 14) {
            fail(QStringLiteral("Rufzeichen \"%1\" ist %2 Zeichen lang, erlaubt sind 3 bis 14")
                     .arg(f.at(2).left(30))
                     .arg(f.at(2).size()));
        }
        exactly(3, QStringLiteral("Mode code"), {1});
        exactly(4, QStringLiteral("Sent-RST"), {0, 2, 3});
        exactly(5, QStringLiteral("Sent QSO number"), {0, 3, 4});
        exactly(6, QStringLiteral("Received-RST"), {0, 2, 3});
        exactly(7, QStringLiteral("Received QSO number"), {0, 3, 4});
        upTo(8, QStringLiteral("Received Exchange"), 6);
        exactly(9, QStringLiteral("Received WWL"), {0, 4, 6});
        if (f.at(10).isEmpty() || f.at(10).size() > 6) {
            fail(QStringLiteral("QSO points \"%1\" ist %2 Zeichen lang, erlaubt sind 1 bis 6")
                     .arg(f.at(10))
                     .arg(f.at(10).size()));
        }
        for (int c = 11; c <= 14; ++c) {
            upTo(c, QStringLiteral("Spalte %1").arg(c + 1), 1);
        }
    }
}

void checkHeaderLengths(const QString& file)
{
    for (const QString& line : file.split(QStringLiteral("\r\n"), Qt::SkipEmptyParts)) {
        for (const QString& key : {QStringLiteral("PWWLo"), QStringLiteral("PExch")}) {
            if (line.startsWith(key + QLatin1Char('='))) {
                const QString value = line.mid(key.size() + 1);
                QVERIFY2(value.size() <= 6,
                         qPrintable(QStringLiteral("%1 ist %2 Zeichen lang, erlaubt sind sechs")
                                        .arg(key)
                                        .arg(value.size())));
            }
        }
    }
}

void checkWholeFile(const QString& file)
{
    checkCharacterSet(file);
    checkLineLengths(file);
    checkQsoColumns(file);
    checkHeaderLengths(file);
}

} // namespace

class TestEdiSpezifikation : public QObject
{
    Q_OBJECT

private slots:
    void umschriftFuerDieAbgabe();
    void gewoehnlicherContestlauf();
    void umlauteKommenAlsAsciiHeraus();
    void langeAngabenSprengenDieZeileNicht();
    void unsinnAusEinemImportWirdZurechtgebogen();
};

// Die Umschrift selbst (core/AsciiText.h): lesbar bleiben, nicht
// bloss ASCII werden.
void TestEdiSpezifikation::umschriftFuerDieAbgabe()
{
    QCOMPARE(asciiOnly(QStringLiteral("Österreich")), QStringLiteral("Oesterreich"));
    QCOMPARE(asciiOnly(QStringLiteral("ÖSTERREICH")), QStringLiteral("OESTERREICH"));
    QCOMPARE(asciiOnly(QStringLiteral("Musterstraße 1")), QStringLiteral("Musterstrasse 1"));
    QCOMPARE(asciiOnly(QStringLiteral("10 m über Grund")), QStringLiteral("10 m ueber Grund"));
    QCOMPARE(asciiOnly(QStringLiteral("Jörg Müller")), QStringLiteral("Joerg Mueller"));
    // Akzente fallen weg, der Buchstabe bleibt.
    QCOMPARE(asciiOnly(QStringLiteral("José Sørensen")), QStringLiteral("Jose Soerensen"));
    QCOMPARE(asciiOnly(QStringLiteral("Hrádek")), QStringLiteral("Hradek"));
    // Zeilenumbruch mitten im Feld wird ein Leerzeichen, nicht eine
    // zweite Zeile in der Abgabedatei.
    QCOMPARE(asciiOnly(QStringLiteral("Zeile1\nZeile2")), QStringLiteral("Zeile1 Zeile2"));
    // Was reines ASCII ist, bleibt Zeichen fuer Zeichen gleich.
    const QString ascii = QStringLiteral("OE5SOS/P 59 001 JN67UT; 10 m");
    QCOMPARE(asciiOnly(ascii), ascii);
}

// Der Normalfall: ein Log, wie es am Feuerkogel entsteht, muss die
// Spezifikation Zeile fuer Zeile einhalten.
void TestEdiSpezifikation::gewoehnlicherContestlauf()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("spec.sqlite")), QStringLiteral("edi_spec_1")));

    QString defError;
    const ContestDefinition definition = ContestDefinition::loadFromJson(QByteArray(kIaruJson), &defError);
    QVERIFY2(definition.isValid(), qPrintable(defError));

    QsoRecord qso1 = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"),
                                 QStringLiteral("14:01"), 1, 3, QStringLiteral("JN58SD"), 187.4);
    QVERIFY(db.insertQso(qso1));
    QsoRecord qso2 = makeQso(QStringLiteral("OE3XYZ"), QStringLiteral("144"), QStringLiteral("CW"),
                                 QStringLiteral("14:11"), 2, 17, QStringLiteral("JN88TC"), 214.6);
    QVERIFY(db.insertQso(qso2));
    // Ein Portabelrufzeichen mit Schraegstrich und ein vierstelliger
    // Locator, wie ihn mancher durchgibt.
    QsoRecord portable = makeQso(QStringLiteral("DL1ABC/P"), QStringLiteral("144"), QStringLiteral("SSB"),
                                 QStringLiteral("14:30"), 3, 4, QStringLiteral("JN59"), 250.0);
    QVERIFY(db.insertQso(portable));

    const QString file = EdiExporter(db).exportBand(definition.id(), QStringLiteral("144"), definition,
                                                    feuerkogel(), stationMitUmlauten());
    checkWholeFile(file);
}

// Oesterreich, Musterstrasse, "ueber Grund": alles das muss in der
// Datei als reines ASCII stehen -- und lesbar bleiben, nicht als
// Fragezeichen.
void TestEdiSpezifikation::umlauteKommenAlsAsciiHeraus()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("spec2.sqlite")), QStringLiteral("edi_spec_2")));

    QString defError;
    const ContestDefinition definition = ContestDefinition::loadFromJson(QByteArray(kIaruJson), &defError);
    QVERIFY2(definition.isValid(), qPrintable(defError));
    QsoRecord qso3 = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"),
                                 QStringLiteral("14:01"), 1, 3, QStringLiteral("JN58SD"), 187.4);
    QVERIFY(db.insertQso(qso3));

    const QString file = EdiExporter(db).exportBand(definition.id(), QStringLiteral("144"), definition,
                                                    feuerkogel(), stationMitUmlauten());
    checkWholeFile(file);

    QVERIFY2(file.contains(QStringLiteral("RCoun=Oesterreich")), qPrintable(file.left(900)));
    QVERIFY2(file.contains(QStringLiteral("RAdr1=Musterstrasse 1")), qPrintable(file.left(900)));
    QVERIFY2(file.contains(QStringLiteral("PAdr1=Feuerkogel, Oberoesterreich")), qPrintable(file.left(900)));
    QVERIFY2(file.contains(QStringLiteral("ueber Grund")), qPrintable(file.left(900)));
    // Keine Fragezeichen als Ersatz: der Robot soll lesen koennen, wer
    // da gefunkt hat.
    QVERIFY(!file.contains(QLatin1Char('?')));
}

// Eine ausfuehrliche Geraete- und Antennenbeschreibung darf die
// 75-Zeichen-Grenze nicht sprengen.
void TestEdiSpezifikation::langeAngabenSprengenDieZeileNicht()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("spec3.sqlite")), QStringLiteral("edi_spec_3")));

    QString defError;
    const ContestDefinition definition = ContestDefinition::loadFromJson(QByteArray(kIaruJson), &defError);
    QVERIFY2(definition.isValid(), qPrintable(defError));
    QsoRecord qso4 = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"),
                                 QStringLiteral("14:01"), 1, 3, QStringLiteral("JN58SD"), 187.4);
    QVERIFY(db.insertQso(qso4));

    EdiStationInfo station = stationMitUmlauten();
    station.antenna = QStringLiteral("4 x 17 Element DK7ZB gekreuzt auf 12 m Alu-Steckmast, "
                                     "Vorverstaerker SP2000, Phasenleitung Ecoflex 15");
    station.txEquipment = QStringLiteral("Elecraft K3 als ZF mit Transverter DB6NT MK2 und "
                                         "600-W-Endstufe, Umschaltung ueber Sequenzer");
    station.locationLine1 = QStringLiteral("Feuerkogel bei Ebensee am Traunsee, Oberoesterreich, "
                                           "1592 m ueber dem Meer");
    station.name = QStringLiteral("Ralph Martin Fischer, verantwortlicher Operator der Gruppe");

    const QString file = EdiExporter(db).exportBand(definition.id(), QStringLiteral("144"), definition,
                                                    feuerkogel(), station);
    checkWholeFile(file);
}

// Was ueber einen Import in die Datenbank kommt, hat kein Eingabefeld
// begrenzt: ein achtstelliger Locator, ein viel zu langes Rufzeichen,
// ein RST mit vier Stellen. Die Datei muss trotzdem der Spezifikation
// entsprechen.
void TestEdiSpezifikation::unsinnAusEinemImportWirdZurechtgebogen()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("spec4.sqlite")), QStringLiteral("edi_spec_4")));

    QString defError;
    const ContestDefinition definition = ContestDefinition::loadFromJson(QByteArray(kIaruJson), &defError);
    QVERIFY2(definition.isValid(), qPrintable(defError));

    // Achtstelliger Locator (manche Logs fuehren ihn so).
    QsoRecord lang = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("SSB"),
                             QStringLiteral("14:01"), 1, 3, QStringLiteral("JN58SD12"), 187.4);
    QVERIFY(db.insertQso(lang));

    // Rufzeichen weit jenseits der 14 Zeichen.
    QsoRecord monster = makeQso(QString(60, QLatin1Char('X')), QStringLiteral("144"), QStringLiteral("SSB"),
                                QStringLiteral("14:05"), 2, 4, QStringLiteral("JN58SE"), 190.0);
    QVERIFY(db.insertQso(monster));

    // RST mit vier Stellen (das Eingabefeld laesst vier zu).
    QsoRecord rst4 = makeQso(QStringLiteral("DL2ABC"), QStringLiteral("144"), QStringLiteral("SSB"),
                             QStringLiteral("14:09"), 3, 5, QStringLiteral("JN58SF"), 191.0);
    rst4.rstSent = QStringLiteral("5999");
    rst4.rstRcvd = QStringLiteral("5999");
    QVERIFY(db.insertQso(rst4));

    // Ein unbrauchbarer Locator: die Spezifikation kennt 0, 4 oder 6
    // Zeichen und nichts dazwischen.
    QsoRecord krumm = makeQso(QStringLiteral("DL3ABC"), QStringLiteral("144"), QStringLiteral("SSB"),
                              QStringLiteral("14:12"), 4, 6, QStringLiteral("JN5"), 0.0);
    QVERIFY(db.insertQso(krumm));

    // Auch der EIGENE Locator kann achtstellig in den Einstellungen
    // stehen; PWWLo erlaubt sechs Zeichen.
    ContestSettings settings = feuerkogel();
    settings.ownGrid = QStringLiteral("JN67UT55");

    const QString file = EdiExporter(db).exportBand(definition.id(), QStringLiteral("144"), definition,
                                                    settings, stationMitUmlauten());
    checkWholeFile(file);
    QVERIFY(file.contains(QStringLiteral("PWWLo=JN67UT\r\n")));

    // Der achtstellige Locator wird auf den gueltigen sechsstelligen
    // gekuerzt, nicht weggeworfen.
    QVERIFY2(file.contains(QStringLiteral(";JN58SD;")), qPrintable(file.section(QStringLiteral("[QSORecords"), 1)));
}

QTEST_MAIN(TestEdiSpezifikation)
#include "test_edi_spezifikation.moc"
