#include <QtTest>

#include <QApplication>
#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>

#include "app/ContestSettings.h"
#include "core/GeoFilter.h"
#include "core/SpotCandidate.h"
#include "data/ContestDatabase.h"
#include "data/ContestDefinition.h"
#include "data/DupeChecker.h"
#include "data/QsoRecord.h"
#include "models/ChatFeedModel.h"
#include "models/LogTableModel.h"
#include "ui/UnifiedLogWidget.h"

using namespace Contestprogramm;

// New coverage for the merged Log panel (ui/UnifiedLogWidget.h), which
// replaces EntryBarWidget + LogTableView + both ChatFeedViews per
// Martin's "alles im gleichen Raster" layout change, plus his four
// numbered follow-ups from this same task (see this task's own report
// for the verbatim quotes): (1) Mode gets Band's exact treatment --
// covered in MainWindow-level tests, not here, since this widget itself
// carries no Mode UI at all any more; (2) DXLog.net's real field-
// navigation model (Enter logs from any field, Space skips RST, Tab
// visits it) and the RST auto-default; (3) a logged history row's Call/
// Exch Emp. cells are editable in place, and the Status cell toggles an
// invalid mark. Also still covers the exchange row not being hardcoded
// to one sub-field, click-to-fill from a spot/chat candidate row, and
// the KST/CLU source pill landing on the right rows. Dupe-status
// rendering (DUPE/NEU on the entry row) is covered alongside the pill
// test, since both read the same PillTextRole/PillBgRole/... roles (see
// UnifiedLogWidget.h).
class TestUnifiedLogWidget : public QObject
{
    Q_OBJECT

private slots:
    void enterLogsFromAnyEntryRowField();
    void enterLogsImmediatelyWithNoExchangeFields();
    void spaceAndTabBothSkipRstField();
    void rstAutoDefaultsPerModeAndStaysOverridable();
    void exchangeFieldsSupportMoreThanOneSubField();
    void clickToFillFromSpotRowEmitsCandidateActivated();
    void dupeStatusRendersAsPill();
    void sourcePillDistinguishesKstFromCluster();
    void historyRowCallAndSerialGridRcvdAreEditableInPlace();
    void historyRowStatusClickTogglesInvalid();
    void invalidQsoRendersAsUngueltigPillAndDimmed();
    void statusLineShowsDashBeforeFirstQsoThenLastCallsignAndTime();
    void statusLineTracksLogModelWithoutExplicitPush();
    void statusLineShowsOperatingMode();
    void viewModeTogglesSerialColumnVisibilityOnly();
    void serialColumnShowsRealSerialSentOrDash();
    void logViewModeSettingRoundTripsThroughDatabase();
    void dxLogFullColumnsShowsSplitColumnsInDxLogOrder();
    void entryRowPositionBottomPlacesEntryRowAfterFeedTable();
    void logEntryRowPositionSettingRoundTripsThroughDatabase();
    void dupeHistoryRowRendersAsDupePill();
    void narrowPanelFitsTheColumnsAndTheEntryRowFollows();
    void aNewlyLoggedQsoScrollsIntoView();
    void serialsReadAsThreeDigitsEverywhere();
    void dupeDetailReplacesTheLastQsoLine();
    void distanceColumnsFollowTheExchangeFields();
};

namespace {

QVector<ContestDefinition::ExchangeField> threeFieldExchange()
{
    ContestDefinition::ExchangeField serial;
    serial.key = QStringLiteral("serial");
    serial.label = QStringLiteral("Serial");
    serial.type = QStringLiteral("int");
    serial.autoIncrement = true;

    ContestDefinition::ExchangeField grid;
    grid.key = QStringLiteral("grid");
    grid.label = QStringLiteral("Grid");
    grid.type = QStringLiteral("grid6");

    ContestDefinition::ExchangeField name;
    name.key = QStringLiteral("name");
    name.label = QStringLiteral("Name");
    name.type = QStringLiteral("text");

    return {serial, grid, name};
}

// The shipped contest_definitions' shape as of this task: RST, then
// Serial, then Grid (resources/contest_definitions/oe_vhf_uhf.json) --
// matching Martin's own stated order ("59 steht automatisch... dann
// kommt die Nummer und/oder der Locator").
QVector<ContestDefinition::ExchangeField> rstSerialGridFields()
{
    ContestDefinition::ExchangeField rst;
    rst.key = QStringLiteral("rst");
    rst.label = QStringLiteral("RST");
    rst.type = QStringLiteral("rst");

    ContestDefinition::ExchangeField serial;
    serial.key = QStringLiteral("serial");
    serial.label = QStringLiteral("Serial");
    serial.type = QStringLiteral("int");
    serial.autoIncrement = true;

    ContestDefinition::ExchangeField grid;
    grid.key = QStringLiteral("grid");
    grid.label = QStringLiteral("Grid");
    grid.type = QStringLiteral("grid6");

    return {rst, serial, grid};
}

QLineEdit* findEditByPlaceholder(const QWidget& widget, const QString& placeholder)
{
    for (QLineEdit* edit : widget.findChildren<QLineEdit*>()) {
        if (edit->placeholderText() == placeholder) {
            return edit;
        }
    }
    return nullptr;
}

SpotCandidate makeCandidate(const QString& callsign, const QString& grid, const QString& source)
{
    SpotCandidate candidate;
    candidate.callsign = callsign;
    candidate.grid = grid;
    candidate.rawLine = callsign + QStringLiteral(" ") + grid;
    candidate.timestampUtc = QDateTime::currentDateTimeUtc();
    candidate.source = source;
    candidate.freqHz = 144300000;
    return candidate;
}

QsoRecord makeLoggedRecord(int id, const QString& callsign)
{
    QsoRecord record;
    record.id = id;
    record.callsign = callsign;
    record.band = QStringLiteral("144");
    record.mode = QStringLiteral("SSB");
    record.timestampUtc = QStringLiteral("2026-06-13T12:00:00Z");
    record.exchangeRcvd = QStringLiteral("599 001 JN77QT");
    record.contestId = QStringLiteral("OE_VHF_UHF");
    return record;
}

} // namespace

