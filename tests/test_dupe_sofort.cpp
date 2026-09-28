// Aus Martins Arbeitsanweisung fuer die UKW-Conteste (2026-09-27):
// "dupe solle sofort mti uhrzeiz angezeigt werden" -- und praeziser am
// 2026-09-21: "sollte ein dupe kommen soll sofort die nummer stehen, mit
// der ich geloggt habe, inkl. uhrzeit".
//
// "Sofort" heisst: waehrend des Tippens, nicht erst beim Return. Dieser
// Pruefstand tippt ein schon gearbeitetes Rufzeichen Zeichen fuer Zeichen
// in die echte Eingabezeile des echten Fensters und sieht nach, ab wann
// die DUPE-Pille steht und was daneben geschrieben ist.

#include <QtTest>

#include <QAbstractItemModel>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QTableView>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/MainWindow.h"
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

class TestDupeSofort : public QObject
{
    Q_OBJECT

private slots:
    void aWorkedCallsignShowsDupeWithItsNumberAndTimeWhileTyping();
    void aFreshCallsignShowsNoDupe();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
};

std::unique_ptr<AppController> TestDupeSofort::makeController(QTemporaryDir& dir, const QString& file)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(file))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67UT");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    controller->setSettings(settings);
    return controller;
}

void TestDupeSofort::aWorkedCallsignShowsDupeWithItsNumberAndTimeWhileTyping()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("dupe.sqlite"));
    QVERIFY(controller);

    // Ein QSO, das schon im Log steht -- mit einer Uhrzeit, die im
    // Hinweis wiederauftauchen muss.
    QsoRecord fruehes;
    fruehes.callsign = QStringLiteral("DL1ABC");
    fruehes.band = QStringLiteral("144");
    fruehes.mode = QStringLiteral("SSB");
    fruehes.timestampUtc = QDateTime(QDate::currentDate(), QTime(14, 37), QTimeZone::UTC).toString(Qt::ISODate);
    fruehes.rstSent = QStringLiteral("59");
    fruehes.rstRcvd = QStringLiteral("59");
    fruehes.serialSent = 7;
    fruehes.serialRcvd = 14;
    fruehes.gridSquare = QStringLiteral("JN58SD");
    fruehes.distanceKm = 165.0;
    fruehes.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    QVERIFY(controller->database().insertQso(fruehes));

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* pille = window.findChild<QLabel*>(QLatin1String(UnifiedLogWidget::kStatusPillObjectName));
    QVERIFY2(pille, "Es gibt keine Status-Pille");
    auto* hinweis = window.findChild<QLabel*>(QLatin1String(UnifiedLogWidget::kLastQsoLabelObjectName));
    QVERIFY2(hinweis, "Es gibt keine Hinweiszeile neben der Eingabe");

    const QList<QLineEdit*> fields = entryFields(window);
    QVERIFY(fields.size() >= 4);
    QLineEdit* callField = fields.at(0);
    callField->setFocus();

    // Zeichen fuer Zeichen -- und nach jedem nachsehen. Kein Return.
    const QString ruf = QStringLiteral("DL1ABC");
    int abZeichen = -1;
    for (int i = 0; i < ruf.size(); ++i) {
        QTest::keyClick(callField, ruf.at(i).toLatin1());
        QCoreApplication::processEvents();
        if (abZeichen < 0 && pille->text() == QStringLiteral("DUPE")) {
            abZeichen = i + 1;
        }
    }
    qInfo().noquote() << "DUPE stand ab dem" << abZeichen << ". Zeichen; Pille:" << pille->text()
                      << "| Hinweis:" << hinweis->text();

    QVERIFY2(abZeichen > 0, "Die DUPE-Pille kam beim Tippen gar nicht");
    QCOMPARE(pille->text(), QStringLiteral("DUPE"));

    // Und der Hinweis muss die Nummer UND die Uhrzeit nennen -- das ist
    // der Punkt der ganzen Sache.
    const QString text = hinweis->text();
    QVERIFY2(text.contains(QStringLiteral("DUPE")), qPrintable(text));
    QVERIFY2(text.contains(QStringLiteral("007")), qPrintable(QStringLiteral("Ohne die Nummer: %1").arg(text)));
    QVERIFY2(text.contains(QStringLiteral("14:37")), qPrintable(QStringLiteral("Ohne die Uhrzeit: %1").arg(text)));
    QVERIFY2(text.contains(QStringLiteral("144")), qPrintable(QStringLiteral("Ohne das Band: %1").arg(text)));

    // Das frueher geloggte QSO soll dabei in der Liste markiert stehen,
    // damit man sieht, WELCHES gemeint ist.
    auto* table = window.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(table);
    QVERIFY2(table->selectionModel()->hasSelection(), "Das frueher geloggte QSO ist nicht markiert");

    // Rufzeichen wieder weg: der Hinweis muss verschwinden.
    callField->clear();
    QCoreApplication::processEvents();
    QCOMPARE(pille->text(), QString());
    QVERIFY2(!hinweis->text().contains(QStringLiteral("DUPE")),
             qPrintable(QStringLiteral("Der DUPE-Hinweis bleibt stehen: %1").arg(hinweis->text())));
}

void TestDupeSofort::aFreshCallsignShowsNoDupe()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("frisch.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* pille = window.findChild<QLabel*>(QLatin1String(UnifiedLogWidget::kStatusPillObjectName));
    QVERIFY(pille);
    const QList<QLineEdit*> fields = entryFields(window);
    QVERIFY(fields.size() >= 4);
    fields.at(0)->setFocus();
    QTest::keyClicks(fields.at(0), QStringLiteral("OK2NEU"));
    QCoreApplication::processEvents();
    qInfo().noquote() << "frisches Rufzeichen -- Pille:" << (pille->text().isEmpty() ? QStringLiteral("(leer)")
                                                                                    : pille->text());
    QCOMPARE(pille->text(), QString());
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestDupeSofort tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_dupe_sofort.moc"
