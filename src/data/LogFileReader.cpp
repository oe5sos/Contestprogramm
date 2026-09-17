#include "data/LogFileReader.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QTimeZone>

namespace Contestprogramm {

namespace {

// REG1TEST mode codes (see EdiExporter::modeCode).
QString modeFromEdiCode(int code)
{
    switch (code) {
    case 1: return QStringLiteral("SSB");
    case 2: return QStringLiteral("CW");
    case 3: return QStringLiteral("SSB");
    case 4: return QStringLiteral("CW");
    case 5: return QStringLiteral("AM");
    case 6: return QStringLiteral("FM");
    case 7: return QStringLiteral("RTTY");
    case 8: return QStringLiteral("SSTV");
    case 9: return QStringLiteral("ATV");
    default: return QString();
    }
}

QString isoFrom(const QDate& date, const QTime& time)
{
    if (!date.isValid()) {
        return QString();
    }
    // QTimeZone::UTC (the enum), not utc(): the former formats as the
    // "...Z" the rest of this program writes, the latter as "+00:00".
    return QDateTime(date, time.isValid() ? time : QTime(0, 0), QTimeZone::UTC).toString(Qt::ISODate);
}

// EDI dates are YYMMDD; anything before 1980 is not a contest log.
QDate ediDate(const QString& yymmdd)
{
    if (yymmdd.size() != 6) {
        return QDate();
    }
    QDate d = QDate::fromString(yymmdd, QStringLiteral("yyMMdd"));
    if (d.isValid() && d.year() < 1980) {
        d = d.addYears(100);
    }
    return d;
}

} // namespace

LogFileReader::Format LogFileReader::detect(const QByteArray& data, const QString& fileName)
{
    const QByteArray head = data.left(4096);
    if (head.contains("[REG1TEST")) {
        return Format::Edi;
    }
    const QByteArray lower = data.toLower();
    if (lower.contains("<eor>") || lower.contains("<eoh>")) {
        return Format::Adif;
    }
    const QString name = fileName.toLower();
    if (name.endsWith(QStringLiteral(".edi"))) {
        return Format::Edi;
    }
    if (name.endsWith(QStringLiteral(".adi")) || name.endsWith(QStringLiteral(".adif"))) {
        return Format::Adif;
    }
    return Format::Unknown;
}

QVector<ImportedQso> LogFileReader::parse(const QByteArray& data, const QString& fileName)
{
    switch (detect(data, fileName)) {
    case Format::Edi: return parseEdi(data);
    case Format::Adif: return parseAdif(data);
    case Format::Unknown: break;
    }
    return {};
}

QString LogFileReader::bandFromLabel(const QString& label)
{
    const QString l = label.trimmed().toLower();
    struct Entry {
        const char* label;
        const char* band;
    };
    static const Entry kEntries[] = {
        {"50 mhz", "50"},   {"6m", "50"},      {"70 mhz", "70"},   {"4m", "70"},
        {"144 mhz", "144"}, {"2m", "144"},     {"432 mhz", "432"}, {"435 mhz", "432"}, {"70cm", "432"},
        {"1,3 ghz", "1296"}, {"1.3 ghz", "1296"}, {"23cm", "1296"},
        {"2,3 ghz", "2320"}, {"2.3 ghz", "2320"}, {"13cm", "2320"},
        {"3,4 ghz", "3400"}, {"3.4 ghz", "3400"}, {"9cm", "3400"},
        {"5,7 ghz", "5760"}, {"5.7 ghz", "5760"}, {"6cm", "5760"},
        {"10 ghz", "10368"}, {"3cm", "10368"},
        {"24 ghz", "24048"}, {"1.25cm", "24048"},
    };
    for (const Entry& e : kEntries) {
        if (l == QLatin1String(e.label)) {
            return QLatin1String(e.band);
        }
    }
    // A bare number ("144") passes through.
    bool ok = false;
    l.toInt(&ok);
    return ok ? l : QString();
}

QVector<ImportedQso> LogFileReader::parseEdi(const QByteArray& data)
{
    QVector<ImportedQso> result;
    QString band;
    bool inRecords = false;
    // Latin-1 is the format's own encoding; UTF-8 files decode the ASCII
    // fields identically, which is all a QSO line carries.
    const QString text = QString::fromLatin1(data);
    for (const QString& rawLine : text.split(QLatin1Char('\n'))) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QLatin1Char('['))) {
            inRecords = line.startsWith(QStringLiteral("[QSORecords"), Qt::CaseInsensitive);
            continue;
        }
        if (!inRecords) {
            if (line.startsWith(QStringLiteral("PBand="), Qt::CaseInsensitive)) {
                band = bandFromLabel(line.mid(6));
            }
            continue;
        }
        const QStringList f = line.split(QLatin1Char(';'));
        if (f.size() < 10) {
            continue;
        }
        ImportedQso q;
        q.callsign = f.at(2).trimmed().toUpper();
        if (q.callsign.isEmpty()) {
            continue;
        }
        q.mode = modeFromEdiCode(f.at(3).trimmed().toInt());
        q.grid = f.at(9).trimmed().toUpper();
        q.band = band;
        q.timestampUtc = isoFrom(ediDate(f.at(0).trimmed()), QTime::fromString(f.at(1).trimmed(), QStringLiteral("HHmm")));
        result.append(q);
    }
    return result;
}

