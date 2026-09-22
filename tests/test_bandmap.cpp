#include <QtTest>

#include <QApplication>
#include <QMouseEvent>
#include <QSignalSpy>

#include "core/BandmapModel.h"
#include "core/SpotCandidate.h"
#include "ui/BandmapWidget.h"
#include "ui/StyleKit.h"

using namespace Contestprogramm;

namespace {

SpotCandidate spot(const QString& call, qint64 hz, const QString& grid = QString(), int minutesAgo = 0)
{
    SpotCandidate c;
    c.callsign = call;
    c.grid = grid;
    c.freqHz = hz;
    c.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-60 * minutesAgo);
    c.source = QStringLiteral("cluster");
    return c;
}

QStringList calls(const QVector<BandmapSpot>& spots)
{
    QStringList result;
    for (const BandmapSpot& s : spots) {
        result << s.callsign;
    }
    return result;
}

} // namespace

class TestBandmap : public QObject
{
    Q_OBJECT

private slots:
    void modelKeepsNewestPerCallSortsByFrequencyAndAgesOut();
    void modelIgnoresSpotsWithoutFrequencyAndFiltersByBand();
    void widgetRangeFollowsSpotsAndOwnFrequency();
    void widgetStacksOverlappingLabelsAndEmitsOnClick();
    void aNeededMultiplierIsDrawnInAmber();
};

void TestBandmap::modelKeepsNewestPerCallSortsByFrequencyAndAgesOut()
{
    BandmapModel model;
    model.addSpot(spot(QStringLiteral("DL1ABC"), 144300000, QStringLiteral("JN58SD")));
    model.addSpot(spot(QStringLiteral("OE3XYZ"), 144250000));
    model.addSpot(spot(QStringLiteral("HB9OLD"), 144280000, QString(), 45)); // beyond the 30 min limit
    // Re-spot moves the call and keeps the earlier grid.
    model.addSpot(spot(QStringLiteral("dl1abc"), 144310000));
    QCOMPARE(model.count(), 3);

    const auto now = QDateTime::currentDateTimeUtc();
    const QVector<BandmapSpot> on2m = model.spotsForBand(QStringLiteral("144"), now);
    QCOMPARE(calls(on2m), (QStringList{QStringLiteral("OE3XYZ"), QStringLiteral("DL1ABC")}));
    QCOMPARE(on2m.last().freqHz, qint64(144310000));
    QCOMPARE(on2m.last().grid, QStringLiteral("JN58SD"));
    QCOMPARE(model.count(), 2); // the expired one is gone for good
}

void TestBandmap::modelIgnoresSpotsWithoutFrequencyAndFiltersByBand()
{
    BandmapModel model;
    model.addSpot(spot(QStringLiteral("OE5XYZ"), 0));
    model.addSpot(spot(QStringLiteral("OE1AAA"), 432200000));
    model.addSpot(spot(QStringLiteral("OE2BBB"), 144200000));
    QCOMPARE(model.count(), 2);
    const auto now = QDateTime::currentDateTimeUtc();
    QCOMPARE(calls(model.spotsForBand(QStringLiteral("432"), now)), QStringList{QStringLiteral("OE1AAA")});
    QCOMPARE(calls(model.spotsForBand(QStringLiteral("144"), now)), QStringList{QStringLiteral("OE2BBB")});
    QVERIFY(model.spotsForBand(QStringLiteral("1296"), now).isEmpty());
}

void TestBandmap::widgetRangeFollowsSpotsAndOwnFrequency()
{
    BandmapWidget widget;
    widget.resize(250, 300);
    // Nothing known: the band's usual segment.
    widget.setBand(QStringLiteral("144"));
    QCOMPARE(widget.rangeLowHz(), qint64(144000000));
    QCOMPARE(widget.rangeHighHz(), qint64(144400000));

    // Only the rig: ±30 kHz (the 60 kHz minimum span) around it.
    widget.setOwnFrequencyHz(144300000);
    QCOMPARE(widget.rangeLowHz(), qint64(144270000));
    QCOMPARE(widget.rangeHighHz(), qint64(144330000));

    // Spots widen it, with a 5 kHz margin, and still include the rig.
    BandmapSpot a;
    a.callsign = QStringLiteral("DL1ABC");
    a.freqHz = 144200000;
    BandmapSpot b;
    b.callsign = QStringLiteral("OE3XYZ");
    b.freqHz = 144380000;
    widget.setSpots({a, b});
    QCOMPARE(widget.rangeLowHz(), qint64(144195000));
    QCOMPARE(widget.rangeHighHz(), qint64(144385000));

    // Kurzwelle: kein eigener Ausschnitt nötig, dort wird das ganze
    // Band gearbeitet -- die Achse nimmt die Bandgrenzen. Vorher fiel
    // jedes Band ohne Eintrag auf 0 .. 60 kHz durch und zeichnete eine
    // Skala, die bei "0.000" begann.
    BandmapWidget hf;
    hf.resize(250, 300);
    hf.setBand(QStringLiteral("14"));
    QCOMPARE(hf.rangeLowHz(), qint64(14000000));
    QCOMPARE(hf.rangeHighHz(), qint64(14350000));
    // 160 m hat einen: die Zuteilung geht bis 2,000, gearbeitet wird
    // der untere Streifen.
    hf.setBand(QStringLiteral("1.8"));
    QCOMPARE(hf.rangeLowHz(), qint64(1810000));
    QCOMPARE(hf.rangeHighHz(), qint64(1850000));
    // Ein Band, das die Tabelle nicht kennt: unverändert der
    // Mindestausschnitt.
    hf.setBand(QStringLiteral("23cm"));
    QCOMPARE(hf.rangeLowHz(), qint64(0));
    QCOMPARE(hf.rangeHighHz(), qint64(60000));
}

