#pragma once

#include <QByteArray>
#include <QString>

namespace Contestprogramm {

// Writes `bytes` to `path` and only reports success when every byte of
// it actually reached the disk.
//
// Why this exists: QFile::write() returns how much it wrote and
// QFile::close() flushes without saying whether the flush worked, so
// the plain open/write/close sequence the exporters used reported
// "Log exportiert" for a half-written file as readily as for a whole
// one -- a full disk, a stick pulled out mid-write, or a quota limit
// all produce exactly that, and the operator would only find out when
// the contest robot rejected the entry. Every byte is accounted for
// here: the short write, the failed flush and the failed close each
// fail the call and fill `errorOut` with QFile's own message.
//
// Not atomic on purpose (no write-to-temp-and-rename): these are
// operator-chosen export targets, and replacing a file the operator
// picked in a save dialog is exactly what they asked for.
bool writeFileChecked(const QString& path, const QByteArray& bytes, QString* errorOut = nullptr);

} // namespace Contestprogramm
