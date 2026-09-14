#include <QtTest>

#include <QApplication>
#include <QLabel>
#include <QSignalSpy>

#include "ui/PanelContainerWidget.h"

using namespace Contestprogramm;

// PanelContainerWidget is a QWidget subclass (drag/resize state, child
// widgets), so this needs a live QApplication -- same reasoning as
// test_rotorwidget.cpp/test_mapwidget.cpp. Covers the movable/resizable/
// lockable panel wave's one hard, directly-testable guarantee: a locked
// panel's geometry cannot change until it is unlocked again --
// trySetGeometry() is the single gate every drag/resize/programmatic
// move funnels through (see the class comment in PanelContainerWidget.h),
// so exercising it directly here covers the lock behavior without
// needing to simulate real QMouseEvent drag sequences.
class TestPanelContainerWidget : public QObject
{
    Q_OBJECT

private slots:
    void objectNameAndIdMatchTheRegisteredId();
    void trySetGeometryAppliesWhenUnlocked();
    void trySetGeometryClampsBelowMinimumSize();
    void lockedPanelRejectsGeometryChanges();
    void unlockingRestoresGeometryChanges();
    void setLockedEmitsLockedChangedExactlyOnceOnRealChange();
    void chromelessModeStillExposesContentAndGeometryGate();
};

namespace {

QWidget* makeContent()
{
    // No parent -- PanelContainerWidget's constructor reparents it.
    return new QLabel(QStringLiteral("content"));
}

} // namespace

void TestPanelContainerWidget::objectNameAndIdMatchTheRegisteredId()
{
    PanelContainerWidget panel(QStringLiteral("testPanel"), QStringLiteral("Test"), makeContent(), false);
    QCOMPARE(panel.objectName(), QStringLiteral("testPanel"));
    QCOMPARE(panel.id(), QStringLiteral("testPanel"));
}

void TestPanelContainerWidget::trySetGeometryAppliesWhenUnlocked()
{
    PanelContainerWidget panel(QStringLiteral("p"), QStringLiteral("P"), makeContent(), false);
    QVERIFY(!panel.isLocked());
    QVERIFY(panel.trySetGeometry(QRect(10, 20, 300, 200)));
    QCOMPARE(panel.geometry(), QRect(10, 20, 300, 200));
}

void TestPanelContainerWidget::trySetGeometryClampsBelowMinimumSize()
{
    PanelContainerWidget panel(QStringLiteral("p"), QStringLiteral("P"), makeContent(), false);
    QVERIFY(panel.trySetGeometry(QRect(5, 5, 10, 10)));
    QCOMPARE(panel.geometry().width(), PanelContainerWidget::kMinWidth);
    QCOMPARE(panel.geometry().height(), PanelContainerWidget::kMinHeight);
}

void TestPanelContainerWidget::lockedPanelRejectsGeometryChanges()
{
    PanelContainerWidget panel(QStringLiteral("p"), QStringLiteral("P"), makeContent(), false);
    QVERIFY(panel.trySetGeometry(QRect(0, 0, 300, 200)));

    panel.setLocked(true);
    QVERIFY(panel.isLocked());
    QVERIFY(!panel.trySetGeometry(QRect(50, 50, 400, 300)));
    // Rejected -- the panel must still sit exactly where it was.
    QCOMPARE(panel.geometry(), QRect(0, 0, 300, 200));
}

void TestPanelContainerWidget::unlockingRestoresGeometryChanges()
{
    PanelContainerWidget panel(QStringLiteral("p"), QStringLiteral("P"), makeContent(), false);
    panel.setLocked(true);
    QVERIFY(!panel.trySetGeometry(QRect(10, 10, 300, 200)));

    panel.setLocked(false);
    QVERIFY(panel.trySetGeometry(QRect(10, 10, 300, 200)));
    QCOMPARE(panel.geometry(), QRect(10, 10, 300, 200));
}

void TestPanelContainerWidget::setLockedEmitsLockedChangedExactlyOnceOnRealChange()
{
    PanelContainerWidget panel(QStringLiteral("p"), QStringLiteral("P"), makeContent(), false);
    QSignalSpy spy(&panel, &PanelContainerWidget::lockedChanged);

    panel.setLocked(true);
    panel.setLocked(true); // same value again -- must not re-emit
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.constFirst().constFirst().toBool(), true);

    panel.setLocked(false);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.constLast().constFirst().toBool(), false);
}

void TestPanelContainerWidget::chromelessModeStillExposesContentAndGeometryGate()
{
    // Chromeless mode (MapWidget's own use) skips PanelHeaderBar
    // entirely -- content() and the lock gate must still work the same
    // way as header mode.
    QWidget* content = makeContent();
    PanelContainerWidget panel(QStringLiteral("map"), QStringLiteral("Karte"), content,
                                /*contentHasOwnChrome=*/true);
    QCOMPARE(panel.content(), content);
    QVERIFY(panel.trySetGeometry(QRect(0, 0, 300, 200)));

    panel.setLocked(true);
    QVERIFY(!panel.trySetGeometry(QRect(100, 100, 300, 200)));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestPanelContainerWidget tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_panelcontainer.moc"
