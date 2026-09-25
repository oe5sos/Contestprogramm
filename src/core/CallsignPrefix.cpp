#include "core/CallsignPrefix.h"

#include <QSet>
#include <QStringList>

namespace Contestprogramm {

namespace {

// Betriebszusätze, die für den Präfix nicht zählen (WPX-Regeln:
// "/MM, /AM, /P, /M, /QRP ... are ignored"). Nur gegen den TEIL HINTER
// dem Schrägstrich geprüft, nie gegen das Rufzeichen selbst.
bool isOperatingSuffix(const QString& part)
{
    static const QSet<QString> kSuffixes{
        QStringLiteral("P"),  QStringLiteral("M"),   QStringLiteral("MM"), QStringLiteral("AM"),
        QStringLiteral("A"),  QStringLiteral("QRP"), QStringLiteral("LH"), QStringLiteral("LGT"),
        QStringLiteral("R"),  QStringLiteral("B"),   QStringLiteral("J"),  QStringLiteral("QRPP"),
    };
    return kSuffixes.contains(part);
}

int lastDigitIndex(const QString& text)
{
    for (int i = text.size() - 1; i >= 0; --i) {
        if (text.at(i).isDigit()) {
            return i;
        }
    }
    return -1;
}

// Der Präfix eines einzelnen Rufzeichenteils: bis einschließlich der
// letzten Ziffer, und ohne Ziffer die ersten zwei Zeichen plus 0.
QString prefixOfPart(const QString& part)
{
    if (part.isEmpty()) {
        return QString();
    }
    const int digit = lastDigitIndex(part);
    if (digit >= 0) {
        return part.left(digit + 1);
    }
    return part.left(2) + QLatin1Char('0');
}

} // namespace

QString wpxPrefix(const QString& callsign)
{
    QStringList parts;
    for (const QString& raw : callsign.trimmed().toUpper().split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        const QString part = raw.trimmed();
        if (!part.isEmpty()) {
            parts.append(part);
        }
    }
    // Betriebszusätze weg -- aber nie den letzten verbliebenen Teil,
    // sonst bliebe von "P/OE5SOS" nichts übrig (dort ist "P" der erste
    // Teil und damit kein Betriebszusatz).
    while (parts.size() > 1 && isOperatingSuffix(parts.last())) {
        parts.removeLast();
    }
    if (parts.isEmpty()) {
        return QString();
    }
    if (parts.size() == 1) {
        return prefixOfPart(parts.first());
    }

    // Mehr als zwei Teile kommen vor (DL/OE5SOS/P) -- die Zusätze sind
    // oben schon weg, den Rest über die ersten beiden entscheiden.
    const QString first = parts.at(0);
    const QString second = parts.at(1);

    // Ein Zusatz aus einer einzigen Ziffer ersetzt die Ziffer im
    // Präfix des Rufzeichens: N8BJQ/9 -> N9.
    const auto replaceDigit = [](const QString& call, const QChar& digit) {
        const QString prefix = prefixOfPart(call);
        const int at = lastDigitIndex(prefix);
        if (at < 0) {
            return prefix + digit;
        }
        QString replaced = prefix;
        replaced[at] = digit;
        return replaced;
    };
    if (second.size() == 1 && second.at(0).isDigit()) {
        return replaceDigit(first, second.at(0));
    }
    if (first.size() == 1 && first.at(0).isDigit()) {
        return replaceDigit(second, first.at(0));
    }

    // Sonst zählt der Zusatz -- der kürzere Teil, bei Gleichstand der
    // erste (KH6/N8BJQ und N8BJQ/KH6 ergeben beide KH6).
    const QString designator = second.size() < first.size() ? second : first;
    return prefixOfPart(designator);
}

QString baseCallsign(const QString& callsign)
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
    QString best;
    for (const QString& part : parts) {
        if (part.size() > best.size()) {
            best = part;
        }
    }
    return best;
}

} // namespace Contestprogramm