void TestBandmap::widgetStacksOverlappingLabelsAndEmitsOnClick()
{
    BandmapWidget widget;
    widget.resize(250, 300);
    widget.setBand(QStringLiteral("144"));
    BandmapSpot a;
    a.callsign = QStringLiteral("DL1ABC");
    a.freqHz = 144300000;
    a.grid = QStringLiteral("JN58SD");
    BandmapSpot b;
    b.callsign = QStringLiteral("OE3XYZ");
    b.freqHz = 144300500; // 500 Hz away: the labels would overlap
    b.worked = true;
    BandmapSpot c;
    c.callsign = QStringLiteral("HB9FAR");
    c.freqHz = 144350000;
    widget.setSpots({a, b, c});

    const QVector<QRect> rects = widget.labelRects();
    QCOMPARE(rects.size(), 3);
    QVERIFY(rects.at(1).top() >= rects.at(0).bottom());  // pushed below the first
    QVERIFY(rects.at(2).top() > rects.at(1).bottom());   // far enough to sit on its own
    QVERIFY(rects.at(0).top() < rects.at(2).top());      // lowest frequency at the top

    QSignalSpy activated(&widget, &BandmapWidget::spotActivated);
    const QPoint onFirst = rects.at(0).center();
    QMouseEvent press(QEvent::MouseButtonPress, onFirst, widget.mapToGlobal(onFirst), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&widget, &press);
    QCOMPARE(activated.size(), 1);
    QCOMPARE(activated.first().at(0).toString(), QStringLiteral("DL1ABC"));
    QCOMPARE(activated.first().at(1).toString(), QStringLiteral("JN58SD"));
    QCOMPARE(activated.first().at(2).toLongLong(), qint64(144300000));

    // A click on empty canvas activates nothing.
    const QPoint nowhere(widget.width() - 2, widget.height() - 2);
    QMouseEvent miss(QEvent::MouseButtonPress, nowhere, widget.mapToGlobal(nowhere), Qt::LeftButton, Qt::LeftButton,
                     Qt::NoModifier);
    QApplication::sendEvent(&widget, &miss);
    QCOMPARE(activated.size(), 1);

    // Painting a widget with stacked labels must not crash off-screen.
    widget.grab();
}

// Ein Spot, der einen fehlenden Multiplikator brächte, steht in
// Bernstein -- er zählt mehr als ein QSO. Geprüft am Bild: dieselbe
// Bandmap zweimal, einmal mit und einmal ohne die Kennzeichnung.
void TestBandmap::aNeededMultiplierIsDrawnInAmber()
{
    BandmapWidget widget;
    widget.resize(250, 300);
    widget.setBand(QStringLiteral("144"));

    BandmapSpot spot;
    spot.callsign = QStringLiteral("DL1ABC");
    spot.freqHz = 144300000;
    spot.worked = false;

    widget.setSpots({spot});
    const QImage plain = widget.grab().toImage();

    spot.neededMultiplier = true;
    widget.setSpots({spot});
    const QImage amber = widget.grab().toImage();

    QVERIFY2(plain != amber, "Die Kennzeichnung kommt im Bild nicht an");

    // Und die Farbe ist wirklich der Bernstein des Hauses.
    const QRgb amberRgb = QColor(Style::kAmberText()).rgb();
    bool found = false;
    for (int y = 0; y < amber.height() && !found; ++y) {
        for (int x = 0; x < amber.width(); ++x) {
            if ((amber.pixel(x, y) | 0xff000000) == (amberRgb | 0xff000000)) {
                found = true;
                break;
            }
        }
    }
    QVERIFY2(found, "Kein einziger Bildpunkt in Bernstein");
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestBandmap tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_bandmap.moc"
