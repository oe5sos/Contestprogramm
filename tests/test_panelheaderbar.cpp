#include <QtTest>

#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>

#include "ui/PanelHeaderBar.h"

using namespace Contestprogramm;

// PanelHeaderBar is a QWidget subclass (real QLabel/QPushButton
// children), so this needs a live QApplication -- same reasoning as
// test_panelcontainer.cpp/test_rotorwidget.cpp. Covers the new ⚙
// options affordance the same way test_panelcontainer.cpp already
// covers the existing lock affordance: hidden by default (so every
// existing PanelHeaderBar call site that never opts in stays visually
// unaffected -- see PanelContainerWidget.cpp's own
// setLockAffordanceEnabled(true) call for the one place that does),
// shown only via setOptionsAffordanceEnabled(true), and emitting
// optionsRequested() only on an actual click, never as a side effect of
// construction or of the enable/disable calls themselves.
class TestPanelHeaderBar : public QObject
{
    Q_OBJECT

private slots:
    void optionsButtonHiddenByDefault();
    void setOptionsAffordanceEnabledShowsButton();
    void disablingHidesButtonAgain();
    void clickingOptionsButtonEmitsOptionsRequested();
    void constructionDoesNotEmitOptionsRequested();
};

namespace {

// PanelHeaderBar keeps its buttons private; findChild() by the stable
// objectName PanelHeaderBar.cpp's constructor sets on this one button
// reaches it without needing a new test-only accessor on the class
// itself, and without depending on construction order relative to the
// (also-private) lock button.
QPushButton* findOptionsButton(PanelHeaderBar& bar)
{
    return bar.findChild<QPushButton*>(QStringLiteral("panelHeaderOptionsButton"));
}

} // namespace

void TestPanelHeaderBar::optionsButtonHiddenByDefault()
{
    // isHidden() (the explicit shown/hidden flag setVisible() controls)
    // rather than isVisible() (actual on-screen visibility, which also
    // depends on every ancestor being shown) -- `bar` itself is never
    // shown in this test, so isVisible() would read false regardless of
    // what setOptionsAffordanceEnabled() did.
    PanelHeaderBar bar(QStringLiteral("Test"));
    QPushButton* button = findOptionsButton(bar);
    QVERIFY(button);
    QVERIFY(button->isHidden());
}

void TestPanelHeaderBar::setOptionsAffordanceEnabledShowsButton()
{
    PanelHeaderBar bar(QStringLiteral("Test"));
    bar.setOptionsAffordanceEnabled(true);
    QPushButton* button = findOptionsButton(bar);
    QVERIFY(button);
    QVERIFY(!button->isHidden());
}

void TestPanelHeaderBar::disablingHidesButtonAgain()
{
    PanelHeaderBar bar(QStringLiteral("Test"));
    bar.setOptionsAffordanceEnabled(true);
    QVERIFY(!findOptionsButton(bar)->isHidden());

    bar.setOptionsAffordanceEnabled(false);
    QVERIFY(findOptionsButton(bar)->isHidden());
}

void TestPanelHeaderBar::clickingOptionsButtonEmitsOptionsRequested()
{
    PanelHeaderBar bar(QStringLiteral("Test"));
    bar.setOptionsAffordanceEnabled(true);
    QSignalSpy spy(&bar, &PanelHeaderBar::optionsRequested);

    findOptionsButton(bar)->click();

    QCOMPARE(spy.count(), 1);
}

void TestPanelHeaderBar::constructionDoesNotEmitOptionsRequested()
{
    // Same "click only, not programmatic" contract lockToggled() already
    // has (see PanelHeaderBar.h) -- enabling the affordance or just
    // constructing the widget must never itself look like a click.
    PanelHeaderBar bar(QStringLiteral("Test"));
    QSignalSpy spy(&bar, &PanelHeaderBar::optionsRequested);

    bar.setOptionsAffordanceEnabled(true);
    bar.setOptionsAffordanceEnabled(false);

    QCOMPARE(spy.count(), 0);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestPanelHeaderBar tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_panelheaderbar.moc"
