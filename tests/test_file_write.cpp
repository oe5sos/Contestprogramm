#include <QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "data/FileWrite.h"

using namespace Contestprogramm;

class TestFileWrite : public QObject
{
    Q_OBJECT

private slots:
    void writesEveryByteExactlyAsGiven();
    void unwritablePathFailsWithAMessage();
};

void TestFileWrite::writesEveryByteExactlyAsGiven()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("export.edi"));

    // CRLF and a non-ASCII byte: both must survive untouched (no Text
    // mode, no re-encoding) -- an EDI file is CRLF-terminated Latin-1.
    const QByteArray bytes = QByteArrayLiteral("[REG1TEST;1]\r\nRName=Gr\xFC\x62\x65r\r\n");
    QString error = QStringLiteral("nicht geleert");
    QVERIFY2(writeFileChecked(path, bytes, &error), qPrintable(error));
    QVERIFY2(error.isEmpty(), qPrintable(error));

    QFile written(path);
    QVERIFY(written.open(QIODevice::ReadOnly));
    QCOMPARE(written.readAll(), bytes);

    // Writing again truncates rather than appending.
    QVERIFY(writeFileChecked(path, QByteArrayLiteral("kurz"), &error));
    QFile again(path);
    QVERIFY(again.open(QIODevice::ReadOnly));
    QCOMPARE(again.readAll(), QByteArrayLiteral("kurz"));
}

void TestFileWrite::unwritablePathFailsWithAMessage()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // A directory that does not exist: the export must fail, say why,
    // and leave nothing behind -- never report a finished export.
    const QString path = dir.filePath(QStringLiteral("gibt-es-nicht/export.edi"));
    QString error;
    QVERIFY(!writeFileChecked(path, QByteArrayLiteral("x"), &error));
    QVERIFY2(!error.isEmpty(), "a failed write has to explain itself");
    QVERIFY(!QFile::exists(path));

    // A path that is a directory, not a file.
    QString dirError;
    QVERIFY(!writeFileChecked(dir.path(), QByteArrayLiteral("x"), &dirError));
    QVERIFY(!dirError.isEmpty());

    // No errorOut at all is allowed.
    QVERIFY(!writeFileChecked(path, QByteArrayLiteral("x")));
}

QTEST_MAIN(TestFileWrite)
#include "test_file_write.moc"
