#include <QtTest>

#include <QLocalSocket>
#include <QPointer>
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
    void aStartFromANewerBuildAsksForARestart();
    void releasingFromTheRestartSignalDoesNotPullTheSocketFromUnderItsOwnRead();
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

// Built and started again: the hand-over carries the build stamp; a
// different one makes the running instance ask for its own restart
// (main.cpp), the same one leaves it at a plain raise.
void TestSingleInstance::aStartFromANewerBuildAsksForARestart()
{
    QTemporaryDir dir;
    SingleInstanceGuard first(dir.path());
    first.setBuildStamp(QStringLiteral("1000"));
    QVERIFY(first.tryAcquire());
    QSignalSpy activated(&first, &SingleInstanceGuard::activateRequested);
    QSignalSpy newer(&first, &SingleInstanceGuard::newerBuildStarted);

    SingleInstanceGuard sameBuild(dir.path());
    sameBuild.setBuildStamp(QStringLiteral("1000"));
    QVERIFY(!sameBuild.tryAcquire());
    QTRY_COMPARE(activated.count(), 1);
    QCOMPARE(newer.count(), 0);

    SingleInstanceGuard rebuilt(dir.path());
    rebuilt.setBuildStamp(QStringLiteral("2000"));
    QVERIFY(!rebuilt.tryAcquire());
    QTRY_COMPARE(newer.count(), 1);
    QCOMPARE(activated.count(), 1);
}

// Absturz 2026-09-28 11:01:33 (Contestprogramm-2026-09-28-110149.ips):
// a second build was started beside the running one, main.cpp's restart
// handler ran release() straight from newerBuildStarted -- that deleted
// the QLocalServer and with it the client socket, while the socket was
// still inside its own readyRead. Qt emits channelReadyRead() on it right
// after, into freed memory: EXC_BAD_ACCESS at 0x30 in doActivate. Under
// AddressSanitizer with free_fill_byte=0 (freed memory zeroed, like the
// crash) the unfixed code dies here at exactly the reported offsets.
void TestSingleInstance::releasingFromTheRestartSignalDoesNotPullTheSocketFromUnderItsOwnRead()
{
    QTemporaryDir dir;
    SingleInstanceGuard first(dir.path());
    first.setBuildStamp(QStringLiteral("1000"));
    QVERIFY(first.tryAcquire());
    int restarts = 0;
    bool socketOutlivedRelease = false;
    // The same order as main.cpp's restart lambda: release() first.
    connect(&first, &SingleInstanceGuard::newerBuildStarted, this, [&] {
        // The connection that brought the line -- this slot runs inside
        // its readyRead, so it must still exist when release() returns
        // (only an ASan build notices the use itself, and only with
        // free_fill_byte; this check fails on every platform).
        const QPointer<QLocalSocket> client = first.findChild<QLocalSocket*>();
        QVERIFY(client);
        first.release();
        socketOutlivedRelease = !client.isNull();
        ++restarts;
    });

    SingleInstanceGuard rebuilt(dir.path());
    rebuilt.setBuildStamp(QStringLiteral("2000"));
    QVERIFY(!rebuilt.tryAcquire());
    QTRY_COMPARE(restarts, 1);
    QVERIFY(socketOutlivedRelease);
    // Let the deferred clean-up run, then the directory is free again.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTest::qWait(50);
    SingleInstanceGuard successor(dir.path());
    QVERIFY(successor.tryAcquire());
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