// DXLog.net's real model (dxlog.net/docs/index.php/Main_Window,
// verified for this task -- see the report): "[Enter] is used to log a
// contact" from ANY entry-row field, not only the last one in a fixed
// sequence -- replaces the old progressive-Enter scheme (advance to the
// next sub-field, log only from the last one). Uses a three-field
// exchange (not the shipped rst+serial+grid) so a MIDDLE field, not
// just the first/last, is actually exercised.
void TestUnifiedLogWidget::enterLogsFromAnyEntryRowField()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(threeFieldExchange());

    QSignalSpy logSpy(&widget, &UnifiedLogWidget::logRequested);

    QLineEdit* callsignEdit = findEditByPlaceholder(widget, QStringLiteral("Callsign"));
    QLineEdit* serialEdit = findEditByPlaceholder(widget, QStringLiteral("Serial"));
    QLineEdit* gridEdit = findEditByPlaceholder(widget, QStringLiteral("Grid"));
    QLineEdit* nameEdit = findEditByPlaceholder(widget, QStringLiteral("Name"));
    QVERIFY(callsignEdit);
    QVERIFY(serialEdit);
    QVERIFY(gridEdit);
    QVERIFY(nameEdit);

    callsignEdit->setText(QStringLiteral("DL3ABC"));
    serialEdit->setText(QStringLiteral("5"));
    gridEdit->setText(QStringLiteral("JN77QT"));
    nameEdit->setText(QStringLiteral("Hans"));

    // Enter on a MIDDLE field (Grid, neither first nor last) logs
    // immediately -- the whole point of the DXLog.net model this
    // replaces the old progressive scheme with.
    QTest::keyClick(gridEdit, Qt::Key_Return);
    QCOMPARE(logSpy.count(), 1);

    QCOMPARE(widget.callsign(), QStringLiteral("DL3ABC"));
    const QMap<QString, QString> received = widget.exchangeReceived();
    QCOMPARE(received.value(QStringLiteral("serial")), QStringLiteral("5"));
    QCOMPARE(received.value(QStringLiteral("grid")), QStringLiteral("JN77QT"));
    QCOMPARE(received.value(QStringLiteral("name")), QStringLiteral("Hans"));

    // Enter on Callsign itself also logs directly -- Call is still
    // always the starting field, but no longer a special "advance into
    // the row" case.
    QTest::keyClick(callsignEdit, Qt::Key_Return);
    QCOMPARE(logSpy.count(), 2);
}

// A contest with zero exchange fields (edge case, but a real one --
// ContestDefinition::exchangeFields() can legitimately be empty) logs
// straight from Callsign, same as EntryBarWidget always did.
void TestUnifiedLogWidget::enterLogsImmediatelyWithNoExchangeFields()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields({});

    QSignalSpy logSpy(&widget, &UnifiedLogWidget::logRequested);
    QLineEdit* callsignEdit = findEditByPlaceholder(widget, QStringLiteral("Callsign"));
    QVERIFY(callsignEdit);

    callsignEdit->setText(QStringLiteral("DL3ABC"));
    QTest::keyClick(callsignEdit, Qt::Key_Return);
    QCOMPARE(logSpy.count(), 1);
}

// DXLog.net's documented model was originally "[Space] to step through
// relevant entry fields. [Tab] will step through all entry fields" --
// overridden by the operator directly, 2026-09-11 ("der cursor spring
// zu 59, was unwichtig ist, dies sollte übersprungen werden"): Tab now
// skips RST too, same as Space -- RST is still directly clickable with
// the mouse for the rare manual override, just never a keyboard-
// navigation stop any more.
void TestUnifiedLogWidget::spaceAndTabBothSkipRstField()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(rstSerialGridFields());
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    QLineEdit* callsignEdit = findEditByPlaceholder(widget, QStringLiteral("Callsign"));
    QLineEdit* rstEdit = findEditByPlaceholder(widget, QStringLiteral("RST"));
    QLineEdit* serialEdit = findEditByPlaceholder(widget, QStringLiteral("Serial"));
    QLineEdit* gridEdit = findEditByPlaceholder(widget, QStringLiteral("Grid"));
    QVERIFY(callsignEdit);
    QVERIFY(rstEdit);
    QVERIFY(serialEdit);
    QVERIFY(gridEdit);

    // QWidget::focusWidget() (the "logical" focus child Qt tracks per
    // top-level, set by setFocus() regardless of whether the window
    // manager has actually activated this window) rather than
    // hasFocus()/QApplication::focusWidget() -- the latter two depend
    // on real OS-level window activation, which a test process is not
    // guaranteed to have (and reliably does not, running under ctest),
    // so they would make this test flaky/false-failing for reasons
    // unrelated to the eventFilter()/nextRelevantField() logic actually
    // under test here.
    callsignEdit->setFocus();
    QCOMPARE(widget.focusWidget(), static_cast<QWidget*>(callsignEdit));

    // Space from Call skips straight over RST to Serial.
    QTest::keyClick(callsignEdit, Qt::Key_Space);
    QCOMPARE(widget.focusWidget(), static_cast<QWidget*>(serialEdit));

    QTest::keyClick(serialEdit, Qt::Key_Space);
    QCOMPARE(widget.focusWidget(), static_cast<QWidget*>(gridEdit));

    // Tab now skips RST too (see this test's own doc comment) --
    // straight from Call to Serial, same as Space.
    callsignEdit->setFocus();
    QCOMPARE(widget.focusWidget(), static_cast<QWidget*>(callsignEdit));
    QTest::keyClick(callsignEdit, Qt::Key_Tab);
    QCOMPARE(widget.focusWidget(), static_cast<QWidget*>(serialEdit));

    // RST is still directly reachable by mouse click/setFocus() --
    // navigation skips it, the field itself is not removed or disabled.
    rstEdit->setFocus();
    QCOMPARE(widget.focusWidget(), static_cast<QWidget*>(rstEdit));
}

// DXLog.net's own documented RST rule (dxlog.net/docs, verified for
// this task): "RST sent and RST rcvd items are set to 59 (if the mode
// is SSB or FM) or 599 (if the mode is CW, RTTY, or PSK)" -- and an
// operator's own hand-typed RST must survive a later mode change (the
// same "auto-filled, still overridable" convention grid/serial autofill
// already use).
void TestUnifiedLogWidget::rstAutoDefaultsPerModeAndStaysOverridable()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(rstSerialGridFields());

    // No mode known yet -- falls back to 59 (SSB/FM's value), matching
    // this widget's own "assume SSB" default reasoning.
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("rst")), QStringLiteral("59"));

    widget.setCurrentMode(QStringLiteral("CW"));
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("rst")), QStringLiteral("599"));

    widget.setCurrentMode(QStringLiteral("FM"));
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("rst")), QStringLiteral("59"));

    widget.setCurrentMode(QStringLiteral("RTTY"));
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("rst")), QStringLiteral("599"));

    // The operator hand-types over the auto-filled value.
    QLineEdit* rstEdit = findEditByPlaceholder(widget, QStringLiteral("RST"));
    QVERIFY(rstEdit);
    rstEdit->clear();
    QTest::keyClicks(rstEdit, QStringLiteral("5NN"));
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("rst")), QStringLiteral("5NN"));

    // A later mode change must not clobber the operator's own value.
    widget.setCurrentMode(QStringLiteral("SSB"));
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("rst")), QStringLiteral("5NN"));

    // A fresh entry (next contact) re-defaults RST per the current
    // mode, same as DXLog.net does at the start of every new entry.
    widget.resetForNextEntry();
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("rst")), QStringLiteral("59"));
}

