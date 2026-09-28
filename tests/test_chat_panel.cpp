// Martin, 2026-09-28: "erkläre bitte den chat usw. wie der funktioniert,
// was ich sehe, wie dieser in der praxis aussieht usw."
//
// Beim Nachsehen kam heraus, dass ich den Chat am selben Tag unsichtbar
// gemacht hatte: die ON4KST- und Cluster-Zeilen standen bis dahin unter
// einer Trennlinie in der Log-Liste, ich habe sie dort herausgenommen
// (zu Recht -- bei leerem Log sahen sie aus wie kaputte QSOs), und einen
// anderen Ort gab es nicht. Man konnte senden und bekam die Antwort
// nicht zu sehen.
//
// Dieses Panel ist der Ort. Der Prüfstand schickt echte Zeilen durch --
// eine mit Locator, eine ohne, eine vom Cluster, eine von einer schon
// gearbeiteten Station -- und sieht nach, was dasteht.

#include <QtTest>

#include <QAbstractItemModel>
#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>

#include "app/AppController.h"
#include "app/ContestSettings.h"
#include "core/SpotCandidate.h"
#include "data/ContestDatabase.h"
#include "data/QsoRecord.h"
#include "ui/ChatPanelWidget.h"
#include "ui/MainWindow.h"

#include <memory>

using namespace Contestprogramm;

namespace {

SpotCandidate macheZeile(const QString& call, const QString& grid, const QString& text, const QString& quelle,
                          int minutenVorher, qint64 freqHz = 0)
{
    SpotCandidate k;
    k.callsign = call;
    k.grid = grid;
    k.rawLine = text;
    k.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-60 * minutenVorher);
    k.source = quelle;
    k.freqHz = freqHz;
    return k;
}

} // namespace

class TestChatPanel : public QObject
{
    Q_OBJECT

private slots:
    void theChatShowsWhatCameInFromBothSources();
    void aDoubleClickTakesTheStationOver();
    void sendingAMessageLeavesTheField();
    void theChatPanelExistsInTheMainWindow();

private:
    std::unique_ptr<AppController> makeController(QTemporaryDir& dir, const QString& file);
};

std::unique_ptr<AppController> TestChatPanel::makeController(QTemporaryDir& dir, const QString& file)
{
    auto controller = std::make_unique<AppController>();
    if (!controller->openDatabase(dir.filePath(file))) {
        return nullptr;
    }
    ContestSettings settings = controller->settings();
    settings.ownCallsign = QStringLiteral("OE5SOS");
    settings.ownGrid = QStringLiteral("JN67VV");
    settings.activeContestId = QStringLiteral("IARU_R1_VHF_UHF");
    settings.rigctldHost.clear();
    settings.rotor1Enabled = false;
    settings.rotor2Enabled = false;
    settings.esmEnabled = false;
    controller->setSettings(settings);
    return controller;
}

