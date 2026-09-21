#include <QtTest>

#include <QApplication>
#include <QImage>
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
    QString all = widget.readingsText();
    all.replace(QChar(0x00A0), QLatin1Char(' '));
    all.replace(QChar(0x202F), QLatin1Char(' '));
    return all;
}

// How many distinct colours a rendering of the widget at `size` has --
// a painted instrument with chips and readings has many, an unpainted
// or blank one a handful.
int distinctColours(RateMeterWidget& widget, const QSize& size)
{
    widget.resize(size);
    QImage image(size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    widget.render(&image);
    QSet<QRgb> colours;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            colours.insert(image.pixel(x, y));
        }
    }
    return colours.size();
}

} // namespace

// The "Punkte"/"ODX" readings RateMeterWidget gained with the km scoring
// (see data/ContestScoring.h): dash without an own locator, grouped km
// per band plus the sum and the ODX once the locator is known -- and
// the layout that follows the panel's size.
class TestRateMeterScore : public QObject
{
    Q_OBJECT

private slots:
    void showsDashWithoutOwnLocatorAndKmWithIt();
    void withoutASourceEveryReadingIsADash();
    void layoutFollowsTheSize();
    void paintsInBothLayouts();
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
    QVERIFY2(text.contains(QStringLiteral("QSOs 3 (144: 2 · 432: 1)")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Punkte —— (kein eigener Locator)")), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("4 814")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("ODX ——")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Felder 3")), qPrintable(text)); // JN58, JN88, JN67 -- squares need no own grid

    widget.setScoring(QStringLiteral("JN67UT"), {QStringLiteral("144"), QStringLiteral("432")},
                      QStringLiteral("distance_km"));
    text = visibleText(widget);
    QVERIFY2(text.contains(QStringLiteral("Punkte 4 815 (144: 4 814 · 432: 1)")), qPrintable(text)); // 4613 + 201 (truncated + 1 each), grouped; same-square QSO floor of 1
    QVERIFY2(text.contains(QStringLiteral("ODX 4 613 km (DL1ABC · JN58SD)")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Felder 3 (144: 2 · 432: 1)")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("(beste 3 (14z))")), qPrintable(text));

    // An invalidated QSO leaves the band's points at 0 and the squares
    // without its one.
    QVERIFY(db.setQsoInvalid(c.id, true));
    widget.refresh();
    text = visibleText(widget);
    QVERIFY2(text.contains(QStringLiteral("Punkte 4 814 (144: 4 814 · 432: 0)")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Felder 2 (144: 2 · 432: 0)")), qPrintable(text));
}

void TestRateMeterScore::withoutASourceEveryReadingIsADash()
{
    RateMeterWidget widget;
    const QString text = visibleText(widget);
    QVERIFY2(text.contains(QStringLiteral("QSOs ——")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("Punkte ——")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("10 min ——")), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral(" 0")), qPrintable(text));
}

// Chosen from the size alone: the panel's default 270×130 and any tall
// shape are tiles; a low, wide panel is the strip.
void TestRateMeterScore::layoutFollowsTheSize()
{
    QCOMPARE(RateMeterWidget::layoutFor(QSize(270, 130)), RateMeterWidget::Layout::Tiles);
    QCOMPARE(RateMeterWidget::layoutFor(QSize(270, 300)), RateMeterWidget::Layout::Tiles);
    QCOMPARE(RateMeterWidget::layoutFor(QSize(760, 130)), RateMeterWidget::Layout::Strip);
    QCOMPARE(RateMeterWidget::layoutFor(QSize(660, 220)), RateMeterWidget::Layout::Strip);
    QCOMPARE(RateMeterWidget::layoutFor(QSize(660, 221)), RateMeterWidget::Layout::Tiles); // too tall for a strip
    QCOMPARE(RateMeterWidget::layoutFor(QSize(659, 130)), RateMeterWidget::Layout::Tiles); // too narrow for one
    QCOMPARE(RateMeterWidget::layoutFor(QSize(1200, 400)), RateMeterWidget::Layout::Tiles); // taller than a band
}

void TestRateMeterScore::paintsInBothLayouts()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("paint.sqlite")), QStringLiteral("rate_meter_paint")));
    for (int i = 0; i < 12; ++i) {
        QsoRecord r = makeQso(QStringLiteral("DL%1ABC").arg(i), i % 3 == 0 ? QStringLiteral("432") : QStringLiteral("144"),
                              QStringLiteral("JN58SD"), 300.0 + i);
        r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-i * 900).toString(Qt::ISODate);
        QVERIFY(db.insertQso(r));
    }
    RateMeterWidget widget;
    widget.setSource(&db, QStringLiteral("IARU_R1_VHF_UHF"));
    widget.setScoring(QStringLiteral("JN67UT"), {QStringLiteral("144"), QStringLiteral("432")},
                      QStringLiteral("distance_km"));

    // Every size paints something substantial and none crashes -- from
    // the tiny corner a panel can be dragged to, through the default
    // tiles, to the wide strip with its sparkline.
    for (const QSize& size : {QSize(90, 40), QSize(270, 130), QSize(270, 260), QSize(400, 300), QSize(700, 130),
                              QSize(900, 130), QSize(900, 200)}) {
        QVERIFY2(distinctColours(widget, size) > 12, qPrintable(QStringLiteral("%1x%2").arg(size.width()).arg(size.height())));
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestRateMeterScore tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_rate_meter_score.moc"
