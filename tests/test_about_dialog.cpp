// Hilfe > Über Contestprogramm: version line from the generated
// BuildInfo.h, and the data facts as given.

#include <QtTest>

#include <QApplication>
#include <QRegularExpression>

#include "ui/AboutDialog.h"

using namespace Contestprogramm;

class TestAboutDialog : public QObject
{
    Q_OBJECT

private slots:
    void versionLineCarriesVersionCommitAndDate();
    void textListsTheDataFacts();
};

void TestAboutDialog::versionLineCarriesVersionCommitAndDate()
{
    const QString line = AboutDialog::versionLine();
    // "Contestprogramm 0.1.0 (e1f6f93a53+, 2026-09-21)" -- the commit
    // is a short hash (with "+" for an uncommitted tree) or "unbekannt".
    const QRegularExpression shape(QStringLiteral(R"(^Contestprogramm \d+\.\d+\.\d+ \(([0-9a-f]{7,12}\+?|unbekannt), \d{4}-\d{2}-\d{2}\)$)"));
    QVERIFY2(shape.match(line).hasMatch(), qPrintable(line));
}

void TestAboutDialog::textListsTheDataFacts()
{
    AboutDialog::Facts facts;
    facts.databasePath = QStringLiteral("/Users/martin/Library/Application Support/Contestprogramm/contestprogramm.sqlite");
    facts.backupDirectory = QStringLiteral("/Users/martin/Library/Application Support/Contestprogramm/backups");
    AboutDialog dialog(facts);
    const QString text = dialog.text();
    QVERIFY(text.contains(AboutDialog::versionLine()));
    QVERIFY(text.contains(facts.databasePath));
    QVERIFY(text.contains(facts.backupDirectory));
    QVERIFY(text.contains(QStringLiteral("keine"))); // no second backup folder
    QVERIFY(text.contains(QString::fromLatin1(qVersion())));

    facts.mirrorDirectory = QStringLiteral("/Volumes/STICK/Contestprogramm");
    AboutDialog withMirror(facts);
    QVERIFY(withMirror.text().contains(QStringLiteral("/Volumes/STICK/Contestprogramm")));
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestAboutDialog tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_about_dialog.moc"