void TestChatPanel::theChatShowsWhatCameInFromBothSources()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("chat.sqlite"));
    QVERIFY(controller);

    // Eine Station, die schon im Log steht -- die muss gedämpft
    // erscheinen.
    QsoRecord r;
    r.callsign = QStringLiteral("DL1ABC");
    r.band = QStringLiteral("144");
    r.mode = QStringLiteral("SSB");
    r.timestampUtc = QDateTime::currentDateTimeUtc().addSecs(-3600).toString(Qt::ISODate);
    r.rstSent = QStringLiteral("59");
    r.rstRcvd = QStringLiteral("59");
    r.serialSent = 1;
    r.serialRcvd = 1;
    r.gridSquare = QStringLiteral("JN58SD");
    r.distanceKm = 165.0;
    r.contestId = QStringLiteral("IARU_R1_VHF_UHF");
    QVERIFY(controller->database().insertQso(r));

    ChatPanelWidget panel;
    panel.setFeedModels(&controller->on4kstFeedModel(), &controller->clusterFeedModel());
    panel.resize(900, 300);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));

    // So kommt es wirklich herein: eine Plauderzeile ohne Locator, eine
    // mit, ein Cluster-Spot mit Frequenz, und die schon gearbeitete.
    controller->on4kstFeedModel().addCandidate(
        macheZeile(QStringLiteral("OK2XYZ"), QString(), QStringLiteral("QRV 144.300? wer hoert mich"),
                    QStringLiteral("on4kst"), 5));
    controller->on4kstFeedModel().addCandidate(
        macheZeile(QStringLiteral("HA5QRP"), QStringLiteral("JN97MM"),
                    QStringLiteral("CQ 144.310 JN97MM"), QStringLiteral("on4kst"), 3));
    controller->clusterFeedModel().addCandidate(
        macheZeile(QStringLiteral("S51DX"), QStringLiteral("JN76AB"), QStringLiteral("144325.0 S51DX JN76AB"),
                    QStringLiteral("cluster"), 2, 144325000));
    controller->on4kstFeedModel().addCandidate(
        macheZeile(QStringLiteral("DL1ABC"), QStringLiteral("JN58SD"), QStringLiteral("tnx qso 73"),
                    QStringLiteral("on4kst"), 1));
    QCoreApplication::processEvents();

    auto* table = panel.findChild<QTableView*>(QLatin1String(ChatPanelWidget::kTableObjectName));
    QVERIFY(table);
    QAbstractItemModel* model = table->model();
    QVERIFY(model);

    QStringList zeilen;
    for (int r2 = 0; r2 < model->rowCount(); ++r2) {
        QStringList felder;
        for (int c = 0; c < ChatPanelWidget::ColumnCount; ++c) {
            felder << model->index(r2, c).data().toString();
        }
        zeilen << felder.join(QStringLiteral(" | "));
    }
    qInfo().noquote() << "So sieht der Chat aus:";
    for (const QString& z : zeilen) {
        qInfo().noquote() << "   " << z;
    }

    // Standardmäßig GEFILTERT -- Martin, 2026-09-28: "alles was mich
    // nicht erreicht bzw. was absolut nicht funktionieren kann möchte
    // ich gefiltert haben um nicht 1000 unnötige chat zu sehen." Es darf
    // also weniger dastehen, als hereinkam. Was nicht sein darf: dass
    // man es nicht merkt.
    auto* status = panel.findChild<QLabel*>(QLatin1String(ChatPanelWidget::kStatusObjectName));
    QVERIFY(status);
    qInfo().noquote() << "Kopfzeile:" << status->text();
    const int gefiltert = model->rowCount();
    QVERIFY2(gefiltert >= 1, "Der Chat ist ganz leer");
    if (gefiltert < 4) {
        QVERIFY2(status->text().contains(QStringLiteral("gefiltert")),
                 qPrintable(QStringLiteral("Es wird gefiltert, aber die Kopfzeile sagt es nicht: %1")
                                .arg(status->text())));
        QVERIFY2(status->text().contains(QStringLiteral("von 4")),
                 qPrintable(QStringLiteral("Die Kopfzeile nennt nicht, wie viele hereinkamen: %1")
                                .arg(status->text())));
    }

    // Und wer nachsehen will, was weggefiltert wurde, kann es -- über
    // den ⚙. Dann steht alles da.
    panel.setShowAll(true);
    QCoreApplication::processEvents();
    qInfo().noquote() << "mit „alle zeigen“:" << status->text();
    QCOMPARE(model->rowCount(), 4);
    zeilen.clear();
    for (int r2 = 0; r2 < model->rowCount(); ++r2) {
        QStringList felder;
        for (int c = 0; c < ChatPanelWidget::ColumnCount; ++c) {
            felder << model->index(r2, c).data().toString();
        }
        zeilen << felder.join(QStringLiteral(" | "));
    }
    qInfo().noquote() << "Alle Zeilen:";
    for (const QString& z : zeilen) {
        qInfo().noquote() << "   " << z;
    }

    // Älteste oben: die Plauderzeile von vor fünf Minuten steht vor der
    // von vor einer.
    const QString ersteZeile = zeilen.first();
    QVERIFY2(ersteZeile.contains(QStringLiteral("OK2XYZ")), qPrintable(ersteZeile));
    QVERIFY2(zeilen.last().contains(QStringLiteral("DL1ABC")), qPrintable(zeilen.last()));

    // Die Quelle steht dabei.
    QVERIFY2(ersteZeile.contains(QStringLiteral("KST")), qPrintable(ersteZeile));
    bool clusterZeileDa = false;
    for (const QString& z : zeilen) {
        if (z.contains(QStringLiteral("CLU")) && z.contains(QStringLiteral("S51DX"))) {
            clusterZeileDa = true;
        }
    }
    QVERIFY2(clusterZeileDa, "Die Cluster-Zeile fehlt oder ist nicht als solche gekennzeichnet");

    // Der Text steht so da, wie er gesendet wurde.
    QVERIFY2(ersteZeile.contains(QStringLiteral("wer hoert mich")), qPrintable(ersteZeile));

    // Mit Locator: Entfernung und Peilung. Ohne: ein Strich, keine
    // erfundene Zahl.
    for (int r2 = 0; r2 < model->rowCount(); ++r2) {
        const QString call = model->index(r2, ChatPanelWidget::ColumnCall).data().toString();
        const QString km = model->index(r2, ChatPanelWidget::ColumnDistance).data().toString();
        if (call == QStringLiteral("HA5QRP")) {
            QVERIFY2(km.contains(QStringLiteral("·")), qPrintable(QStringLiteral("HA5QRP ohne km/°: %1").arg(km)));
        }
        if (call == QStringLiteral("OK2XYZ")) {
            QVERIFY2(!km.contains(QStringLiteral("·")),
                      qPrintable(QStringLiteral("OK2XYZ hat keinen Locator, trotzdem km/°: %1").arg(km)));
        }
    }
}

