#include <QtTest>

#include <QApplication>
#include <QLabel>
#include <QTemporaryDir>

#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/RateMeterWidget.h"

using namespace Contestprogramm;

namespace {

QsoRecord makeQso(const QString& call, const QString& band, const QString& grid, double km)
{
    QsoRecord r;
    r.callsign = call;
    r.band = band;
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QStringLiteral("2026-10-03T14:01:00Z");
    r.gridSquare = grid;
    r.distanceKm = km;
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    return r;
}

// Everything the widget currently shows, as one plain-text blob. The
// Austrian locale groups digits with a (narrow) no-break space; both
// are folded to a plain space so the expectations below can be typed.
QString visibleText(const RateMeterWidget& widget)
{
    QString all;
    for (const QLabel* label : widget.findChildren<QLabel*>()) {
        QTextDocument doc;
        doc.setHtml(label->text());
        all += doc.toPlainText() + QLatin1Char('\n');
    }
    all.replace(QChar(0x00A0), QLatin1Char(' '));
    all.replace(QChar(0x202F), QLatin1Char(' '));
    return all;
}

} // namespace

// The "Punkte"/"ODX" rows RateMeterWidget gained with the km scoring
// (see data/ContestScoring.h): dash without an own locator, grouped km
// per band plus the sum and the ODX once the locator is known.
class TestRateMeterScore : public QObject
{
    Q_OBJECT

private slots:
    void showsDashWithoutOwnLocatorAndKmWithIt();
};

void TestRateMeterScore::showsDashWithoutOwnLocatorAndKmWithIt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("score.sqlite")), QStringLiteral("rate_meter_score")));

    QsoRecord a = makeQso(QStringLiteral("DL1ABC"), QStringLiteral("144"), QStringLiteral("JN58SD"), 4612.4);
    QsoRecord b = makeQso(QStringLiteral("OE3XYZ"), QStringLiteral("144"), QStringLiteral("JN88TC"), 200.0);
    QsoRecord c = makeQso(QStringLiteral("OE5XYZ"), QStringLiteral("432"), QStringLiteral("JN67UT"), 0.0);
    QVERIFY(db.insertQso(a));
    QVERIFY(db.insertQso(b));
    QVERIFY(db.insertQso(c));

    RateMeterWidget widget;
    widget.setSource(&db, QStringLiteral("IARU_R1_VHF_UHF"));

    // No own grid yet: the score is unknown, not zero.
    widget.setScoring(QString(), {QStringLiteral("144"), QStringLiteral("432")}, QStringLiteral("distance_km"));
    QString text = visibleText(widget);
    QVERIFY2(text.contains(QStringLiteral("PUNKTE")), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("4 814")), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("ODX")), qPrintable(text));

    widget.setScoring(QStringLiteral("JN67UT"), {QStringLiteral("144"), QStringLiteral("432")},
                      QStringLiteral("distance_km"));
    text = visibleText(widget);
    QVERIFY2(text.contains(QStringLiteral("144: 4 814")), qPrintable(text)); // 4613 + 201 (truncated + 1 each), grouped
    QVERIFY2(text.contains(QStringLiteral("432: 1")), qPrintable(text));     // same-square QSO, floor of 1
    QVERIFY2(text.contains(QStringLiteral("Σ: 4 815")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("ODX DL1ABC JN58SD 4 613 km")), qPrintable(text));

    // Only one band worked: the sum would just repeat the band's number.
    QVERIFY(db.setQsoInvalid(c.id, true));
    widget.refresh();
    text = visibleText(widget);
    QVERIFY2(text.contains(QStringLiteral("144: 4 814  432: 0")), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("Σ")), qPrintable(text));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestRateMeterScore tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_rate_meter_score.moc"