// The "Exch Emp." cell is not hardcoded to a single text box -- a
// contest can declare more than one received-exchange field (see this
// task's report on how a multi-field exchange fits one compact cell),
// and each sub-field's value must round-trip independently through
// setExchangeFieldValue()/exchangeReceived().
void TestUnifiedLogWidget::exchangeFieldsSupportMoreThanOneSubField()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(threeFieldExchange());
    QCOMPARE(widget.exchangeReceived().size(), 3);

    widget.setExchangeFieldValue(QStringLiteral("serial"), QStringLiteral("012"));
    widget.setExchangeFieldValue(QStringLiteral("grid"), QStringLiteral("jn88tc")); // lower-case on purpose
    widget.setExchangeFieldValue(QStringLiteral("name"), QStringLiteral("Eva"));

    const QMap<QString, QString> received = widget.exchangeReceived();
    QCOMPARE(received.value(QStringLiteral("serial")), QStringLiteral("012"));
    // grid6-typed fields are upper-cased regardless of position in the row.
    QCOMPARE(received.value(QStringLiteral("grid")), QStringLiteral("JN88TC"));
    QCOMPARE(received.value(QStringLiteral("name")), QStringLiteral("Eva"));
}

// Clicking a not-yet-worked spot/chat candidate row in the feed table
// must fill the entry row exactly like today's click-to-fill did (see
// MainWindow::handleCandidateActivated) -- driven directly through the
// same private slot a real QTableView::clicked would invoke, matching
// this project's established "drive the tested state-changing entry
// point directly, not the UI chrome that triggers it" approach (see
// tests/test_panellayoutmanager.cpp's own class comment for the same
// reasoning applied to PanelContainerWidget's drag/resize signals).
void TestUnifiedLogWidget::clickToFillFromSpotRowEmitsCandidateActivated()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("unifiedlog_click.sqlite")), QStringLiteral("unifiedlog_click")));
    DupeChecker dupeChecker(db);
    GeoFilter geoFilter; // default radius/no own grid -- classify() always inRange for this test

    ChatFeedModel onKst(geoFilter, dupeChecker);
    ChatFeedModel cluster(geoFilter, dupeChecker);
    onKst.addCandidate(makeCandidate(QStringLiteral("DL3ABC"), QStringLiteral("JN58XX"), QStringLiteral("on4kst")));

    UnifiedLogWidget widget;
    widget.setChatModels(&onKst, &cluster);

    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    QVERIFY(feedTable->model());
    // Row 0 is the "Spots & Chat" divider (both chat models are set, so
    // it always appears); row 1 is the one on4kst candidate.
    QCOMPARE(feedTable->model()->rowCount(), 2);
    const QModelIndex candidateIndex = feedTable->model()->index(1, UnifiedLogWidget::ColumnCall);

    QSignalSpy activatedSpy(&widget, &UnifiedLogWidget::candidateActivated);
    QMetaObject::invokeMethod(&widget, "handleFeedRowClicked", Q_ARG(QModelIndex, candidateIndex));

    QCOMPARE(activatedSpy.count(), 1);
    const QList<QVariant> args = activatedSpy.constFirst();
    QCOMPARE(args.at(0).toString(), QStringLiteral("DL3ABC"));
    QCOMPARE(args.at(1).toString(), QStringLiteral("JN58XX"));
    QCOMPARE(args.at(2).toLongLong(), qint64(144300000));
}

// The entry row's own dupe indicator (mockup E: a real QLabel, objectName
// UnifiedLogWidget::kStatusPillObjectName -- see UnifiedLogWidget.h's
// class comment for why this is no longer a QTableView cell/PillDelegate
// pill the way the feed table's own DUPE/UNGÜLTIG/KST/CLU pills still
// are). Restyled 2026-09-11 (operator, pointing at DXLog.net's real
// "Contest recorder" screenshot, row 10: "die eingabezeile sollte keine
// seperate zeile sein ... gleiche grafik uns stil wie die fertigen
// qso"): blank while the row is still just "new," matching
// m_feedTable's own Status column on an ordinary valid logged row (see
// UnifiedFeedModel::historyData()'s own comment: "Status: blank for a
// valid, already-logged row") and DXLog's own row 10 (blank "Stn"
// cell) -- only a real dupe gets a small red badge.
void TestUnifiedLogWidget::dupeStatusRendersAsPill()
{
    UnifiedLogWidget widget;
    auto* pill = widget.findChild<QLabel*>(QLatin1String(UnifiedLogWidget::kStatusPillObjectName));
    QVERIFY(pill);

    // Default state: not a dupe -- blank, no badge at all.
    QVERIFY(pill->text().isEmpty());

    widget.setDupeIndicator(true);
    QCOMPARE(pill->text(), QStringLiteral("DUPE"));
    // A real warning badge now (border + background), not just plain
    // transparent text.
    QVERIFY(pill->styleSheet().contains(QStringLiteral("border")));

    widget.setDupeIndicator(false);
    QVERIFY(pill->text().isEmpty());
}

// KST (on4kst) vs. CLU (cluster) source pill -- replacing the old
// separate "ON4KST"/"Cluster" panel headers -- must land on the row
// that actually came from that source, not just "some" pill on every
// candidate row.
void TestUnifiedLogWidget::sourcePillDistinguishesKstFromCluster()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("unifiedlog_pill.sqlite")), QStringLiteral("unifiedlog_pill")));
    DupeChecker dupeChecker(db);
    GeoFilter geoFilter;

    ChatFeedModel onKst(geoFilter, dupeChecker);
    ChatFeedModel cluster(geoFilter, dupeChecker);
    onKst.addCandidate(makeCandidate(QStringLiteral("DL3ABC"), QStringLiteral("JN58XX"), QStringLiteral("on4kst")));
    cluster.addCandidate(makeCandidate(QStringLiteral("HB9XYZ"), QStringLiteral("JN47AA"), QStringLiteral("cluster")));

    UnifiedLogWidget widget;
    widget.setChatModels(&onKst, &cluster);

    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    QAbstractItemModel* model = feedTable->model();
    QVERIFY(model);
    // Row 0: divider. Row 1: the on4kst candidate. Row 2: the cluster
    // candidate (see UnifiedFeedModel::rebuild()'s append order).
    QCOMPARE(model->rowCount(), 3);

    const QModelIndex kstStatus = model->index(1, UnifiedLogWidget::ColumnStatus);
    const QModelIndex cluStatus = model->index(2, UnifiedLogWidget::ColumnStatus);
    QCOMPARE(model->data(kstStatus, UnifiedLogWidget::PillTextRole).toString(), QStringLiteral("KST"));
    QCOMPARE(model->data(cluStatus, UnifiedLogWidget::PillTextRole).toString(), QStringLiteral("CLU"));
    // Different colour families too (amber vs. blue), not just different text.
    QVERIFY(model->data(kstStatus, UnifiedLogWidget::PillFgRole).toString()
            != model->data(cluStatus, UnifiedLogWidget::PillFgRole).toString());
}

