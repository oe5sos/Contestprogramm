#include "core/ClockCheck.h"

#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>

namespace Contestprogramm {

namespace {
// Each answer is trusted to the second; the round trip is short on any
// link where the check matters, so half of it is not corrected for.
constexpr int kTimeoutMs = 6000;
} // namespace

ClockCheck::ClockCheck(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_urls({QStringLiteral("https://www.google.com/"), QStringLiteral("https://www.apple.com/"),
              QStringLiteral("https://www.on4kst.com/")})
{
    m_network->setTransferTimeout(kTimeoutMs);
}

void ClockCheck::setUrls(const QStringList& urls)
{
    m_urls = urls;
}

void ClockCheck::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    m_next = 0;
    askNext();
}

void ClockCheck::askNext()
{
    if (m_next >= m_urls.size()) {
        m_running = false;
        m_reachable = false;
        m_offsetSecs = 0;
        m_source.clear();
        m_checkedAt = QDateTime::currentDateTimeUtc();
        emit finished(false, 0, QString());
        return;
    }
    const QUrl url(m_urls.at(m_next++));
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Contestprogramm clock check"));
    const QDateTime sentAt = QDateTime::currentDateTimeUtc();
    QNetworkReply* reply = m_network->head(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, sentAt] { handleReply(reply, sentAt); });
}

void ClockCheck::handleReply(QNetworkReply* reply, const QDateTime& sentAtUtc)
{
    reply->deleteLater();
    // Any answer with a Date header will do -- a 4xx/5xx status still
    // comes from a server with a clock.
    const QDateTime serverTime = parseHttpDate(QString::fromLatin1(reply->rawHeader("Date")));
    if (!serverTime.isValid()) {
        askNext();
        return;
    }
    const QDateTime receivedAt = QDateTime::currentDateTimeUtc();
    // The server stamped the reply somewhere inside the round trip; its
    // midpoint is the best local counterpart.
    const qint64 midpointMs = sentAtUtc.toMSecsSinceEpoch() + sentAtUtc.msecsTo(receivedAt) / 2;
    m_running = false;
    m_reachable = true;
    m_offsetSecs = qRound64((midpointMs - serverTime.toMSecsSinceEpoch()) / 1000.0);
    m_source = reply->url().host();
    m_checkedAt = receivedAt;
    emit finished(true, m_offsetSecs, m_source);
}

QDateTime ClockCheck::parseHttpDate(const QString& text)
{
    // RFC 7231 IMF-fixdate: "Sat, 03 Oct 2026 14:00:00 GMT" -- always
    // English names and always GMT, so the C locale parses it; Qt's own
    // RFC 2822 parser does not take the "GMT" zone word.
    const QString trimmed = text.trimmed();
    for (const char* format : {"ddd, dd MMM yyyy HH:mm:ss 'GMT'", "dd MMM yyyy HH:mm:ss 'GMT'", "ddd, d MMM yyyy HH:mm:ss 'GMT'"}) {
        QDateTime parsed = QLocale::c().toDateTime(trimmed, QString::fromLatin1(format));
        if (parsed.isValid()) {
            parsed.setTimeZone(QTimeZone::utc());
            return parsed;
        }
    }
    const QDateTime rfc2822 = QDateTime::fromString(trimmed, Qt::RFC2822Date);
    return rfc2822.isValid() ? rfc2822.toUTC() : QDateTime();
}

} // namespace Contestprogramm
