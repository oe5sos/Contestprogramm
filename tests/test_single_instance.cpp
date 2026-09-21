#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "app/SingleInstanceGuard.h"

using namespace Contestprogramm;

// app/SingleInstanceGuard.h: one instance per data directory, a second
// start hands over to the first.
class TestSingleInstance : public QObject
{
    Q_OBJECT

private slots:
    void secondStartOnTheSameDirectoryHandsOverAndIsRefused();
    void differentDirectoriesDoNotInterfere();
    void aFinishedInstanceFreesTheDirectory();
};

void TestSingleInstance::secondStartOnTheSameDirectoryHandsOverAndIsRefused()
{
    QTemporaryDir dir;
    SingleInstanceGuard first(dir.path());
    QVERIFY(first.tryAcquire());
    QSignalSpy activated(&first, &SingleInstanceGuard::activateRequested);

    SingleInstanceGuard second(dir.path());
    QVERIFY(!second.tryAcquire());
    // The running instance is asked to show itself.
    QTRY_COMPARE(activated.count(), 1);

    // And a third, the same.
    SingleInstanceGuard third(dir.path());
    QVERIFY(!third.tryAcquire());
    QTRY_COMPARE(activated.count(), 2);
}

void TestSingleInstance::differentDirectoriesDoNotInterfere()
{
    QTemporaryDir a;
    QTemporaryDir b;
    SingleInstanceGuard onA(a.path());
    SingleInstanceGuard onB(b.path());
    QVERIFY(onA.tryAcquire());
    QVERIFY(onB.tryAcquire());
    QVERIFY(SingleInstanceGuard::serverNameFor(a.path()) != SingleInstanceGuard::serverNameFor(b.path()));
}

void TestSingleInstance::aFinishedInstanceFreesTheDirectory()
{
    QTemporaryDir dir;
    {
        SingleInstanceGuard first(dir.path());
        QVERIFY(first.tryAcquire());
    }
    SingleInstanceGuard next(dir.path());
    QVERIFY(next.tryAcquire());
    // release() before a planned restart: the successor is not a
    // second instance, and the released guard no longer answers.
    next.release();
    SingleInstanceGuard successor(dir.path());
    QVERIFY(successor.tryAcquire());
}

QTEST_GUILESS_MAIN(TestSingleInstance)
#include "test_single_instance.moc"
