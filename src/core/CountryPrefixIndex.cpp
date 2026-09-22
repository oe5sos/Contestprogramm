#include "core/CountryPrefixIndex.h"

#include <QRegularExpression>
#include <QStringList>

namespace Contestprogramm {

namespace {

// Betriebszusätze, die am Land nichts ändern -- dieselbe Liste wie in
// core/CallsignPrefix.cpp, hier aber mit anderer Folge: dort bestimmen
// sie den Präfix, hier das Land.
bool isOperatingSuffix(const QString& part)
{
    static const QStringList kSuffixes{
        QStringLiteral("P"),  QStringLiteral("M"),   QStringLiteral("MM"), QStringLiteral("AM"),
        QStringLiteral("A"),  QStringLiteral("QRP"), QStringLiteral("LH"), QStringLiteral("LGT"),
        QStringLiteral("R"),  QStringLiteral("B"),   QStringLiteral("J"),  QStringLiteral("QRPP"),
    };
    return kSuffixes.contains(part);
}

// "47.33" bzw. "-13.33" -- cty.dat führt die Länge nach WESTEN positiv.
double toEastPositive(const QString& field)
{
    return -field.trimmed().toDouble();
}

// Zieht die Abweichungen aus einem Präfix heraus und rechnet sie in
// `entry` ein; zurück kommt der Präfix ohne die Klammern.
QString applyAliasOverrides(const QString& alias, CountryEntry& entry)
{
    QString rest = alias;
    static const QRegularExpression pattern(
        QStringLiteral("\\(([^)]*)\\)|\\[([^\\]]*)\\]|<([^>]*)>|\\{([^}]*)\\}|~([^~]*)~"));
    QRegularExpressionMatchIterator it = pattern.globalMatch(alias);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (!match.captured(1).isNull()) {
            entry.cqZone = match.captured(1).toInt();
        } else if (!match.captured(2).isNull()) {
            entry.ituZone = match.captured(2).toInt();
        } else if (!match.captured(3).isNull()) {
            // "<lat/lon>", beide wie in der Kopfzeile: Länge nach Westen
            // positiv.
            const QStringList pair = match.captured(3).split(QLatin1Char('/'));
            if (pair.size() == 2) {
                entry.latitudeDeg = pair.at(0).trimmed().toDouble();
                entry.longitudeDeg = toEastPositive(pair.at(1));
            }
        } else if (!match.captured(4).isNull()) {
            entry.continent = match.captured(4).trimmed().toUpper();
        } else if (!match.captured(5).isNull()) {
            entry.utcOffsetHours = toEastPositive(match.captured(5));
        }
    }
    rest.remove(pattern);
    return rest.trimmed();
}

} // namespace

QString CountryPrefixIndex::lookupKey(const QString& callsign)
{
    QStringList parts;
    for (const QString& raw : callsign.trimmed().toUpper().split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        const QString part = raw.trimmed();
        if (!part.isEmpty()) {
            parts.append(part);
        }
    }
    while (parts.size() > 1 && isOperatingSuffix(parts.last())) {
        parts.removeLast();
    }
    if (parts.isEmpty()) {
        return QString();
    }
    if (parts.size() == 1) {
        return parts.first();
    }
    const QString first = parts.at(0);
    const QString second = parts.at(1);
    // Eine einzelne Ziffer verschiebt nur den Rufzeichenbezirk, nicht
    // das Land: W1AW/4 bleibt USA.
    if (second.size() == 1 && second.at(0).isDigit()) {
        return first;
    }
    if (first.size() == 1 && first.at(0).isDigit()) {
        return second;
    }
    // Sonst zählt der Zusatz -- der kürzere Teil, bei Gleichstand der
    // erste: DL/OE5SOS und OE5SOS/DL sind beide Deutschland.
    return second.size() < first.size() ? second : first;
}

