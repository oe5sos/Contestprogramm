#include "core/OnlineScoreboard.h"

#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QXmlStreamWriter>

namespace Contestprogramm {

namespace {

// Contest Online Score mode codes: CW, PH, DIG (as N1MM+ reports them).
QString scoreboardMode(const QString& mode)
{
    const QString m = mode.trimmed().toUpper();
    if (m == QStringLiteral("CW")) { return QStringLiteral("CW"); }
    if (m == QStringLiteral("RTTY") || m == QStringLiteral("FT8") || m == QStringLiteral("FT4")
        || m == QStringLiteral("DIGITAL") || m == QStringLiteral("DATA") || m == QStringLiteral("DIG")) {
        return QStringLiteral("DIG");
    }
    return QStringLiteral("PH"); // SSB/FM/AM and anything voice-like
}

} // namespace

OnlineScoreboard::OnlineScoreboard(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_timer(new QTimer(this))
{
    connect(m_network, &QNetworkAccessManager::finished, this, &OnlineScoreboard::handleReply);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        QString error;
        if (!postNow(&error) && !error.isEmpty()) {
            emit failed(error);
        }
    });
}

void OnlineScoreboard::setConfig(const ScoreboardConfig& config)
{
    m_config = config;
    m_timer->stop();
    if (m_config.enabled && m_config.url.isValid() && !m_config.url.isEmpty()) {
        m_timer->start(std::max(1, m_config.intervalMinutes) * 60 * 1000);
    }
}

void OnlineScoreboard::setRecords(const QVector<QsoRecord>& records)
{
    m_records = records;
}

QByteArray OnlineScoreboard::buildXml(const QVector<QsoRecord>& records, const ScoreboardConfig& config,
                                      const QDateTime& stampUtc)
{
    const ContestScore score = computeContestScore(records, config.grid, config.bandOrder, config.scoring);

    // QSOs per band and mode (valid ones only), in band order.
    QMap<QString, QMap<QString, int>> qsosByBandMode;
    for (const QsoRecord& r : records) {
        if (r.isInvalid || r.isDupe) {
            continue;
        }
        qsosByBandMode[r.band][scoreboardMode(r.mode)] += 1;
    }

    QByteArray out;
    QXmlStreamWriter xml(&out);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("dynamicresults"));
    xml.writeTextElement(QStringLiteral("contest"), config.contestName);
    xml.writeTextElement(QStringLiteral("call"), config.callsign.trimmed().toUpper());
    xml.writeTextElement(QStringLiteral("ops"), config.callsign.trimmed().toUpper());
    xml.writeStartElement(QStringLiteral("class"));
    xml.writeAttribute(QStringLiteral("ops"), QStringLiteral("SINGLE-OP"));
    xml.writeAttribute(QStringLiteral("transmitter"), QStringLiteral("ONE"));
    xml.writeAttribute(QStringLiteral("bands"), score.bands.size() == 1 ? score.bands.first().band : QStringLiteral("ALL"));
    xml.writeAttribute(QStringLiteral("mode"), QStringLiteral("MIXED"));
    xml.writeEndElement();
    if (!config.club.trimmed().isEmpty()) {
        xml.writeTextElement(QStringLiteral("club"), config.club.trimmed());
    }
    xml.writeStartElement(QStringLiteral("qth"));
    xml.writeTextElement(QStringLiteral("grid6"), config.grid.trimmed().toUpper());
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("breakdown"));
    for (const BandScore& band : score.bands) {
        const QMap<QString, int> byMode = qsosByBandMode.value(band.band);
        for (auto it = byMode.constBegin(); it != byMode.constEnd(); ++it) {
            xml.writeStartElement(QStringLiteral("qso"));
            xml.writeAttribute(QStringLiteral("band"), band.band);
            xml.writeAttribute(QStringLiteral("mode"), it.key());
            xml.writeCharacters(QString::number(it.value()));
            xml.writeEndElement();
        }
        xml.writeStartElement(QStringLiteral("point"));
        xml.writeAttribute(QStringLiteral("band"), band.band);
        xml.writeAttribute(QStringLiteral("mode"), QStringLiteral("ALL"));
        xml.writeCharacters(QString::number(band.points));
        xml.writeEndElement();
    }
    xml.writeStartElement(QStringLiteral("qso"));
    xml.writeAttribute(QStringLiteral("band"), QStringLiteral("total"));
    xml.writeAttribute(QStringLiteral("mode"), QStringLiteral("ALL"));
    xml.writeCharacters(QString::number(score.validQsos));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("point"));
    xml.writeAttribute(QStringLiteral("band"), QStringLiteral("total"));
    xml.writeAttribute(QStringLiteral("mode"), QStringLiteral("ALL"));
    xml.writeCharacters(QString::number(score.points));
    xml.writeEndElement();
    xml.writeEndElement(); // breakdown

    xml.writeTextElement(QStringLiteral("score"), QString::number(score.points));
    xml.writeTextElement(QStringLiteral("timestamp"), stampUtc.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    xml.writeEndElement(); // dynamicresults
    xml.writeEndDocument();
    return out;
}

bool OnlineScoreboard::postNow(QString* errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }
    if (!m_config.enabled) {
        return false;
    }
    if (!m_config.url.isValid() || m_config.url.isEmpty() || m_config.callsign.trimmed().isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Scoreboard: URL oder eigenes Rufzeichen fehlt");
        }
        return false;
    }
    QNetworkRequest request(m_config.url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/xml; charset=utf-8"));
    if (!m_config.username.isEmpty()) {
        const QByteArray credentials = (m_config.username + QLatin1Char(':') + m_config.password).toUtf8().toBase64();
        request.setRawHeader("Authorization", "Basic " + credentials);
    }
    m_network->post(request, buildXml(m_records, m_config, QDateTime::currentDateTimeUtc()));
    return true;
}

void OnlineScoreboard::handleReply(QNetworkReply* reply)
{
    reply->deleteLater();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError) {
        emit failed(QStringLiteral("Scoreboard: %1 (HTTP %2)").arg(reply->errorString()).arg(status));
        return;
    }
    emit posted(QStringLiteral("Scoreboard aktualisiert (HTTP %1)").arg(status));
}

} // namespace Contestprogramm
