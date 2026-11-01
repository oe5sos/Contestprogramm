#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    void theRestartGetsTheProgramTheNewerBuildWasStartedAs();
    void anOlderBuildsHandOverWithoutAProgramStillWorks();
    void aHandedPathThatIsNotAProgramIsDropped();
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

namespace {
// A stand-in for a second build installed beside the running one, as on
// 2026-09-28 (Contestprogramm-neu.app next to Contestprogramm.app): an
// executable file in a directory with a space in its name. Named like
// this test's own executable, so it counts as a program on Windows too
// (there by its suffix).
QString makeProgramBeside(const QTemporaryDir& dir)
{
    const QString program = dir.filePath(QStringLiteral("Contestprogramm neu.app/Contents/MacOS/"))
        + QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    QDir().mkpath(QFileInfo(program).absolutePath());
    QFile file(program);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write("#!/bin/sh\n");
    file.close();
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                        | QFileDevice::ReadUser | QFileDevice::ExeUser);
    return program;
}

// What a second start of an older build puts on the wire. The socket
// stays open until the caller is done: a named pipe hung up on right
// after the write can lose the line (Windows, see tryAcquire()).
bool handOverLike(QLocalSocket& socket, const QString& dataDir, const QByteArray& line)
{
    socket.connectToServer(SingleInstanceGuard::serverNameFor(dataDir));
    if (!socket.waitForConnected(1500)) {
        return false;
    }
    socket.write(line);
    socket.flush();
    return true;
}
} // namespace

// The rebuilt program was started from a different place than the
// running one (2026-09-28: Contestprogramm-neu.app beside
// Contestprogramm.app). The restart must run THAT program -- the
// running one's own path brought the old program back.
void TestSingleInstance::theRestartGetsTheProgramTheNewerBuildWasStartedAs()
{
    QTemporaryDir dir;
    QTemporaryDir installed;
    const QString program = makeProgramBeside(installed);
    QVERIFY(!program.isEmpty());
    QVERIFY(program.contains(QLatin1Char(' ')));

    SingleInstanceGuard first(dir.path());
    first.setBuildStamp(QStringLiteral("1000"));
    QVERIFY(first.tryAcquire());
    QSignalSpy activated(&first, &SingleInstanceGuard::activateRequested);
    QSignalSpy newer(&first, &SingleInstanceGuard::newerBuildStarted);

    SingleInstanceGuard beside(dir.path());
    beside.setBuildStamp(QStringLiteral("2000"));
    beside.setProgramPath(program);
    QVERIFY(!beside.tryAcquire());
    QTRY_COMPARE(newer.count(), 1);
    QCOMPARE(newer.at(0).at(0).toString(), program);

    // A copy of the SAME build elsewhere is the same program: only raised.
    SingleInstanceGuard copy(dir.path());
    copy.setBuildStamp(QStringLiteral("1000"));
    copy.setProgramPath(program);
    QVERIFY(!copy.tryAcquire());
    QTRY_COMPARE(activated.count(), 1);
    QCOMPARE(newer.count(), 1);
}

// Builds before this change send "raise <stamp>\n", the very first ones
// a bare "raise\n". Both keep working: a different stamp still restarts
// (with no program named, main.cpp falls back to its own path), no
// stamp still raises.
void TestSingleInstance::anOlderBuildsHandOverWithoutAProgramStillWorks()
{
    QTemporaryDir dir;
    SingleInstanceGuard first(dir.path());
    first.setBuildStamp(QStringLiteral("1000"));
    QVERIFY(first.tryAcquire());
    QSignalSpy activated(&first, &SingleInstanceGuard::activateRequested);
    QSignalSpy newer(&first, &SingleInstanceGuard::newerBuildStarted);

    QLocalSocket stampOnly;
    QVERIFY(handOverLike(stampOnly, dir.path(), "raise 2000\n"));
    QTRY_COMPARE(newer.count(), 1);
    QVERIFY(newer.at(0).at(0).toString().isEmpty());
    // It still gets its "ok" and can hang up.
    QTRY_VERIFY(stampOnly.canReadLine());
    QCOMPARE(stampOnly.readLine(), QByteArray("ok\n"));

    QLocalSocket bare;
    QVERIFY(handOverLike(bare, dir.path(), "raise\n"));
    QTRY_COMPARE(activated.count(), 1);
    QCOMPARE(newer.count(), 1);
}

// Whatever comes on the socket is not started blindly: a path that is
// not an absolute path to an executable file is dropped, and the
// restart falls back to the running program's own path.
void TestSingleInstance::aHandedPathThatIsNotAProgramIsDropped()
{
    QTemporaryDir dir;
    SingleInstanceGuard first(dir.path());
    first.setBuildStamp(QStringLiteral("1000"));
    QVERIFY(first.tryAcquire());
    QSignalSpy newer(&first, &SingleInstanceGuard::newerBuildStarted);

    const QByteArray missing = QDir(dir.path()).filePath(QStringLiteral("gibt es nicht/Contestprogramm"))
                                   .toUtf8().toPercentEncoding("/");
    const QByteArray notAProgram = dir.path().toUtf8().toPercentEncoding("/"); // a directory
    const QList<QByteArray> lines = {
        "raise 2000 " + missing + '\n',
        "raise 2000 " + notAProgram + '\n',
        "raise 2000 Contestprogramm\n", // relative: would be looked up on PATH
    };
    for (const QByteArray& line : lines) {
        QLocalSocket socket;
        QVERIFY(handOverLike(socket, dir.path(), line));
        const int before = int(newer.count());
        QTRY_COMPARE(int(newer.count()), before + 1);
        QVERIFY2(newer.last().at(0).toString().isEmpty(), line.constData());
    }
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