bool CountryPrefixIndex::loadFromCty(const QByteArray& text, QString* errorOut)
{
    QVector<CountryEntry> entries;
    QHash<QString, int> byPrefix;
    QHash<QString, int> byExactCall;
    int countryCount = 0;

    // Zeilenenden vereinheitlichen, Kommentare (#) weg, dann an den
    // Semikola in Aufzeichnungen schneiden.
    QString content = QString::fromUtf8(text);
    content.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    static const QRegularExpression comment(QStringLiteral("(?m)^\\s*#.*$"));
    content.remove(comment);

    const QStringList records = content.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& rawRecord : records) {
        const QString record = rawRecord.trimmed();
        if (record.isEmpty()) {
            continue;
        }
        const int headerEnd = record.indexOf(QLatin1Char('\n'));
        const QString header = headerEnd < 0 ? record : record.left(headerEnd);
        const QString aliasBlock = headerEnd < 0 ? QString() : record.mid(headerEnd + 1);

        const QStringList fields = header.split(QLatin1Char(':'));
        if (fields.size() < 8) {
            continue; // keine Kopfzeile -- überlesen statt raten
        }
        CountryEntry entry;
        entry.name = fields.at(0).trimmed();
        entry.cqZone = fields.at(1).trimmed().toInt();
        entry.ituZone = fields.at(2).trimmed().toInt();
        entry.continent = fields.at(3).trimmed().toUpper();
        entry.latitudeDeg = fields.at(4).trimmed().toDouble();
        entry.longitudeDeg = toEastPositive(fields.at(5));
        entry.utcOffsetHours = toEastPositive(fields.at(6));
        QString primary = fields.at(7).trimmed().toUpper();
        // Ein '*' davor heißt "kein eigenes DXCC-Gebiet" (etwa der
        // europäische Teil der Türkei) -- für die Zuordnung egal.
        if (primary.startsWith(QLatin1Char('*'))) {
            primary.remove(0, 1);
        }
        entry.primaryPrefix = primary;
        if (entry.primaryPrefix.isEmpty()) {
            continue;
        }

        entries.append(entry);
        ++countryCount;
        const int index = entries.size() - 1;

        const QStringList aliases = aliasBlock.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString& rawAlias : aliases) {
            QString alias = rawAlias.simplified().toUpper();
            if (alias.isEmpty()) {
                continue;
            }
            // Die Abweichungen in Klammern gehören zu DIESEM Präfix und
            // werden eingerechnet -- sonst läge jedes US-Rufzeichen in
            // Zone 5 und am selben Punkt.
            CountryEntry aliasEntry = entry;
            alias = applyAliasOverrides(alias, aliasEntry);
            if (alias.isEmpty()) {
                continue;
            }
            int aliasIndex = index;
            if (aliasEntry.cqZone != entry.cqZone || aliasEntry.ituZone != entry.ituZone
                || !qFuzzyCompare(aliasEntry.latitudeDeg + 1000.0, entry.latitudeDeg + 1000.0)
                || !qFuzzyCompare(aliasEntry.longitudeDeg + 1000.0, entry.longitudeDeg + 1000.0)
                || aliasEntry.continent != entry.continent
                || !qFuzzyCompare(aliasEntry.utcOffsetHours + 1000.0, entry.utcOffsetHours + 1000.0)) {
                entries.append(aliasEntry);
                aliasIndex = entries.size() - 1;
            }
            if (alias.startsWith(QLatin1Char('='))) {
                byExactCall.insert(alias.mid(1), aliasIndex);
            } else {
                byPrefix.insert(alias, aliasIndex);
            }
        }
        // Der Hauptpräfix zählt immer mit, auch wenn er in der
        // Präfixliste fehlt.
        if (!byPrefix.contains(entry.primaryPrefix)) {
            byPrefix.insert(entry.primaryPrefix, index);
        }
    }

    if (entries.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("In dieser Datei steht kein Land im cty.dat-Format.");
        }
        return false;
    }

    m_entries = entries;
    m_countryCount = countryCount;
    m_byPrefix = byPrefix;
    m_byExactCall = byExactCall;
    if (errorOut) {
        errorOut->clear();
    }
    return true;
}

CountryEntry CountryPrefixIndex::lookup(const QString& callsign) const
{
    const QString key = lookupKey(callsign);
    if (key.isEmpty() || m_entries.isEmpty()) {
        return CountryEntry();
    }
    // Ein einzeln eingetragenes Rufzeichen schlägt jeden Präfix -- genau
    // dafür stehen die "="-Einträge in der Datei.
    if (const auto it = m_byExactCall.constFind(key); it != m_byExactCall.constEnd()) {
        return m_entries.at(it.value());
    }
    // Sonst der längste passende Präfix: OE5 vor OE, sonst landete jedes
    // Rufzeichen im erstbesten Land, dessen Anfangsbuchstabe passt.
    for (int length = key.size(); length > 0; --length) {
        const auto it = m_byPrefix.constFind(key.left(length));
        if (it != m_byPrefix.constEnd()) {
            return m_entries.at(it.value());
        }
    }
    return CountryEntry();
}

} // namespace Contestprogramm