// DXLog.net's real scope for editing an already-logged QSO
// (dxlog.net/docs/index.php/Menu_Edit, verified for this task): the
// exchange fields are correctable in place in the log grid; this
// widget's own equivalent is the History row's Call and Exch Emp.
// cells (see UnifiedFeedModel::flags()/setData()) -- every other
// column (Zeit here) stays non-editable, matching the deliberately
// narrower scope described in this task's report.
void TestUnifiedLogWidget::historyRowCallAndSerialGridRcvdAreEditableInPlace()
{
    UnifiedLogWidget widget;
    LogTableModel logModel;
    QsoRecord record = makeLoggedRecord(42, QStringLiteral("OE1ABC"));
    record.rstRcvd = QStringLiteral("599");
    record.serialRcvd = 1;
    record.gridSquare = QStringLiteral("JN77QT");
    logModel.setRecords({record});
    widget.setLogModel(&logModel);

    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    QAbstractItemModel* model = feedTable->model();
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 1); // one history row, no chat models set -> no divider/candidates

    const QModelIndex callIndex = model->index(0, UnifiedLogWidget::ColumnCall);
    QVERIFY(model->flags(callIndex) & Qt::ItemIsEditable);

    QSignalSpy callSpy(&widget, &UnifiedLogWidget::historyCallsignEditRequested);
    QVERIFY(model->setData(callIndex, QStringLiteral("oe1xyz")));
    QCOMPARE(callSpy.count(), 1);
    QCOMPARE(callSpy.constFirst().at(0).toInt(), 42);
    QCOMPARE(callSpy.constFirst().at(1).toString(), QStringLiteral("OE1XYZ")); // upper-cased, like the entry row's own callsign()

    // RST is deliberately NOT editable -- operator, 2026-09-11: "59 ist
    // immer fix, kann man nicht ändern, man soll auch nicht hier
    // reinklicken können" (RST is a fixed contest-report convention, not
    // genuinely-exchanged information). The old combined
    // ColumnExchangeRcvd cell is gone from editability too -- it used to
    // be the only way to fix Serial/Grid in Compact view, and doing so
    // let an edit reach RST right along with it (same-day follow-up:
    // "ich habe bei 59 reingeklickt und der locator war in der gleichen
    // raster").
    const QModelIndex rstIndex = model->index(0, UnifiedLogWidget::ColumnRstRcvd);
    QVERIFY(!(model->flags(rstIndex) & Qt::ItemIsEditable));
    const QModelIndex oldExchIndex = model->index(0, UnifiedLogWidget::ColumnExchangeRcvd);
    QVERIFY(!(model->flags(oldExchIndex) & Qt::ItemIsEditable));

    // ColumnSerialGridRcvd is the one editable received-exchange cell,
    // in either view mode -- its EditRole text is Serial+Grid only, RST
    // excluded (serialGridRcvdText()'s own comment).
    const QModelIndex serialGridIndex = model->index(0, UnifiedLogWidget::ColumnSerialGridRcvd);
    QVERIFY(model->flags(serialGridIndex) & Qt::ItemIsEditable);
    QCOMPARE(model->data(serialGridIndex, Qt::EditRole).toString(), QStringLiteral("001 JN77QT"));

    QSignalSpy exchSpy(&widget, &UnifiedLogWidget::historyExchangeRcvdEditRequested);
    QVERIFY(model->setData(serialGridIndex, QStringLiteral(" 002 JN88TC ")));
    QCOMPARE(exchSpy.count(), 1);
    QCOMPARE(exchSpy.constFirst().at(0).toInt(), 42);
    // The record's own existing RST ("599") is re-attached unchanged as
    // the first token -- MainWindow::handleHistoryExchangeRcvdEditRequested()
    // re-derives RST/Serial/Grid from this text POSITIONALLY, by field
    // order; without this, the typed Serial digits would land in RST's
    // slot instead (setData()'s own comment).
    QCOMPARE(exchSpy.constFirst().at(1).toString(), QStringLiteral("599 002 JN88TC"));

    // Zeit joined the editable set with the time-correction pass: the
    // cell opens on its shown HH:mm and hands the typed text up
    // verbatim -- MainWindow::handleHistoryTimeEditRequested() parses
    // it (HH:MM keeps the date, YYYY-MM-DD HH:MM sets both).
    const QModelIndex timeIndex = model->index(0, UnifiedLogWidget::ColumnTime);
    QVERIFY(model->flags(timeIndex) & Qt::ItemIsEditable);
    QCOMPARE(model->data(timeIndex, Qt::EditRole).toString(), model->data(timeIndex, Qt::DisplayRole).toString());
    QSignalSpy timeSpy(&widget, &UnifiedLogWidget::historyTimeEditRequested);
    QVERIFY(model->setData(timeIndex, QStringLiteral(" 11:58 ")));
    QCOMPARE(timeSpy.count(), 1);
    QCOMPARE(timeSpy.constFirst().at(0).toInt(), 42);
    QCOMPARE(timeSpy.constFirst().at(1).toString(), QStringLiteral("11:58"));
    QVERIFY(!model->setData(timeIndex, QString()));
    QCOMPARE(timeSpy.count(), 1);

    // Frequency/operator and the rest stay read-only.
    QVERIFY(!(model->flags(model->index(0, UnifiedLogWidget::ColumnBand)) & Qt::ItemIsEditable));

    // An empty callsign is rejected, not silently logged as blank.
    QSignalSpy rejectedCallSpy(&widget, &UnifiedLogWidget::historyCallsignEditRequested);
    QVERIFY(!model->setData(callIndex, QString()));
    QCOMPARE(rejectedCallSpy.count(), 0);
}

// DXLog.net deliberately has no delete function for a logged QSO
// (dxlog.net/docs/index.php/Menu_Edit, verified for this task: "in the
// spirit of honest contest logging... you don't") -- clicking the
// Status cell of a History row is this widget's equivalent of
// DXLog.net's Ctrl+X mark-invalid.
void TestUnifiedLogWidget::historyRowStatusClickTogglesInvalid()
{
    UnifiedLogWidget widget;
    LogTableModel logModel;
    logModel.setRecords({makeLoggedRecord(7, QStringLiteral("OE2XYZ"))});
    widget.setLogModel(&logModel);

    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    QVERIFY(feedTable->model());
    const QModelIndex statusIndex = feedTable->model()->index(0, UnifiedLogWidget::ColumnStatus);

    QSignalSpy toggleSpy(&widget, &UnifiedLogWidget::historyInvalidToggleRequested);
    QMetaObject::invokeMethod(&widget, "handleFeedRowClicked", Q_ARG(QModelIndex, statusIndex));
    QCOMPARE(toggleSpy.count(), 1);
    QCOMPARE(toggleSpy.constFirst().at(0).toInt(), 7);

    // Clicking Status on a Candidate row (not History) must not emit
    // this -- only a logged QSO can be marked invalid. Reuses the same
    // click-to-fill fixture shape as clickToFillFromSpotRowEmitsCandidateActivated().
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("unifiedlog_status_click.sqlite")), QStringLiteral("unifiedlog_status_click")));
    DupeChecker dupeChecker(db);
    GeoFilter geoFilter;
    ChatFeedModel onKst(geoFilter, dupeChecker);
    ChatFeedModel cluster(geoFilter, dupeChecker);
    onKst.addCandidate(makeCandidate(QStringLiteral("DL3ABC"), QStringLiteral("JN58XX"), QStringLiteral("on4kst")));
    widget.setChatModels(&onKst, &cluster);
    // Row 0: history. Row 1: divider. Row 2: the on4kst candidate.
    const QModelIndex candidateStatusIndex = feedTable->model()->index(2, UnifiedLogWidget::ColumnStatus);
    QSignalSpy secondToggleSpy(&widget, &UnifiedLogWidget::historyInvalidToggleRequested);
    QMetaObject::invokeMethod(&widget, "handleFeedRowClicked", Q_ARG(QModelIndex, candidateStatusIndex));
    QCOMPARE(secondToggleSpy.count(), 0);
}

