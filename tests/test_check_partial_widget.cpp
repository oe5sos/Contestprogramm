#include <QtTest>

#include <QApplication>
#include <QLabel>
#include <QSignalSpy>

#include "core/CheckPartialIndex.h"
#include "ui/CheckPartialWidget.h"

using namespace Contestprogramm;

namespace {

QString plainText(const CheckPartialWidget& widget)
{
    QString all;
    for (const QLabel* label : widget.findChildren<QLabel*>()) {
        QTextDocument doc;
        doc.setHtml(label->text());
        all += doc.toPlainText() + QLatin1Char('\n');
    }
    all.replace(QChar(0x00A0), QLatin1Char(' '));
    return all;
}

} // namespace

class TestCheckPartialWidget : public QObject
{
    Q_OBJECT

private slots:
    void rendersMatchesAndEmitsTheClickedCall();
    void statusLineOffersScpLoadUntilOneIsLoaded();
};

void TestCheckPartialWidget::rendersMatchesAndEmitsTheClickedCall()
{
    CheckPartialWidget widget;
    QVERIFY(plainText(widget).contains(QStringLiteral("—")) || plainText(widget).contains(QStringLiteral("-")));

    CheckPartialMatch known;
    known.callsign = QStringLiteral("OE5XYZ");
    known.grid = QStringLiteral("JN67UT");
    known.sources = CheckPartialMatch::History;
    CheckPartialMatch dupe;
    dupe.callsign = QStringLiteral("DL1ABC");
    dupe.sources = CheckPartialMatch::Log;
    dupe.workedThisBand = true;
    dupe.workedBands = {QStringLiteral("144")};
    CheckPartialMatch otherBand;
    otherBand.callsign = QStringLiteral("OE5XYB");
    otherBand.sources = CheckPartialMatch::Log;
    otherBand.workedBands = {QStringLiteral("432")};
    CheckPartialMatch near;
    near.callsign = QStringLiteral("OE5XYA");
    near.nearMiss = true;
    near.sources = CheckPartialMatch::Scp;
    widget.setMatches(QStringLiteral("OE5X"), {known, dupe, near, otherBand});

    const QString text = plainText(widget);
    QVERIFY2(text.contains(QStringLiteral("OE5XYZ JN67")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("DL1ABC")), qPrintable(text));
    // Worked on the current band: no band hint (it is struck through);
    // worked elsewhere: the "✓432" hint.
    QVERIFY2(!text.contains(QStringLiteral("DL1ABC ✓")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("OE5XYB ✓432")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("≈OE5XYA")), qPrintable(text));

    // The link carries call and grid; activating it hands both on.
    QSignalSpy chosen(&widget, &CheckPartialWidget::callsignChosen);
    QLabel* matchesLabel = widget.findChildren<QLabel*>().first();
    emit matchesLabel->linkActivated(QStringLiteral("OE5XYZ|JN67UT"));
    QCOMPARE(chosen.size(), 1);
    QCOMPARE(chosen.first().at(0).toString(), QStringLiteral("OE5XYZ"));
    QCOMPARE(chosen.first().at(1).toString(), QStringLiteral("JN67UT"));

    widget.setMatches(QStringLiteral("ZZ9"), {});
    QVERIFY2(plainText(widget).contains(QStringLiteral("ZZ9: kein Treffer")), qPrintable(plainText(widget)));
}

void TestCheckPartialWidget::statusLineOffersScpLoadUntilOneIsLoaded()
{
    CheckPartialWidget widget;
    widget.setSources(0, QString(), 12, 3);
    QString text = plainText(widget);
    QVERIFY2(text.contains(QStringLiteral("SCP-LISTE LADEN")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("HISTORIE 12")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("GEHÖRT 3")), qPrintable(text));

    QSignalSpy load(&widget, &CheckPartialWidget::scpLoadRequested);
    QLabel* statusLabel = widget.findChildren<QLabel*>().last();
    emit statusLabel->linkActivated(QStringLiteral("scp"));
    QCOMPARE(load.size(), 1);

    widget.setSources(38412, QStringLiteral("VHF.scp"), 12, 3);
    text = plainText(widget);
    QVERIFY2(text.contains(QStringLiteral("SCP 38412 (VHF.scp)")), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("LADEN")), qPrintable(text));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestCheckPartialWidget tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_check_partial_widget.moc"
