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
#include <QMenu>
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
#include "ui/PanelContainerWidget.h"
#include "ui/PanelHeaderBar.h"
#include "ui/PanelLayoutManager.h"

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
    void theOptionsMenuOffersEverythingThatCanBeChanged();
    void everyPanelWithOptionsAlsoShowsTheGear();

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

    // Die Lage aller Panels -- und ob das Chat-Panel unter einem
    // anderen liegt. Ein Panel, das man erst hervorziehen muss, ist
    // keine gute Vorgabe (im laufenden Betrieb lag der Chat unter Rate
    // und Rotoren).
    QMap<QString, QRect> lagen;
    for (const QString& id : {QStringLiteral("unifiedlog"), QStringLiteral("rotorrow"), QStringLiteral("map"),
                               QStringLiteral("suggestion"), QStringLiteral("ratemeter"),
                               QStringLiteral("checkpartial"), QStringLiteral("bandmap"),
                               QStringLiteral("skeds"), QStringLiteral("chat")}) {
        if (auto* w = window.findChild<QWidget*>(id)) {
            if (!w->isHidden()) {
                lagen.insert(id, w->geometry());
            }
        }
    }
    for (auto it = lagen.constBegin(); it != lagen.constEnd(); ++it) {
        qInfo().noquote() << QStringLiteral("  %1: %2,%3 %4x%5")
                                  .arg(it.key(), -14)
                                  .arg(it.value().x())
                                  .arg(it.value().y())
                                  .arg(it.value().width())
                                  .arg(it.value().height());
    }
    // Im kompakten Entwurf hat der Chat bewusst keine Vorgabe -- die
    // Fläche ist dort voll (siehe die Registrierung in MainWindow).
    // Dann steht er nicht da, und es gibt nichts zu überlappen.
    if (!lagen.contains(QStringLiteral("chat"))) {
        qInfo().noquote() << "Der Chat hat in diesem Entwurf keine Vorgabe -- er wird über "
                              "Fenster > Panels eingeschaltet.";
        return;
    }
    const QRect chatLage = lagen.value(QStringLiteral("chat"));
    QStringList ueberlappt;
    for (auto it = lagen.constBegin(); it != lagen.constEnd(); ++it) {
        if (it.key() == QStringLiteral("chat")) {
            continue;
        }
        if (it.value().intersects(chatLage)) {
            ueberlappt << it.key();
        }
    }
    qInfo().noquote() << "Chat überlappt mit:"
                      << (ueberlappt.isEmpty() ? QStringLiteral("(nichts)")
                                                : ueberlappt.join(QStringLiteral(", ")));
    QVERIFY2(ueberlappt.isEmpty(),
             qPrintable(QStringLiteral("Das Chat-Panel liegt in der Vorgabe unter: %1")
                            .arg(ueberlappt.join(QStringLiteral(", ")))));
}

// Martin, 2026-09-28: "der chat sollte auch optionen haben. wie zb
// wechsel des bandes usw.! alle möglichkeiten sollten dort änderbar
// sein." Also nachsehen, dass im ⚙ des Chat-Kopfes wirklich alles steht,
// was ON4KST hergibt -- Raum, Anwesenheit, CQ -- und was das Panel
// selbst kann.
void TestChatPanel::theOptionsMenuOffersEverythingThatCanBeChanged()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("chatopt.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    // Das Menü ist ein Aufklappmenü: es beim Aufgehen abfangen, seine
    // Einträge lesen und wieder zumachen.
    QStringList eintraege;
    QStringList kennungen;
    QTimer::singleShot(0, [&eintraege, &kennungen]() {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu) {
            return;
        }
        const auto sammle = [&](QMenu* m, auto&& selbst) -> void {
            for (QAction* a : m->actions()) {
                if (a->isSeparator()) {
                    continue;
                }
                if (a->menu()) {
                    selbst(a->menu(), selbst);
                    continue;
                }
                eintraege << a->text();
                if (!a->objectName().isEmpty()) {
                    kennungen << a->objectName();
                }
            }
        };
        sammle(menu, sammle);
        menu->close();
    });
    QMetaObject::invokeMethod(&window, "showChatOptionsPopup");
    QCoreApplication::processEvents();

    qInfo().noquote() << "Im ⚙ des Chats steht:";
    for (const QString& e : eintraege) {
        qInfo().noquote() << "   " << e;
    }

    // Die Räume, die ON4KST kennt (siehe On4kstClient's
    // Klassenkommentar: 50 / 144 / GHZ / EME / HF).
    for (const QString& raum : {QStringLiteral("chatRoom_50"), QStringLiteral("chatRoom_144"),
                                 QStringLiteral("chatRoom_GHZ"), QStringLiteral("chatRoom_EME"),
                                 QStringLiteral("chatRoom_HF")}) {
        QVERIFY2(kennungen.contains(raum), qPrintable(QStringLiteral("Der Raum %1 fehlt").arg(raum)));
    }
    QVERIFY2(kennungen.contains(QStringLiteral("chatRoomFollowBand")),
             "Es fehlt der Weg zurück zu „dem Band folgen“");
    // Anwesenheit, CQ, Filter, Reichweite.
    for (const QString& kennung : {QStringLiteral("chatAwayAction"), QStringLiteral("chatBackAction"),
                                    QStringLiteral("chatCqAction"), QStringLiteral("chatShowAllAction"),
                                    QStringLiteral("chatRadiusAction")}) {
        QVERIFY2(kennungen.contains(kennung), qPrintable(QStringLiteral("Es fehlt: %1").arg(kennung)));
    }
}

// Martin, 2026-09-28: "im chat gibt es keine optionen - diese sollten
// dafür dienen, dass ich zb die gruppe wechseln kann". Das Menü gab es
// längst (siehe den Prüfstand darüber), nur keinen Knopf, der es
// öffnet: PanelHeaderBar zeigt den ⚙ erst nach
// setOptionsAffordanceEnabled(true), und beim Chat fehlte genau diese
// Zeile. Ein Menü, das man prüfen, aber nicht anklicken kann, ist
// keines -- deshalb prüft dieser Prüfstand den Weg dorthin, für jedes
// Panel, das Optionen hat.
void TestChatPanel::everyPanelWithOptionsAlsoShowsTheGear()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto controller = makeController(dir, QStringLiteral("zahnrad.sqlite"));
    QVERIFY(controller);

    MainWindow window(*controller);
    window.resize(1440, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto* manager = window.findChild<PanelLayoutManager*>();
    QVERIFY(manager);
    for (const QString& id : {QStringLiteral("chat"), QStringLiteral("unifiedlog"), QStringLiteral("map")}) {
        PanelContainerWidget* panel = manager->panel(id);
        QVERIFY2(panel, qPrintable(QStringLiteral("Panel %1 fehlt").arg(id)));
        PanelHeaderBar* kopf = panel->headerBar();
        QVERIFY2(kopf, qPrintable(QStringLiteral("Panel %1 hat keinen Kopf").arg(id)));
        auto* zahnrad = kopf->findChild<QPushButton*>(QStringLiteral("panelHeaderOptionsButton"));
        QVERIFY2(zahnrad, qPrintable(QStringLiteral("Panel %1: kein ⚙-Knopf").arg(id)));
        qInfo().noquote() << id << "-- ⚙ sichtbar:" << (zahnrad->isVisibleTo(kopf) ? "ja" : "nein");
        QVERIFY2(zahnrad->isVisibleTo(kopf),
                 qPrintable(QStringLiteral("Panel %1 hat Optionen, zeigt aber kein ⚙").arg(id)));
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestChatPanel tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_chat_panel.moc"