// A QSO marked invalid (QsoRecord::isInvalid) reads as an "UNGÜLTIG"
// pill (red, the DUPE warning family) plus a dimmed/struck-through row
// -- see UnifiedFeedModel::historyData().
void TestUnifiedLogWidget::invalidQsoRendersAsUngueltigPillAndDimmed()
{
    UnifiedLogWidget widget;
    LogTableModel logModel;
    QsoRecord record = makeLoggedRecord(9, QStringLiteral("OE3AAA"));
    record.isInvalid = true;
    logModel.setRecords({record});
    widget.setLogModel(&logModel);

    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    QAbstractItemModel* model = feedTable->model();
    QVERIFY(model);

    const QModelIndex statusIndex = model->index(0, UnifiedLogWidget::ColumnStatus);
    QCOMPARE(model->data(statusIndex, UnifiedLogWidget::PillTextRole).toString(), QStringLiteral("UNGÜLTIG"));

    const QModelIndex callIndex = model->index(0, UnifiedLogWidget::ColumnCall);
    QVERIFY(model->data(callIndex, Qt::FontRole).value<QFont>().strikeOut());

    // A valid (not-invalid) row carries neither.
    LogTableModel validLogModel;
    validLogModel.setRecords({makeLoggedRecord(10, QStringLiteral("OE3BBB"))});
    UnifiedLogWidget validWidget;
    validWidget.setLogModel(&validLogModel);
    auto* validFeedTable = validWidget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(validFeedTable);
    const QModelIndex validStatusIndex = validFeedTable->model()->index(0, UnifiedLogWidget::ColumnStatus);
    QVERIFY(validFeedTable->model()->data(validStatusIndex, UnifiedLogWidget::PillTextRole).toString().isEmpty());
}

// DXLog-style status line (see UnifiedLogWidget.h's setOperatingMode()
// doc comment / this task's own report for the operator's DXLog.net
// screenshot follow-up): "unknown is a dash" (HAUSSTIL rule 7) before
// any QSO is logged for this widget, then the most recently logged
// QSO's own callsign + time once one exists.
void TestUnifiedLogWidget::statusLineShowsDashBeforeFirstQsoThenLastCallsignAndTime()
{
    UnifiedLogWidget widget;
    auto* lastQsoLabel = widget.findChild<QLabel*>(QLatin1String(UnifiedLogWidget::kLastQsoLabelObjectName));
    QVERIFY(lastQsoLabel);
    QVERIFY(lastQsoLabel->text().contains(QString::fromUtf8("——"))); // Style::unknownDash()

    LogTableModel logModel;
    logModel.setRecords({makeLoggedRecord(1, QStringLiteral("OE5ABC"))});
    widget.setLogModel(&logModel);

    QVERIFY(lastQsoLabel->text().contains(QStringLiteral("OE5ABC")));
    // makeLoggedRecord()'s fixed timestampUtc is "2026-06-13T12:00:00Z".
    QVERIFY(lastQsoLabel->text().contains(QStringLiteral("12:00:00Z")));
}

// The status line updates itself from LogTableModel's own signals (see
// setLogModel()'s own doc comment) -- MainWindow never has to call an
// explicit "a QSO was logged" setter for this half, unlike
// setOperatingMode(). Covers a second QSO appended after setLogModel()
// (a fresh setRecords() reset, matching MainWindow::refreshLogTable()'s
// own approach) landing as the new "last QSO" without any further call
// on the widget itself.
void TestUnifiedLogWidget::statusLineTracksLogModelWithoutExplicitPush()
{
    UnifiedLogWidget widget;
    LogTableModel logModel;
    widget.setLogModel(&logModel);

    auto* lastQsoLabel = widget.findChild<QLabel*>(QLatin1String(UnifiedLogWidget::kLastQsoLabelObjectName));
    QVERIFY(lastQsoLabel);
    QVERIFY(lastQsoLabel->text().contains(QString::fromUtf8("——")));

    QsoRecord second = makeLoggedRecord(2, QStringLiteral("OE9ZZZ"));
    second.timestampUtc = QStringLiteral("2026-06-13T13:30:15Z");
    logModel.setRecords({makeLoggedRecord(1, QStringLiteral("OE5ABC")), second});

    QVERIFY(lastQsoLabel->text().contains(QStringLiteral("OE9ZZZ")));
    QVERIFY(lastQsoLabel->text().contains(QStringLiteral("13:30:15Z")));
    QVERIFY(!lastQsoLabel->text().contains(QStringLiteral("OE5ABC")));
}

// The other half of the status line: ContestSettings::OperatingMode,
// pushed explicitly via setOperatingMode() (MainWindow's
// updateStatusBar(), see this task's report) since it lives outside any
// model this widget already watches.
void TestUnifiedLogWidget::statusLineShowsOperatingMode()
{
    UnifiedLogWidget widget;
    auto* modeLabel = widget.findChild<QLabel*>(QLatin1String(UnifiedLogWidget::kOperatingModeLabelObjectName));
    QVERIFY(modeLabel);
    // Default matches ContestSettings::operatingMode's own default.
    QCOMPARE(modeLabel->text(), QStringLiteral("Modus: S&P"));

    widget.setOperatingMode(ContestSettings::OperatingMode::Run);
    QCOMPARE(modeLabel->text(), QStringLiteral("Modus: Run"));

    widget.setOperatingMode(ContestSettings::OperatingMode::SearchAndPounce);
    QCOMPARE(modeLabel->text(), QStringLiteral("Modus: S&P"));
}

// The Log panel's ⚙ "Kompakt"/"DXLog-Vollspalten" view-mode toggle (see
// UnifiedLogWidget.h's Column enum + setViewMode() doc comments):
// ColumnSerial's own visibility (the original, narrower scope of this
// test's name -- see dxLogFullColumnsShowsSplitColumnsInDxLogOrder()
// below for the fuller DXLog column set/order this toggle grew into).
// The model's own column count/data are unaffected either way, matching
// setViewMode()'s "never tear down/rebuild, every column is always real
// data" design.
void TestUnifiedLogWidget::viewModeTogglesSerialColumnVisibilityOnly()
{
    UnifiedLogWidget widget;
    QCOMPARE(widget.viewMode(), ContestSettings::LogViewMode::Compact);

    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    // Default (Kompakt): hidden.
    QVERIFY(feedTable->isColumnHidden(UnifiedLogWidget::ColumnSerial));
    // Every other column stays visible regardless.
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnCall));

    widget.setViewMode(ContestSettings::LogViewMode::DxLogFullColumns);
    QCOMPARE(widget.viewMode(), ContestSettings::LogViewMode::DxLogFullColumns);
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnSerial));

    widget.setViewMode(ContestSettings::LogViewMode::Compact);
    QVERIFY(feedTable->isColumnHidden(UnifiedLogWidget::ColumnSerial));
}

