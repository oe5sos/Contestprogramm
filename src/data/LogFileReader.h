#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace Contestprogramm {

// Reads the two log files a VHF contester has lying around from earlier
// contests -- EDI (REG1TEST, what every IARU-R1/ÖVSV robot took) and
// ADIF (what every logger exports) -- into plain records. Used to seed
// the locator memory (ContestDatabase::imported_locators, N1MM+'s "Call
// History" idea) from old N1MM+/DXLog/UcxLog/Tucnak logs, and as the
// round-trip check of this program's own EdiExporter. Pure parsing, no
// file or database access.
struct ImportedQso {
    QString callsign;
    QString grid;         // may be empty
    QString band;         // "144"/"432"/... or empty when unknown
    QString mode;         // "SSB"/"CW"/"FM"/... or empty
    QString timestampUtc; // ISO-8601, empty when the line had no usable date
};

class LogFileReader {
public:
    enum class Format { Unknown, Edi, Adif };

    // Sniffs the content ("[REG1TEST" / "<EOR>"), then the file name.
    static Format detect(const QByteArray& data, const QString& fileName = QString());

    static QVector<ImportedQso> parse(const QByteArray& data, const QString& fileName = QString());
    static QVector<ImportedQso> parseEdi(const QByteArray& data);
    static QVector<ImportedQso> parseAdif(const QByteArray& data);

    // "144 MHz" -> "144", "1,3 GHz" -> "1296"; ADIF "2m"/"70cm"/"23cm" too.
    static QString bandFromLabel(const QString& label);
};

} // namespace Contestprogramm