QVector<ImportedQso> LogFileReader::parseAdif(const QByteArray& data)
{
    QVector<ImportedQso> result;
    QString text = QString::fromUtf8(data);
    // Skip the header: everything up to <EOH>, when there is one.
    const int eoh = text.indexOf(QStringLiteral("<eoh>"), 0, Qt::CaseInsensitive);
    if (eoh >= 0) {
        text = text.mid(eoh + 5);
    }
    static const QRegularExpression tag(QStringLiteral("<([A-Za-z_]+)(?::(\\d+))?(?::[A-Za-z])?>"));
    ImportedQso current;
    QString qsoDate;
    QString timeOn;
    bool any = false;
    int pos = 0;
    while (true) {
        const QRegularExpressionMatch m = tag.match(text, pos);
        if (!m.hasMatch()) {
            break;
        }
        const QString name = m.captured(1).toUpper();
        const int length = m.captured(2).isEmpty() ? 0 : m.captured(2).toInt();
        const int valueStart = m.capturedEnd(0);
        pos = valueStart;
        if (name == QStringLiteral("EOR")) {
            if (any && !current.callsign.isEmpty()) {
                current.timestampUtc = isoFrom(QDate::fromString(qsoDate, QStringLiteral("yyyyMMdd")),
                                               timeOn.size() >= 6 ? QTime::fromString(timeOn.left(6), QStringLiteral("HHmmss"))
                                                                  : QTime::fromString(timeOn.left(4), QStringLiteral("HHmm")));
                result.append(current);
            }
            current = ImportedQso();
            qsoDate.clear();
            timeOn.clear();
            any = false;
            continue;
        }
        const QString value = text.mid(valueStart, length).trimmed();
        pos = valueStart + length;
        any = true;
        if (name == QStringLiteral("CALL")) {
            current.callsign = value.toUpper();
        } else if (name == QStringLiteral("GRIDSQUARE")) {
            current.grid = value.toUpper();
        } else if (name == QStringLiteral("BAND")) {
            current.band = bandFromLabel(value);
        } else if (name == QStringLiteral("MODE")) {
            current.mode = value.toUpper();
        } else if (name == QStringLiteral("QSO_DATE")) {
            qsoDate = value;
        } else if (name == QStringLiteral("TIME_ON")) {
            timeOn = value;
        }
    }
    return result;
}

} // namespace Contestprogramm
