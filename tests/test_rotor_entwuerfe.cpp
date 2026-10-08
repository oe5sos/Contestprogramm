// Entwurfsblätter für die Frage vom 2026-10-08: "vielleicht kann man
// als option 1-4 rotoren rechts neben dem hauptrotor einblenden. sprich
// übereinander" -- und danach: "die rotoren müssen natürlich kleiner
// sein", "vielleicht kann bei 2 rotoren die hauptanzeige mehr nach
// links rutschen", "besser du machst ein paar vorschläge".
//
// Vier Anordnungen, jede einmal mit zwei und einmal mit vier Rotoren,
// in der wirklichen Panelgröße (618x284, das Maß des Rotoren-Panels in
// der Standardanordnung). Gezeichnet mit denselben Farben und
// Schriften wie das echte Widget, aber bewusst als Skizze: hier soll
// die Anordnung entschieden werden, nicht der letzte Pixel.
//
// Läuft nur mit CP_SHEET_DIR im Environment, sonst überspringt er sich
// selbst -- in der CI hat niemand etwas von Bildern.

#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QtMath>

#include "ui/StyleKit.h"

using namespace Contestprogramm;

namespace {

constexpr int kSheetW = 618;
constexpr int kSheetH = 284;

struct Rotor {
    QString band;
    double azimuthDeg;
    bool connected;
};

// Eine Kompassrose im Stil des echten Widgets: Ring, Teilstriche, die
// vier Himmelsrichtungen (nur wenn Platz ist), Nadel in Bernstein.
void rose(QPainter& p, const QRectF& feld, double azimuthDeg, bool connected, bool beschriftung)
{
    const QPointF m = feld.center();
    const double r = qMin(feld.width(), feld.height()) / 2.0 - 4.0;
    if (r < 8.0) {
        return;
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    QRadialGradient glow(m, r);
    glow.setColorAt(0.0, QColor(255, 176, 60, connected ? 34 : 14));
    glow.setColorAt(1.0, QColor(0, 0, 0, 0));
    p.setBrush(glow);
    p.setPen(Qt::NoPen);
    p.drawEllipse(m, r, r);

    p.setPen(QPen(QColor(Style::kTextScale()), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(m, r, r);

    const int schritt = r < 40 ? 30 : 10;
    for (int grad = 0; grad < 360; grad += schritt) {
        const double rad = qDegreesToRadians(double(grad) - 90.0);
        const bool haupt = (grad % 90) == 0;
        const double innen = r - (haupt ? 9.0 : 5.0);
        p.setPen(QPen(QColor(haupt ? Style::kTextPrimary() : Style::kAmberText()), haupt ? 1.6 : 0.9));
        p.drawLine(QPointF(m.x() + innen * qCos(rad), m.y() + innen * qSin(rad)),
                   QPointF(m.x() + r * qCos(rad), m.y() + r * qSin(rad)));
    }

    if (beschriftung && r >= 34) {
        p.setFont(Style::capsFont(p.font(), 9));
        p.setPen(QColor(Style::kTextTertiary()));
        const QStringList namen{QStringLiteral("N"), QStringLiteral("E"), QStringLiteral("S"), QStringLiteral("W")};
        for (int i = 0; i < 4; ++i) {
            const double rad = qDegreesToRadians(i * 90.0 - 90.0);
            const QPointF pos(m.x() + (r - 18) * qCos(rad), m.y() + (r - 18) * qSin(rad));
            p.drawText(QRectF(pos.x() - 8, pos.y() - 7, 16, 14), Qt::AlignCenter, namen.at(i));
        }
    }

    // Nadel
    const double rad = qDegreesToRadians(azimuthDeg - 90.0);
    const QColor warm(connected ? Style::kAmberText() : Style::kTextInactive());
    QPainterPath nadel;
    nadel.moveTo(m.x() + (r - 6) * qCos(rad), m.y() + (r - 6) * qSin(rad));
    nadel.lineTo(m.x() + 5 * qCos(rad + M_PI_2), m.y() + 5 * qSin(rad + M_PI_2));
    nadel.lineTo(m.x() + 5 * qCos(rad - M_PI_2), m.y() + 5 * qSin(rad - M_PI_2));
    nadel.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(warm);
    p.drawPath(nadel);
    p.restore();
}

void kopf(QPainter& p, const QRectF& r, const QString& titel, bool klein)
{
    p.save();
    QLinearGradient bg(r.topLeft(), r.bottomLeft());
    bg.setColorAt(0.0, QColor(255, 255, 255, 12));
    bg.setColorAt(1.0, QColor(0, 0, 0, 30));
    p.fillRect(r, bg);
    p.fillRect(QRectF(r.left(), r.top(), 3, r.height()), QColor(Style::kAmberText()));
    p.setPen(QColor(255, 255, 255, 22));
    p.drawLine(r.bottomLeft(), r.bottomRight());
    p.setFont(Style::capsFont(p.font(), klein ? 9 : 11));
    p.setPen(QColor(Style::kTextPrimary()));
    p.drawText(r.adjusted(10, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, titel);
    p.restore();
}

void rahmen(QPainter& p, const QRectF& r)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor(Style::kPanelBg()));
    p.setPen(QPen(QColor(255, 255, 255, 18), 1.0));
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
    p.restore();
}

void wert(QPainter& p, const QRectF& r, const QString& text, bool gross, bool warm)
{
    p.save();
    p.setFont(Style::monoFont(p.font(), gross ? 22 : 13, QFont::DemiBold));
    p.setPen(QColor(warm ? Style::kAmberText() : Style::kTextInactive()));
    p.drawText(r, Qt::AlignCenter, text);
    p.restore();
}

QString grad(const Rotor& r)
{
    return r.connected ? QStringLiteral("%1°").arg(QString::number(qRound(r.azimuthDeg)).rightJustified(3, QLatin1Char('0')))
                       : QStringLiteral("—");
}

// Der große Kompass links, in allen vier Entwürfen derselbe.
void hauptrotor(QPainter& p, const QRectF& feld, const Rotor& r, bool mitAblesung)
{
    rahmen(p, feld);
    kopf(p, QRectF(feld.left(), feld.top(), feld.width(), 26), r.band, false);
    const double ableseHoehe = mitAblesung ? 74 : 26;
    rose(p, QRectF(feld.left(), feld.top() + 26, feld.width(), feld.height() - 26 - ableseHoehe), r.azimuthDeg,
         r.connected, true);
    if (mitAblesung) {
        const QStringList titel{QStringLiteral("AKTUELL"), QStringLiteral("ZIEL"), QStringLiteral("ENTFERNUNG")};
        const QStringList werte{grad(r), QStringLiteral("—"), QStringLiteral("—")};
        const double breite = (feld.width() - 24) / 3.0;
        for (int i = 0; i < 3; ++i) {
            const QRectF zelle(feld.left() + 8 + i * (breite + 4), feld.bottom() - 68, breite, 52);
            p.save();
            p.setBrush(QColor(0, 0, 0, 70));
            p.setPen(QPen(QColor(255, 255, 255, 14), 1.0));
            p.drawRoundedRect(zelle, 6, 6);
            p.setFont(Style::capsFont(p.font(), 8));
            p.setPen(QColor(Style::kTextTertiary()));
            p.drawText(zelle.adjusted(0, 6, 0, 0), Qt::AlignHCenter | Qt::AlignTop, titel.at(i));
            p.restore();
            wert(p, zelle.adjusted(0, 16, 0, -4), werte.at(i), false, i == 0 && r.connected);
        }
    } else {
        wert(p, QRectF(feld.left(), feld.bottom() - 26, feld.width(), 22), grad(r), true, r.connected);
    }
}

// ── Entwurf 1: kleine Rosen in einer Spalte ────────────────────────
void entwurf1(QPainter& p, const QVector<Rotor>& rotoren)
{
    const int n = rotoren.size() - 1;
    const double spalte = 150;
    hauptrotor(p, QRectF(0, 0, kSheetW - spalte - 8, kSheetH), rotoren.first(), true);
    const double hoehe = (kSheetH - (n - 1) * 6.0) / n;
    for (int i = 0; i < n; ++i) {
        const QRectF feld(kSheetW - spalte, i * (hoehe + 6), spalte, hoehe);
        rahmen(p, feld);
        kopf(p, QRectF(feld.left(), feld.top(), feld.width(), 20), rotoren.at(i + 1).band, true);
        rose(p, QRectF(feld.left(), feld.top() + 20, feld.width(), feld.height() - 20 - 20),
             rotoren.at(i + 1).azimuthDeg, rotoren.at(i + 1).connected, hoehe > 110);
        wert(p, QRectF(feld.left(), feld.bottom() - 21, feld.width(), 18), grad(rotoren.at(i + 1)), false,
             rotoren.at(i + 1).connected);
    }
}

// ── Entwurf 2: flache Streifen, Rose links, Zahl rechts ────────────
void entwurf2(QPainter& p, const QVector<Rotor>& rotoren)
{
    const int n = rotoren.size() - 1;
    const double spalte = 186;
    hauptrotor(p, QRectF(0, 0, kSheetW - spalte - 8, kSheetH), rotoren.first(), true);
    const double hoehe = qMin(68.0, (kSheetH - (n - 1) * 6.0) / n);
    for (int i = 0; i < n; ++i) {
        const QRectF feld(kSheetW - spalte, i * (hoehe + 6), spalte, hoehe);
        rahmen(p, feld);
        rose(p, QRectF(feld.left() + 4, feld.top() + 4, feld.height() - 8, feld.height() - 8),
             rotoren.at(i + 1).azimuthDeg, rotoren.at(i + 1).connected, false);
        const QRectF text(feld.left() + feld.height() + 2, feld.top(), feld.width() - feld.height() - 8, feld.height());
        p.save();
        p.setFont(Style::capsFont(p.font(), 9));
        p.setPen(QColor(Style::kTextTertiary()));
        p.drawText(text.adjusted(0, 10, 0, 0), Qt::AlignTop | Qt::AlignLeft, rotoren.at(i + 1).band);
        p.restore();
        p.save();
        p.setFont(Style::monoFont(p.font(), 20, QFont::DemiBold));
        p.setPen(QColor(rotoren.at(i + 1).connected ? Style::kAmberText() : Style::kTextInactive()));
        p.drawText(text.adjusted(0, 0, 0, -8), Qt::AlignBottom | Qt::AlignLeft, grad(rotoren.at(i + 1)));
        p.restore();
    }
}

// ── Entwurf 3: nur Zahlen rechts, Hauptrotor so groß wie möglich ───
void entwurf3(QPainter& p, const QVector<Rotor>& rotoren)
{
    const int n = rotoren.size() - 1;
    const double spalte = 104;
    hauptrotor(p, QRectF(0, 0, kSheetW - spalte - 8, kSheetH), rotoren.first(), true);
    const QRectF leiste(kSheetW - spalte, 0, spalte, kSheetH);
    rahmen(p, leiste);
    kopf(p, QRectF(leiste.left(), leiste.top(), leiste.width(), 20), QStringLiteral("weitere"), true);
    const double hoehe = (leiste.height() - 24) / n;
    for (int i = 0; i < n; ++i) {
        const QRectF zeile(leiste.left() + 8, 24 + i * hoehe, leiste.width() - 16, hoehe);
        p.save();
        p.setFont(Style::capsFont(p.font(), 9));
        p.setPen(QColor(Style::kTextTertiary()));
        p.drawText(zeile.adjusted(0, 6, 0, 0), Qt::AlignTop | Qt::AlignLeft, rotoren.at(i + 1).band);
        p.restore();
        wert(p, zeile.adjusted(0, 14, 0, -6), grad(rotoren.at(i + 1)), hoehe > 60, rotoren.at(i + 1).connected);
        if (i + 1 < n) {
            p.setPen(QColor(255, 255, 255, 16));
            p.drawLine(QPointF(zeile.left(), zeile.bottom()), QPointF(zeile.right(), zeile.bottom()));
        }
    }
}

// ── Entwurf 4: Raster rechts (2 Spalten), spart Höhe bei vieren ────
void entwurf4(QPainter& p, const QVector<Rotor>& rotoren)
{
    const int n = rotoren.size() - 1;
    const int spalten = n > 2 ? 2 : 1;
    const double zellbreite = 96;
    const double block = spalten * zellbreite + (spalten - 1) * 6;
    hauptrotor(p, QRectF(0, 0, kSheetW - block - 8, kSheetH), rotoren.first(), true);
    const int zeilen = (n + spalten - 1) / spalten;
    const double zellhoehe = qMin(130.0, (kSheetH - (zeilen - 1) * 6.0) / zeilen);
    for (int i = 0; i < n; ++i) {
        const int sp = i % spalten;
        const int ze = i / spalten;
        const QRectF feld(kSheetW - block + sp * (zellbreite + 6), ze * (zellhoehe + 6), zellbreite, zellhoehe);
        rahmen(p, feld);
        kopf(p, QRectF(feld.left(), feld.top(), feld.width(), 18), rotoren.at(i + 1).band, true);
        rose(p, QRectF(feld.left(), feld.top() + 18, feld.width(), feld.height() - 18 - 18),
             rotoren.at(i + 1).azimuthDeg, rotoren.at(i + 1).connected, false);
        wert(p, QRectF(feld.left(), feld.bottom() - 19, feld.width(), 16), grad(rotoren.at(i + 1)), false,
             rotoren.at(i + 1).connected);
    }
}

} // namespace

class TestRotorEntwuerfe : public QObject
{
    Q_OBJECT

private slots:
    void zeichneBlaetter();
};

void TestRotorEntwuerfe::zeichneBlaetter()
{
    const QByteArray dir = qgetenv("CP_SHEET_DIR");
    if (dir.isEmpty()) {
        QSKIP("ohne CP_SHEET_DIR keine Blätter");
    }
    QDir().mkpath(QString::fromLocal8Bit(dir));

    const QVector<Rotor> zwei{{QStringLiteral("2m"), 301, true}, {QStringLiteral("70cm"), 90, true}};
    const QVector<Rotor> vier{{QStringLiteral("2m"), 301, true},
                              {QStringLiteral("70cm"), 90, true},
                              {QStringLiteral("23cm"), 215, true},
                              {QStringLiteral("6m"), 44, false}};

    struct Blatt {
        QString name;
        void (*zeichnen)(QPainter&, const QVector<Rotor>&);
    };
    const QVector<Blatt> blaetter{{QStringLiteral("1-spalte-rosen"), entwurf1},
                                   {QStringLiteral("2-streifen"), entwurf2},
                                   {QStringLiteral("3-nur-zahlen"), entwurf3},
                                   {QStringLiteral("4-raster"), entwurf4}};

    for (const Blatt& blatt : blaetter) {
        for (const auto& satz : {qMakePair(QStringLiteral("2rotoren"), zwei), qMakePair(QStringLiteral("4rotoren"), vier)}) {
            QPixmap bild(kSheetW, kSheetH);
            bild.fill(QColor(16, 20, 26));
            QPainter p(&bild);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            blatt.zeichnen(p, satz.second);
            p.end();
            const QString pfad = QStringLiteral("%1/rotor-entwurf-%2-%3.png")
                                     .arg(QString::fromLocal8Bit(dir), blatt.name, satz.first);
            QVERIFY(bild.save(pfad));
            qInfo().noquote() << "Blatt:" << pfad;
        }
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestRotorEntwuerfe tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "test_rotor_entwuerfe.moc"