void TestChatPanel::aDoubleClickTakesTheStationOver()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("chat2.sqlite"));
    QVERIFY(controller);

    ChatPanelWidget panel;
    panel.setFeedModels(&controller->on4kstFeedModel(), &controller->clusterFeedModel());
    // Hier geht es um den Doppelklick, nicht um den Filter -- also alles
    // zeigen, damit die Zeile sicher dasteht.
    panel.setShowAll(true);
    panel.resize(900, 300);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));

    controller->on4kstFeedModel().addCandidate(
        macheZeile(QStringLiteral("HA5QRP"), QStringLiteral("JN97MM"), QStringLiteral("CQ 144.310"),
                    QStringLiteral("on4kst"), 1, 144310000));
    QCoreApplication::processEvents();

    auto* table = panel.findChild<QTableView*>(QLatin1String(ChatPanelWidget::kTableObjectName));
    QVERIFY(table);
    QCOMPARE(table->model()->rowCount(), 1);

    QSignalSpy spy(&panel, &ChatPanelWidget::candidateActivated);
    emit table->doubleClicked(table->model()->index(0, ChatPanelWidget::ColumnCall));
    QCoreApplication::processEvents();

    QCOMPARE(spy.count(), 1);
    qInfo().noquote() << "Doppelklick gibt weiter:" << spy.first().at(0).toString() << spy.first().at(1).toString()
                      << spy.first().at(2).toLongLong();
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("HA5QRP"));
    QCOMPARE(spy.first().at(1).toString(), QStringLiteral("JN97MM"));
    QCOMPARE(spy.first().at(2).toLongLong(), 144310000LL);
}

void TestChatPanel::sendingAMessageLeavesTheField()
{
    ChatPanelWidget panel;
    panel.resize(900, 300);
    panel.show();
    QVERIFY(QTest::qWaitForWindowExposed(&panel));

    auto* input = panel.findChild<QLineEdit*>(QLatin1String(ChatPanelWidget::kInputObjectName));
    QVERIFY(input);
    QSignalSpy spy(&panel, &ChatPanelWidget::messageSubmitted);

    input->setFocus();
    QTest::keyClicks(input, QStringLiteral("QRV 144.300 JN67VV"));
    QTest::keyClick(input, Qt::Key_Return);
    QCoreApplication::processEvents();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("QRV 144.300 JN67VV"));
    // Und das Feld ist wieder leer -- sonst schickt der nächste Return
    // dasselbe noch einmal.
    QVERIFY2(input->text().isEmpty(), "Das Eingabefeld ist nach dem Senden nicht leer");

    // Leer abschicken tut nichts.
    QTest::keyClick(input, Qt::Key_Return);
    QCoreApplication::processEvents();
    QCOMPARE(spy.count(), 1);
}

// Und das Panel muss im Fenster wirklich vorkommen -- als eigenes,
// verschiebbares Panel wie Bandmap und Skeds.
void TestChatPanel::theChatPanelExistsInTheMainWindow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("chat3.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* panel = window.findChild<ChatPanelWidget*>();
    QVERIFY2(panel, "Es gibt kein Chat-Panel im Fenster");
    auto* container = window.findChild<QWidget*>(QStringLiteral("chat"));
    QVERIFY2(container, "Das Chat-Panel ist nicht als Panel registriert");

    // Eine Zeile hereinschicken -- sie muss im Fenster ankommen.
    controller->on4kstFeedModel().addCandidate(
        macheZeile(QStringLiteral("OK2XYZ"), QString(), QStringLiteral("QRV?"), QStringLiteral("on4kst"), 1));
    QCoreApplication::processEvents();

    auto* table = panel->findChild<QTableView*>(QLatin1String(ChatPanelWidget::kTableObjectName));
    QVERIFY(table);
    qInfo() << "Zeilen im Chat-Panel des Fensters:" << table->model()->rowCount();
    QVERIFY(table->model()->rowCount() >= 1);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestChatPanel tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_chat_panel.moc"
