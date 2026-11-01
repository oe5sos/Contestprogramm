#include "ui/SideAreaIcons.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace Contestprogramm {

namespace {

// Panel-Kennung -> Symbolart. Getrennt gehalten, weil mehrere
// Kennungen dieselbe Zeichnung tragen könnten und eine unbekannte
// still durchfällt.
QString artFuer(const QString& panelId)
{
    if (panelId == QLatin1String("unifiedlog")) {
        return QStringLiteral("log");
    }
    if (panelId == QLatin1String("rotorrow")) {
        return QStringLiteral("rotor");
    }
    if (panelId == QLatin1String("map")) {
        return QStringLiteral("karte");
    }
    if (panelId == QLatin1String("suggestion")) {
        return QStringLiteral("ziel");
    }
    if (panelId == QLatin1String("ratemeter")) {
        return QStringLiteral("rate");
    }
    if (panelId == QLatin1String("checkpartial")) {
        return QStringLiteral("check");
    }
    if (panelId == QLatin1String("bandmap")) {
        return QStringLiteral("bandmap");
    }
    if (panelId == QLatin1String("skeds")) {
        return QStringLiteral("sked");
    }
    if (panelId == QLatin1String("chat")) {
        return QStringLiteral("chat");
    }
    if (panelId == QLatin1String("cwmacros")) {
        return QStringLiteral("log");
    }
    return QString();
}

// Zeichnet das Symbol in ein Quadrat der Kantenlänge s um (0, 0).
void zeichne(QPainter& p, const QString& art, double s, const QColor& farbe)
{
    const QPen stift(farbe, std::max(1.2, s / 11.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(stift);
    p.setBrush(Qt::NoBrush);
    const double h = s / 2.0;

    if (art == QLatin1String("log")) {
        // Zeilen -- das Log ist eine Liste.
        for (int i = -1; i <= 1; ++i) {
            const double y = i * s * 0.28;
            p.drawLine(QPointF(-h * 0.85, y), QPointF(h * 0.85, y));
        }
    } else if (art == QLatin1String("rotor")) {
        // Kompass mit Nadel. Die Nadel ist ein Strich mit Spitze: ein
        // gefülltes Dreieck in Kreisgröße sah bei 17 px aus wie ein
        // Halbmond.
        p.drawEllipse(QPointF(0, 0), h * 0.85, h * 0.85);
        p.save();
        p.rotate(35);
        p.drawLine(QPointF(0, h * 0.45), QPointF(0, -h * 0.7));
        p.setBrush(farbe);
        p.setPen(Qt::NoPen);
        QPainterPath spitze;
        spitze.moveTo(0, -h * 0.85);
        spitze.lineTo(h * 0.22, -h * 0.42);
        spitze.lineTo(-h * 0.22, -h * 0.42);
        spitze.closeSubpath();
        p.drawPath(spitze);
        p.restore();
    } else if (art == QLatin1String("karte")) {
        // Rundsicht mit Kegel und Standort -- genau das, was das Panel
        // zeigt.
        const QPointF standort(0, h * 0.55);
        p.setBrush(QColor(farbe.red(), farbe.green(), farbe.blue(), 120));
        p.setPen(Qt::NoPen);
        QPainterPath kegel;
        kegel.moveTo(standort);
        kegel.arcTo(QRectF(standort.x() - s * 0.8, standort.y() - s * 0.8, s * 1.6, s * 1.6), 62, 56);
        kegel.closeSubpath();
        p.drawPath(kegel);
        p.setBrush(Qt::NoBrush);
        p.setPen(stift);
        p.drawArc(QRectF(standort.x() - s * 0.8, standort.y() - s * 0.8, s * 1.6, s * 1.6), 20 * 16, 140 * 16);
        p.drawArc(QRectF(standort.x() - s * 0.42, standort.y() - s * 0.42, s * 0.84, s * 0.84), 20 * 16, 140 * 16);
        p.setBrush(farbe);
        p.setPen(Qt::NoPen);
        p.drawEllipse(standort, s * 0.07, s * 0.07);
    } else if (art == QLatin1String("ziel")) {
        // Peilung auf einen Punkt.
        p.drawEllipse(QPointF(h * 0.45, -h * 0.45), h * 0.22, h * 0.22);
        p.drawLine(QPointF(-h * 0.75, h * 0.75), QPointF(h * 0.2, -h * 0.2));
        p.setBrush(farbe);
        p.setPen(Qt::NoPen);
        QPainterPath spitze;
        spitze.moveTo(h * 0.3, -h * 0.3);
        spitze.lineTo(h * 0.05, -h * 0.34);
        spitze.lineTo(h * 0.34, -h * 0.05);
        spitze.closeSubpath();
        p.drawPath(spitze);
    } else if (art == QLatin1String("rate")) {
        p.setBrush(farbe);
        p.setPen(Qt::NoPen);
        const double b = s * 0.17;
        const double hoehen[3] = {0.35, 0.65, 1.0};
        for (int i = 0; i < 3; ++i) {
            const double hh = s * 0.7 * hoehen[i];
            p.drawRect(QRectF(-h * 0.8 + i * (b * 1.7), h * 0.75 - hh, b, hh));
        }
    } else if (art == QLatin1String("check")) {
        // Lupe -- Check Partial sucht im Bestand.
        p.drawEllipse(QPointF(-h * 0.15, -h * 0.15), h * 0.55, h * 0.55);
        p.drawLine(QPointF(h * 0.25, h * 0.25), QPointF(h * 0.8, h * 0.8));
    } else if (art == QLatin1String("bandmap")) {
        p.drawLine(QPointF(-h * 0.45, -h * 0.9), QPointF(-h * 0.45, h * 0.9));
        for (int i = -2; i <= 2; ++i) {
            const double y = i * s * 0.2;
            const double laenge = (i % 2 == 0) ? 0.85 : 0.5;
            p.drawLine(QPointF(-h * 0.45, y), QPointF(-h * 0.45 + s * 0.5 * laenge, y));
        }
    } else if (art == QLatin1String("sked")) {
        // Uhr -- eine Verabredung hat eine Zeit.
        p.drawEllipse(QPointF(0, 0), h * 0.85, h * 0.85);
        p.drawLine(QPointF(0, 0), QPointF(0, -h * 0.55));
        p.drawLine(QPointF(0, 0), QPointF(h * 0.4, h * 0.15));
    } else if (art == QLatin1String("chat")) {
        QPainterPath blase;
        blase.addRoundedRect(QRectF(-h * 0.85, -h * 0.75, s * 0.85, s * 0.6), s * 0.14, s * 0.14);
        blase.moveTo(-h * 0.35, h * 0.1);
        blase.lineTo(-h * 0.5, h * 0.75);
        blase.lineTo(-h * 0.02, h * 0.1);
        p.drawPath(blase);
    }
}

} // namespace

QPixmap sideAreaIconPixmap(const QString& panelId, int kantenlaengePx, const QColor& farbe, qreal geraeteFaktor)
{
    const QString art = artFuer(panelId);
    if (art.isEmpty() || kantenlaengePx <= 0) {
        return QPixmap();
    }
    const qreal faktor = geraeteFaktor > 0.0 ? geraeteFaktor : 1.0;
    QPixmap bild(int(kantenlaengePx * faktor), int(kantenlaengePx * faktor));
    bild.setDevicePixelRatio(faktor);
    bild.fill(Qt::transparent);
    QPainter p(&bild);
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(kantenlaengePx / 2.0, kantenlaengePx / 2.0);
    // Ein Rand von rund 10 %, damit runde Symbole nicht am Bildrand
    // abgeschnitten werden.
    zeichne(p, art, kantenlaengePx * 0.82, farbe);
    p.end();
    return bild;
}

} // namespace Contestprogramm