// ColumnSerial itself is real per-QSO data (QsoRecord::serialSent, from
// ContestDatabase::nextSerialForContest() at log time), not an invented
// "Pts"-style placeholder -- a logged row with a serial shows it, one
// without (the edge case, see UnifiedFeedModel::historyData()'s own
// comment) shows Style::unknownDash(), matching HAUSSTIL rule 7. A
// not-yet-worked spot/chat candidate row has no serial at all -- it
// stays blank, the same treatment its sibling Zeit/Exch Ges. columns
// already get (see UnifiedFeedModel::candidateData()).
void TestUnifiedLogWidget::serialColumnShowsRealSerialSentOrDash()
{
    UnifiedLogWidget widget;
    LogTableModel logModel;

    QsoRecord withSerial = makeLoggedRecord(1, QStringLiteral("OE5ABC"));
    withSerial.serialSent = 12;
    QsoRecord withoutSerial = makeLoggedRecord(2, QStringLiteral("OE9ZZZ"));
    logModel.setRecords({withSerial, withoutSerial});
    widget.setLogModel(&logModel);

    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    QAbstractItemModel* model = feedTable->model();
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 2);

    // UnifiedFeedModel displays history rows chronologically ascending,
    // oldest first (see its own rebuild() comment -- DXLog.net's real
    // order, confirmed 2026-09-11) -- feed row 0 is `withSerial`
    // (logged first), feed row 1 is `withoutSerial` (logged second).
    QCOMPARE(model->data(model->index(0, UnifiedLogWidget::ColumnSerial)).toString(), QStringLiteral("012"));
    QVERIFY(model->data(model->index(1, UnifiedLogWidget::ColumnSerial)).toString().contains(QString::fromUtf8("——")));
}

// ContestSettings::logViewMode persistence -- same
// declared-in-ContestSettings.h/string-serialized-in-.cpp/DB-round-
// tripped-via-settingValue pattern rotorDialStyle already established
// (see ContestSettings.cpp), covered here rather than in
// tests/test_rotor_settings.cpp since this setting is specifically the
// Log panel's own view-mode choice.
void TestUnifiedLogWidget::logViewModeSettingRoundTripsThroughDatabase()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("unifiedlog_view_mode.sqlite")), QStringLiteral("unifiedlog_view_mode")));

    // Default is Compact, unchanged from before this setting existed.
    ContestSettings defaults;
    QCOMPARE(defaults.logViewMode, ContestSettings::LogViewMode::Compact);

    ContestSettings settings;
    settings.logViewMode = ContestSettings::LogViewMode::DxLogFullColumns;
    settings.saveTo(db);

    ContestSettings reloaded;
    reloaded.loadFrom(db);
    QCOMPARE(reloaded.logViewMode, ContestSettings::LogViewMode::DxLogFullColumns);
}

// DXLog-Vollspalten's real column SET and ORDER (Martin, 2026-09-11,
// approving the "log-dxlog-anordnung" design-canvas proposal: "unser
// design bleibt, nur die anordnugn 1:!") -- DXLog.net's own literal
// order (QSO# / Band / Zeit / Call / Sent / Nr. / Rcvd / Nr./Grid),
// this widget's own km/°/Status trailing, and the composed Exch Ges.
// column hidden in favour of the split Sent-side pair. ColumnExchangeRcvd
// is ALWAYS hidden now, in BOTH view modes (same-day follow-up: "ich
// habe bei 59 reingeklickt und der locator war in der gleichen raster"
// -- the combined cell let an edit reach RST too) -- ColumnRstRcvd/
// ColumnSerialGridRcvd are the received exchange's only visible/editable
// form any more, so this test also covers Compact's own received-side
// columns, not just what DxLogFullColumns adds.
void TestUnifiedLogWidget::dxLogFullColumnsShowsSplitColumnsInDxLogOrder()
{
    UnifiedLogWidget widget;
    LogTableModel logModel;

    QsoRecord record = makeLoggedRecord(1, QStringLiteral("OE5AVV"));
    record.rstSent = QStringLiteral("59");
    record.serialSent = 3;
    record.rstRcvd = QStringLiteral("59");
    record.serialRcvd = 2;
    record.gridSquare = QStringLiteral("JN59FF");
    logModel.setRecords({record});
    widget.setLogModel(&logModel);

    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    QAbstractItemModel* model = feedTable->model();
    QVERIFY(model);

    widget.setViewMode(ContestSettings::LogViewMode::DxLogFullColumns);

    // Split columns visible, composed ones hidden.
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnBand));
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnRstSent));
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnSerialSent));
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnRstRcvd));
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnSerialGridRcvd));
    QVERIFY(feedTable->isColumnHidden(UnifiedLogWidget::ColumnExchangeSent));
    QVERIFY(feedTable->isColumnHidden(UnifiedLogWidget::ColumnExchangeRcvd));

    // Real data, split correctly -- not fabricated.
    QCOMPARE(model->data(model->index(0, UnifiedLogWidget::ColumnBand)).toString(), QStringLiteral("144"));
    QCOMPARE(model->data(model->index(0, UnifiedLogWidget::ColumnRstSent)).toString(), QStringLiteral("59"));
    QCOMPARE(model->data(model->index(0, UnifiedLogWidget::ColumnSerialSent)).toString(), QStringLiteral("003"));
    QCOMPARE(model->data(model->index(0, UnifiedLogWidget::ColumnRstRcvd)).toString(), QStringLiteral("59"));
    QCOMPARE(model->data(model->index(0, UnifiedLogWidget::ColumnSerialGridRcvd)).toString(),
             QStringLiteral("002 JN59FF"));

    // Visual order matches DXLog.net's own: QSO# / Band / Zeit / Call /
    // Sent / Nr. / Rcvd / Nr./Grid, our own km/°/Status trailing.
    QHeaderView* header = feedTable->horizontalHeader();
    const QVector<int> expectedOrder = {UnifiedLogWidget::ColumnSerial,     UnifiedLogWidget::ColumnBand,
                                         UnifiedLogWidget::ColumnTime,      UnifiedLogWidget::ColumnCall,
                                         UnifiedLogWidget::ColumnRstSent,   UnifiedLogWidget::ColumnSerialSent,
                                         UnifiedLogWidget::ColumnRstRcvd,   UnifiedLogWidget::ColumnSerialGridRcvd,
                                         UnifiedLogWidget::ColumnDistanceKm, UnifiedLogWidget::ColumnBearingDeg,
                                         UnifiedLogWidget::ColumnStatus};
    for (int visual = 0; visual < expectedOrder.size(); ++visual) {
        QCOMPARE(header->logicalIndex(visual), expectedOrder.at(visual));
    }

    // Switching back to Compact restores the original order and Sent-side
    // column set exactly (same columns viewModeTogglesSerialColumnVisibilityOnly
    // already checks for ColumnSerial/ColumnCall) -- but the received
    // side stays split (ColumnRstRcvd/ColumnSerialGridRcvd), same as
    // DxLogFullColumns: ColumnExchangeRcvd is ALWAYS hidden now, in
    // either mode (see this test's own comment above).
    widget.setViewMode(ContestSettings::LogViewMode::Compact);
    QVERIFY(feedTable->isColumnHidden(UnifiedLogWidget::ColumnBand));
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnExchangeSent));
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnRstRcvd));
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnSerialGridRcvd));
    QVERIFY(feedTable->isColumnHidden(UnifiedLogWidget::ColumnExchangeRcvd));
    // Die Bandspalte steht seit 2026-09-23 auch kompakt an derselben
    // Stelle wie in den Vollspalten (gleich hinter der QSO-Nummer),
    // hier nur ausgeblendet: der Contest dieses Prüfstands hat kein
    // zweites Band. Die Reihenfolge zählt die verborgenen Spalten mit.
    QCOMPARE(header->logicalIndex(0), static_cast<int>(UnifiedLogWidget::ColumnSerial));
    QCOMPARE(header->logicalIndex(1), static_cast<int>(UnifiedLogWidget::ColumnBand));
    QCOMPARE(header->logicalIndex(2), static_cast<int>(UnifiedLogWidget::ColumnTime));
    QCOMPARE(header->logicalIndex(3), static_cast<int>(UnifiedLogWidget::ColumnCall));

    // Und sobald der Contest mehrere Bänder hat, ist sie auch kompakt
    // zu sehen -- ohne sie stünden auf Kurzwelle QSOs von acht Bändern
    // untereinander, ohne dass eines sagt, welches.
    widget.setContestHasSeveralBands(true);
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnBand));
    // Auch nach einem erneuten setViewMode() -- das läuft bei jedem
    // CAT-Takt und holte die Spalte sonst sofort wieder weg.
    widget.setViewMode(ContestSettings::LogViewMode::Compact);
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnBand));
    widget.setContestHasSeveralBands(false);
    QVERIFY(feedTable->isColumnHidden(UnifiedLogWidget::ColumnBand));
}

