#pragma once

#include <QString>

namespace Contestprogramm {

// Putting a backup copy (data/LogBackup.h) back in place of the live
// database. The live connection must be closed first; the copy replaces
// the file and its -wal/-shm companions, and the program then restarts
// on it (MainWindow::restoreBackup -> restartRequested()). Nothing is
// merged: a restore is "back to that minute", and the state before it
// is itself backed up first so the step can be undone the same way.
bool restoreDatabaseFile(const QString& backupPath, const QString& livePath, QString* errorOut = nullptr);

// One line about a backup file for the restore dialog -- QSOs per
// contest in it ("47 QSOs · IARU_R1_UHF; 7 QSOs · IARU_R1_VHF_UHF"),
// read through a connection of its own, never the live one.
QString summarizeBackup(const QString& backupPath);

} // namespace Contestprogramm
