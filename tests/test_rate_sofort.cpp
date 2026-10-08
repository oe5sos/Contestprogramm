// Das Rate-Panel hat einen eigenen Takt (RateMeterWidget: 15 s). Beim
// Durchleuchten am 2026-10-08 stand deshalb nach zwei geloggten QSOs
// im Rate-Panel "QSOS 0 / PUNKTE 0", waehrend das Kartenpanel daneben
// schon "2 . 569" zeigte -- zwei Anzeigen desselben Wertes, die sich
// widersprachen, bis zu einer Viertelminute lang.
//
// Hier wird mit echten Tastendruecken geloggt und sofort danach
// nachgesehen, ohne auf einen Takt zu warten. Gegen die Fassung vor
// dem 2026-10-08 faellt dieser Pruefstand.

#include <QtTest>

#include <QAbstractItemModel>
#include <QApplication>
#include <QLineEdit>
#include <QTableView>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
#include "ui/RateMeterWidget.h"
#include "ui/UnifiedLogWidget.h"

#include <memory>

using namespace Contestprogramm;

namespace {

QList<QLineEdit*> entryFields(MainWindow& window)
{
    auto* row = window.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowObjectName));
    return row ? row->findChildren<QLineEdit*>() : QList<QLineEdit*>{};
}

} // namespace

class TestRateSofort : public QObject
{
    Q_OBJECT

private slots:
    void einGeloggtesQsoStehtSofortImRatePanel();
};

void TestRateSofort::einGeloggtesQsoStehtSofortImRatePanel()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppController controller;
    QVERIFY(controller.openDatabase(dir.filePath(QStringLiteral("rate.sqlite"))));
    ContestSettings settings = controller.settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    controller.setSettings(settings);

    MainWindow window(controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.activateWindow();
    (void)QTest::qWaitForWindowActive(&window);

    auto* rate = window.findChild<RateMeterWidget*>();
    QVERIFY(rate);
    const QString vorher = rate->readingsText();
    qInfo().noquote() << "vor dem QSO:" << vorher;

    const QList<QLineEdit*> fields = entryFields(window);
    QVERIFY(fields.size() >= 4);
    fields.at(0)->setFocus();
    QTest::keyClicks(fields.at(0), QStringLiteral("DL1ABC"));
    QTest::keyClick(fields.at(0), Qt::Key_Tab);
    QTest::keyClicks(QApplication::focusWidget(), QStringLiteral("001"));
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
    QTest::keyClicks(QApplication::focusWidget(), QStringLiteral("JO60AA"));
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Return);
    QCoreApplication::processEvents();

    QCOMPARE(controller.database().qsoCountForContest(settings.activeContestId), 1);
    const QString nachher = rate->readingsText();
    qInfo().noquote() << "sofort nach dem QSO:" << nachher;
    QVERIFY2(nachher != vorher,
             "Das Rate-Panel zeigt nach dem Loggen noch den alten Stand -- "
             "es wartet auf seinen eigenen 15-Sekunden-Takt");
    // JN67UT -> JO60AA sind 274 km, nach IARU-R1-Wertung also 275 Punkte
    // (abgerundete Kilometer plus 1).
    QVERIFY2(nachher.contains(QStringLiteral("275")),
             qPrintable(QStringLiteral("Punkte fehlen im Rate-Panel: %1").arg(nachher)));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestRateSofort tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_rate_sofort.moc"