// Log-panel ⚙ "Eingabezeile: Oben"/"Eingabezeile: Unten" (see
// ContestSettings::LogEntryRowPosition's own doc comment -- operator,
// 2026-09-11, after repeatedly pointing at the exact same spot in a
// screenshot: "zeile unter 04:31" / "genau unter OE5SOS", the table's
// own last visible row): Bottom moves the entry row after the feed
// table in the panel's own layout, Top restores today's original
// order -- the feed table's own row order (chronologically ascending)
// is never touched by this toggle.
void TestUnifiedLogWidget::entryRowPositionBottomPlacesEntryRowAfterFeedTable()
{
    UnifiedLogWidget widget;
    // Bottom is the default now (see ContestSettings::
    // logEntryRowPosition's own doc comment: the feed table's row order
    // became chronologically ascending to match DXLog.net, so Top no
    // longer makes sense as the initial value).
    QCOMPARE(widget.entryRowPosition(), ContestSettings::LogEntryRowPosition::Bottom);

    // The QScrollArea wrapping the entry row (see kEntryRowScrollObjectName's
    // own doc comment) is the actual m_mainLayout item now -- the entry
    // row's own content widget (kEntryRowObjectName) sits one level
    // deeper, inside that scroll area's viewport.
    auto* entryRowScroll = widget.findChild<QWidget*>(QLatin1String(UnifiedLogWidget::kEntryRowScrollObjectName));
    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(entryRowScroll);
    QVERIFY(feedTable);
    QLayout* layout = widget.layout();
    QVERIFY(layout);

    // Default (Bottom): entry row comes after the feed table.
    QVERIFY(layout->indexOf(entryRowScroll) > layout->indexOf(feedTable));

    widget.setEntryRowPosition(ContestSettings::LogEntryRowPosition::Top);
    QCOMPARE(widget.entryRowPosition(), ContestSettings::LogEntryRowPosition::Top);
    QVERIFY(layout->indexOf(entryRowScroll) < layout->indexOf(feedTable));

    // Switching back restores the default order exactly.
    widget.setEntryRowPosition(ContestSettings::LogEntryRowPosition::Bottom);
    QVERIFY(layout->indexOf(entryRowScroll) > layout->indexOf(feedTable));
}

void TestUnifiedLogWidget::logEntryRowPositionSettingRoundTripsThroughDatabase()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ContestDatabase db;
    QVERIFY(db.open(dir.filePath(QStringLiteral("unifiedlog_entry_row_position.sqlite")),
                     QStringLiteral("unifiedlog_entry_row_position")));

    ContestSettings defaults;
    QCOMPARE(defaults.logEntryRowPosition, ContestSettings::LogEntryRowPosition::Bottom);

    ContestSettings settings;
    settings.logEntryRowPosition = ContestSettings::LogEntryRowPosition::Top;
    settings.saveTo(db);

    ContestSettings reloaded;
    reloaded.loadFrom(db);
    QCOMPARE(reloaded.logEntryRowPosition, ContestSettings::LogEntryRowPosition::Top);
}

// A logged duplicate (0 points, "D" in the EDI) carries a DUPE pill in
// its Status cell -- found 2026-09-21 checking the log end to end: a
// logged dupe looked exactly like a valid QSO, only the entry row ever
// said DUPE. Amber, not the red UNGÜLTIG uses.
void TestUnifiedLogWidget::dupeHistoryRowRendersAsDupePill()
{
    UnifiedLogWidget widget;
    LogTableModel logModel;
    QsoRecord record = makeLoggedRecord(11, QStringLiteral("OE3AAA"));
    record.isDupe = true;
    logModel.setRecords({record});
    widget.setLogModel(&logModel);

    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    const QModelIndex statusIndex = feedTable->model()->index(0, UnifiedLogWidget::ColumnStatus);
    QCOMPARE(feedTable->model()->data(statusIndex, UnifiedLogWidget::PillTextRole).toString(), QStringLiteral("DUPE"));
    // Not struck through -- it is a real contact, just worth 0.
    const QModelIndex callIndex = feedTable->model()->index(0, UnifiedLogWidget::ColumnCall);
    QVERIFY(!feedTable->model()->data(callIndex, Qt::FontRole).value<QFont>().strikeOut());
}

// The operator's own layout (2026-09-21) had a 620px-wide Log panel:
// the fixed column grid overflowed into a horizontal scrollbar with
// km/°/Status (the DUPE/UNGÜLTIG pills, the invalid-toggle click) off
// the right edge. Now the visible columns shrink to the viewport (see
// UnifiedLogWidget.cpp's kMinColumnWidths) and the entry row's cells
// follow the same widths, so the two keep lining up.
void TestUnifiedLogWidget::narrowPanelFitsTheColumnsAndTheEntryRowFollows()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(rstSerialGridFields());
    widget.resize(620, 400);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QCoreApplication::processEvents();

    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    int visibleWidth = 0;
    for (int col = 0; col < UnifiedLogWidget::ColumnCount; ++col) {
        if (!feedTable->isColumnHidden(col)) {
            visibleWidth += feedTable->columnWidth(col);
        }
    }
    QVERIFY2(visibleWidth <= feedTable->viewport()->width(),
             qPrintable(QStringLiteral("columns %1 > viewport %2").arg(visibleWidth).arg(feedTable->viewport()->width())));
    // Shrunk, but never below what the text needs.
    QVERIFY(feedTable->columnWidth(UnifiedLogWidget::ColumnCall) < 115);
    QVERIFY(feedTable->columnWidth(UnifiedLogWidget::ColumnExchangeSent) >= 126);

    QLineEdit* callsign = findEditByPlaceholder(widget, QStringLiteral("Callsign"));
    QVERIFY(callsign);
    QCOMPARE(callsign->width(), feedTable->columnWidth(UnifiedLogWidget::ColumnCall));

    // Wide again: back to the design grid.
    widget.resize(1300, 400);
    QCoreApplication::processEvents();
    QCOMPARE(feedTable->columnWidth(UnifiedLogWidget::ColumnCall), 115);
    QCOMPARE(callsign->width(), 115);
}

// The newest QSO must be on screen after logging -- the operator's own
// 7-QSO log (2026-09-21) sat scrolled to the top with the last row
// hidden below the fold.
void TestUnifiedLogWidget::aNewlyLoggedQsoScrollsIntoView()
{
    UnifiedLogWidget widget;
    widget.setEntryRowPosition(ContestSettings::LogEntryRowPosition::Bottom);
    LogTableModel logModel;
    widget.setLogModel(&logModel);
    widget.resize(900, 220);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    QVector<QsoRecord> records;
    for (int i = 0; i < 30; ++i) {
        records.append(makeLoggedRecord(100 + i, QStringLiteral("DL%1AAA").arg(i)));
    }
    logModel.setRecords(records);
    QCoreApplication::processEvents();

    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    const QModelIndex last = feedTable->model()->index(29, UnifiedLogWidget::ColumnCall);
    // The scroll is queued behind the layout pass that shrinks the table
    // to the panel (see UnifiedLogWidget::rebuildFeedRows()).
    QTRY_VERIFY2(feedTable->viewport()->rect().contains(feedTable->visualRect(last).center()),
                 qPrintable(QStringLiteral("last row at %1, viewport %2x%3")
                                .arg(feedTable->visualRect(last).y())
                                .arg(feedTable->viewport()->width())
                                .arg(feedTable->viewport()->height())));
}

// "nummern bitte automatisch bei 001 und nicht bei 1 anfangen"
// (operator, 2026-09-21): a serial reads as three digits in the QSO#
// and Nr./Grid columns and in the entry row's own Nr. field the moment
// the operator leaves it.
void TestUnifiedLogWidget::serialsReadAsThreeDigitsEverywhere()
{
    UnifiedLogWidget widget;
    widget.setExchangeFields(rstSerialGridFields());
    LogTableModel logModel;
    QsoRecord record = makeLoggedRecord(21, QStringLiteral("OE5OHO"));
    record.serialSent = 8;
    record.serialRcvd = 4;
    record.gridSquare = QStringLiteral("JN78EG");
    logModel.setRecords({record});
    widget.setLogModel(&logModel);

    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);
    QCOMPARE(feedTable->model()->index(0, UnifiedLogWidget::ColumnSerial).data().toString(), QStringLiteral("008"));
    QCOMPARE(feedTable->model()->index(0, UnifiedLogWidget::ColumnSerialGridRcvd).data().toString(),
             QStringLiteral("004 JN78EG"));

    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    QLineEdit* serial = findEditByPlaceholder(widget, QStringLiteral("Serial"));
    QVERIFY(serial);
    serial->setFocus();
    QTest::keyClicks(serial, QStringLiteral("4"));
    QTest::keyClick(serial, Qt::Key_Space);
    QCOMPARE(serial->text(), QStringLiteral("004"));
    QCOMPARE(widget.exchangeReceived().value(QStringLiteral("serial")), QStringLiteral("004"));
}

// "sollte ein dupe kommen soll sofort die nummer stehen, mit der ich
// geloggt habe, inkl. uhrzeit" (operator, 2026-09-21): while the entry
// row says DUPE, the status line carries MainWindow's sentence about the
// earlier QSO; gone again with the DUPE.
void TestUnifiedLogWidget::dupeDetailReplacesTheLastQsoLine()
{
    UnifiedLogWidget widget;
    LogTableModel logModel;
    logModel.setRecords({makeLoggedRecord(31, QStringLiteral("OE5AOO"))});
    widget.setLogModel(&logModel);
    auto* lastQso = widget.findChild<QLabel*>(QLatin1String(UnifiedLogWidget::kLastQsoLabelObjectName));
    QVERIFY(lastQso);
    QVERIFY(lastQso->text().startsWith(QStringLiteral("Letzter QSO: OE5AOO")));

    widget.setDupeIndicator(true, QStringLiteral("DUPE: OE5AOO schon geloggt als Nr. 003 um 19:28 UTC auf 144"));
    QCOMPARE(lastQso->text(), QStringLiteral("DUPE: OE5AOO schon geloggt als Nr. 003 um 19:28 UTC auf 144"));
    auto* pill = widget.findChild<QLabel*>(QLatin1String(UnifiedLogWidget::kStatusPillObjectName));
    QVERIFY(pill);
    QCOMPARE(pill->text(), QStringLiteral("DUPE"));

    widget.setDupeIndicator(false);
    QVERIFY(lastQso->text().startsWith(QStringLiteral("Letzter QSO: OE5AOO")));
    QVERIFY(pill->text().isEmpty());
}

// Not QTEST_APPLESS_MAIN: UnifiedLogWidget is a QWidget subclass, which
// needs a live QApplication (not just QCoreApplication) to construct.
// Kilometer und Grad stehen und fallen mit dem Locator: tauscht der
// Contest keinen -- auf Kurzwelle tut das keiner --, bleiben beide
// Spalten für immer leer und nehmen nur Platz weg.
void TestUnifiedLogWidget::distanceColumnsFollowTheExchangeFields()
{
    UnifiedLogWidget widget;
    widget.resize(900, 300);
    auto* feedTable = widget.findChild<QTableView*>(QLatin1String(UnifiedLogWidget::kFeedTableObjectName));
    QVERIFY(feedTable);

    // Mit Locator im Exchange: beide Spalten da.
    widget.setExchangeFields(rstSerialGridFields());
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnDistanceKm));
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnBearingDeg));

    // Kurzwelle: RST und laufende Nummer, kein Locator.
    ContestDefinition::ExchangeField rst;
    rst.key = QStringLiteral("rst");
    rst.label = QStringLiteral("RST");
    rst.type = QStringLiteral("rst");
    ContestDefinition::ExchangeField serial;
    serial.key = QStringLiteral("serial");
    serial.label = QStringLiteral("Nr.");
    serial.type = QStringLiteral("int");
    serial.autoIncrement = true;
    widget.setExchangeFields({rst, serial});
    QVERIFY(feedTable->isColumnHidden(UnifiedLogWidget::ColumnDistanceKm));
    QVERIFY(feedTable->isColumnHidden(UnifiedLogWidget::ColumnBearingDeg));
    // Der Rest bleibt, wie er war.
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnCall));
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnTime));

    // Und zurück: ein UKW-Contest bringt sie wieder.
    widget.setExchangeFields(rstSerialGridFields());
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnDistanceKm));
    QVERIFY(!feedTable->isColumnHidden(UnifiedLogWidget::ColumnBearingDeg));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestUnifiedLogWidget tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_unifiedlog_widget.moc"
