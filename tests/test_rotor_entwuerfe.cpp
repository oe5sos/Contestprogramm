// Entwurfsblätter zur Rotor-Anordnung, 2026-10-08.
//
// Martin: "vielleicht kann man als option 1-4 rotoren rechts neben dem
// hauptrotor einblenden. sprich übereinander" / "die rotoren müssen
// natürlich kleiner sein" / "vielleicht kann bei 2 rotoren die
// hauptanzeige mehr nach links rutschen" / "besser du machst ein paar
// vorschläge" / "würde sie in das eigentliche bild setzen, also kein
// zweites window" / "die graphiken natürlich übernehmen, die wir schon
// haben".
//
// Also: EIN Panel mit einem Rahmen und einer Kopfzeile, und darin die
// ECHTEN RotorWidgets (chromlos, siehe RotorWidget::setChromeless) --
// keine nachgezeichneten Skizzen, sondern dieselben Zifferblätter, die
// das Programm auch sonst malt, in der wirklichen Panelgröße 618x284.
//
// Läuft nur mit CP_SHEET_DIR im Environment; in der CI tut er nichts.

#include <QtTest>

#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include "core/RotorDialStyle.h"
#include "ui/MapWidget.h"
#include "ui/RotorWidget.h"
#include "ui/StyleKit.h"

using namespace Contestprogramm;

namespace {

constexpr int kSheetW = 618;
constexpr int kSheetH = 284;
constexpr int kHeaderH = 26;

struct Rotor {
    QString band;
    double azimuthDeg;
    bool connected;
};

// Ein echtes RotorWidget in der gewünschten Größe, chromlos, mit
// gesetzter Peilung -- und als Bild zurück.
QPixmap kompass(const Rotor& r, const QSize& groesse, bool zielGesetzt = false)
{
    RotorWidget widget(r.band);
    widget.setChromeless(true);
    widget.setDialStyle(RotorDialStyle::FullCompass);
    widget.setConnected(r.connected);
    widget.setAzimuthDeg(r.azimuthDeg);
    if (zielGesetzt) {
        widget.setTargetBearing(r.azimuthDeg + 24.0, 471.0, QStringLiteral("DL1ABC"));
    }
    widget.resize(groesse);
    QPixmap bild(groesse);
    bild.fill(Qt::transparent);
    widget.render(&bild, QPoint(), QRegion(), QWidget::DrawChildren);
    return bild;
}

// Dieselbe Zeichnung, nur kleiner: das Widget wird in seiner richtigen
// Groesse gerendert und das Bild massstaeblich verkleinert. Martin,
// 2026-10-08: "keine neues design vom ziffernblatt" / "wuerde die tolle
// grafik komplett verschlechtern" -- ein auf 80 px gequetschtes
// Zifferblatt ordnet seine Teile neu an und sieht grob aus; ein
// verkleinertes Bild behaelt jede Proportion.
QPixmap kompassSkaliert(const Rotor& r, const QSize& ziel)
{
    // Referenzgroesse: die, fuer die das Zifferblatt gezeichnet ist.
    const QSize referenz(300, 300);
    QPixmap gross = kompass(r, referenz);
    const double faktor = qMin(ziel.width() / double(referenz.width()), ziel.height() / double(referenz.height()));
    return gross.scaled(QSize(int(referenz.width() * faktor), int(referenz.height() * faktor)),
                        Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void panelRahmen(QPainter& p, const QString& titel)
{
    const QRectF alles(0.5, 0.5, kSheetW - 1.0, kSheetH - 1.0);
    QPainterPath pfad;
    pfad.addRoundedRect(alles, Style::kPanelRadius, Style::kPanelRadius);
    p.fillPath(pfad, QColor(Style::kPanelBg()));
    p.setPen(QPen(QColor(Style::kBorderSubtle()), 1.0));
    p.drawPath(pfad);

    const QRectF kopf(0, 0, kSheetW, kHeaderH);
    QLinearGradient bg(kopf.topLeft(), kopf.bottomLeft());
    bg.setColorAt(0.0, QColor(255, 255, 255, 12));
    bg.setColorAt(1.0, QColor(0, 0, 0, 30));
    p.fillRect(kopf, bg);
    p.fillRect(QRectF(0, 0, 3, kHeaderH), QColor(Style::kAmberText()));
    p.setPen(QColor(255, 255, 255, 22));
    p.drawLine(kopf.bottomLeft(), kopf.bottomRight());
    p.setFont(Style::capsFont(p.font(), 11));
    p.setPen(QColor(Style::kTextPrimary()));
    p.drawText(kopf.adjusted(10, 0, -30, 0), Qt::AlignVCenter | Qt::AlignLeft, titel);
    // Das Zahnrad rechts oben, wie im echten Panelkopf.
    p.setFont(Style::capsFont(p.font(), 12));
    p.setPen(QColor(Style::kTextScale()));
    p.drawText(kopf.adjusted(0, 0, -10, 0), Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("⚙"));
}

QString gradText(const Rotor& r)
{
    return r.connected
               ? QStringLiteral("%1°").arg(QString::number(qRound(r.azimuthDeg)).rightJustified(3, QLatin1Char('0')))
               : QStringLiteral("—");
}

// ── Entwurf A: Spalte rechts, kleine echte Rosen ───────────────────
void entwurfA(QPainter& p, const QVector<Rotor>& rotoren)
{
    panelRahmen(p, QStringLiteral("Rotoren"));
    const int n = rotoren.size() - 1;
    const int spalte = n > 2 ? 118 : 140;
    const int hauptBreite = kSheetW - spalte - 16;
    p.drawPixmap(8, kHeaderH + 2, kompass(rotoren.first(), QSize(hauptBreite, kSheetH - kHeaderH - 10), true));

    p.save();
    p.setPen(QColor(255, 255, 255, 18));
    p.drawLine(kSheetW - spalte - 10, kHeaderH + 8, kSheetW - spalte - 10, kSheetH - 8);
    p.restore();

    const int hoehe = (kSheetH - kHeaderH - 8) / n;
    for (int i = 0; i < n; ++i) {
        const QPixmap klein = kompassSkaliert(rotoren.at(i + 1), QSize(spalte - 20, hoehe - 22));
        const int x = kSheetW - spalte - 2 + (spalte - 6 - klein.width()) / 2;
        p.drawPixmap(x, kHeaderH + 6 + i * hoehe, klein);
        // Band und Peilung daneben in der Schrift des Programms, nicht
        // in die Rose hineingequetscht.
        p.save();
        p.setFont(Style::capsFont(p.font(), 8));
        p.setPen(QColor(Style::kTextTertiary()));
        p.drawText(QRectF(kSheetW - spalte - 2, kHeaderH + 2 + i * hoehe, spalte - 6, 12),
                   Qt::AlignHCenter | Qt::AlignTop, rotoren.at(i + 1).band);
        p.setFont(Style::monoFont(p.font(), 13, QFont::DemiBold));
        p.setPen(QColor(rotoren.at(i + 1).connected ? Style::kAmberText() : Style::kTextInactive()));
        p.drawText(QRectF(kSheetW - spalte - 2, kHeaderH + 6 + i * hoehe + klein.height(), spalte - 6, 16),
                   Qt::AlignHCenter | Qt::AlignVCenter, gradText(rotoren.at(i + 1)));
        p.restore();
    }
}

// ── Entwurf B: kleine Rosen oben rechts ins Bild gesetzt ───────────
void entwurfB(QPainter& p, const QVector<Rotor>& rotoren)
{
    panelRahmen(p, QStringLiteral("Rotoren"));
    const int n = rotoren.size() - 1;
    const int klein = n > 2 ? 86 : 108;
    p.drawPixmap(8, kHeaderH + 2, kompass(rotoren.first(), QSize(kSheetW - klein - 30, kSheetH - kHeaderH - 10), true));
    for (int i = 0; i < n; ++i) {
        const int y = kHeaderH + 4 + i * (klein + 4);
        const QPixmap bild = kompassSkaliert(rotoren.at(i + 1), QSize(klein, klein - 14));
        p.drawPixmap(kSheetW - klein - 10 + (klein - bild.width()) / 2, y, bild);
        p.save();
        p.setFont(Style::monoFont(p.font(), 12, QFont::DemiBold));
        p.setPen(QColor(rotoren.at(i + 1).connected ? Style::kAmberText() : Style::kTextInactive()));
        p.drawText(QRectF(kSheetW - klein - 10, y + bild.height() - 2, klein, 14), Qt::AlignCenter,
                   rotoren.at(i + 1).band + QStringLiteral("  ") + gradText(rotoren.at(i + 1)));
        p.restore();
    }
}

// ── Entwurf C: rechts nur Band und Peilung, keine zweite Rose ──────
void entwurfC(QPainter& p, const QVector<Rotor>& rotoren)
{
    panelRahmen(p, QStringLiteral("Rotoren"));
    const int n = rotoren.size() - 1;
    const int leiste = 112;
    p.drawPixmap(8, kHeaderH + 2, kompass(rotoren.first(), QSize(kSheetW - leiste - 20, kSheetH - kHeaderH - 10), true));
    p.save();
    p.setPen(QColor(255, 255, 255, 18));
    p.drawLine(kSheetW - leiste - 8, kHeaderH + 8, kSheetW - leiste - 8, kSheetH - 8);
    p.restore();
    const double hoehe = (kSheetH - kHeaderH - 16.0) / n;
    for (int i = 0; i < n; ++i) {
        const QRectF zeile(kSheetW - leiste, kHeaderH + 8 + i * hoehe, leiste - 10, hoehe);
        p.save();
        p.setFont(Style::capsFont(p.font(), 9));
        p.setPen(QColor(Style::kTextTertiary()));
        p.drawText(zeile.adjusted(0, 4, 0, 0), Qt::AlignTop | Qt::AlignLeft, rotoren.at(i + 1).band);
        p.setFont(Style::monoFont(p.font(), n > 2 ? 20 : 26, QFont::DemiBold));
        p.setPen(QColor(rotoren.at(i + 1).connected ? Style::kAmberText() : Style::kTextInactive()));
        p.drawText(zeile.adjusted(0, 14, 0, -6), Qt::AlignVCenter | Qt::AlignLeft, gradText(rotoren.at(i + 1)));
        p.restore();
    }
}


// ══ Dritte Runde ═══════════════════════════════════════════════════
// Martin, 2026-10-08: "1:1 die gleichen design, ein window, darin sind
// rechts übereinander die beiden rotoren und links daneben deutlich
// größer die log karte".
//
// Also kein Hauptrotor mehr, sondern: Karte und Rotoren teilen sich
// EIN Panel. Links die Karte, so groß wie sie nur werden kann, rechts
// die Kompasse übereinander -- beide Teile unverändert, wie sie das
// Programm heute zeichnet.

constexpr int kWideW = 1100;
constexpr int kWideH = 520;

QPixmap karte(const QSize& groesse, const QVector<Rotor>& rotoren, bool chromlos)
{
    MapWidget widget;
    widget.setOwnGrid(QStringLiteral("JN67UT"));
    widget.setOwnLabel(QStringLiteral("OE5SOS"));
    widget.setRotor1Heading(rotoren.size() > 0 && rotoren.at(0).connected,
                            rotoren.isEmpty() ? 0.0 : rotoren.at(0).azimuthDeg, QStringLiteral("2m"));
    if (rotoren.size() > 1) {
        widget.setRotor2Heading(rotoren.at(1).connected, rotoren.at(1).azimuthDeg, QStringLiteral("70cm"));
    }
    widget.setScoreSummary(37, 9841, QStringLiteral("DL1ABC 471 km"));
    Q_UNUSED(chromlos);
    widget.resize(groesse);
    QPixmap bild(groesse);
    bild.fill(Qt::transparent);
    widget.render(&bild, QPoint(), QRegion(), QWidget::DrawChildren);
    return bild;
}

void breiterRahmen(QPainter& p, const QString& titel)
{
    const QRectF alles(0.5, 0.5, kWideW - 1.0, kWideH - 1.0);
    QPainterPath pfad;
    pfad.addRoundedRect(alles, Style::kPanelRadius, Style::kPanelRadius);
    p.fillPath(pfad, QColor(Style::kPanelBg()));
    p.setPen(QPen(QColor(Style::kBorderSubtle()), 1.0));
    p.drawPath(pfad);
    const QRectF kopf(0, 0, kWideW, kHeaderH);
    QLinearGradient bg(kopf.topLeft(), kopf.bottomLeft());
    bg.setColorAt(0.0, QColor(255, 255, 255, 12));
    bg.setColorAt(1.0, QColor(0, 0, 0, 30));
    p.fillRect(kopf, bg);
    p.fillRect(QRectF(0, 0, 3, kHeaderH), QColor(Style::kAmberText()));
    p.setPen(QColor(255, 255, 255, 22));
    p.drawLine(kopf.bottomLeft(), kopf.bottomRight());
    p.setFont(Style::capsFont(p.font(), 11));
    p.setPen(QColor(Style::kTextPrimary()));
    p.drawText(kopf.adjusted(10, 0, -30, 0), Qt::AlignVCenter | Qt::AlignLeft, titel);
    p.setFont(Style::capsFont(p.font(), 12));
    p.setPen(QColor(Style::kTextScale()));
    p.drawText(kopf.adjusted(0, 0, -10, 0), Qt::AlignVCenter | Qt::AlignRight, QStringLiteral("\u2699"));
}

// ── Entwurf D: Karte links groß, Rotoren rechts übereinander ───────
void entwurfD(QPainter& p, const QVector<Rotor>& rotoren)
{
    breiterRahmen(p, QStringLiteral("Karte / Rotoren"));
    const int spalte = 300;      // die Breite, fuer die das Zifferblatt gezeichnet ist
    const int innenH = kWideH - kHeaderH - 12;
    p.drawPixmap(6, kHeaderH + 6, karte(QSize(kWideW - spalte - 20, innenH), rotoren, true));
    p.save();
    p.setPen(QColor(255, 255, 255, 18));
    p.drawLine(kWideW - spalte - 12, kHeaderH + 8, kWideW - spalte - 12, kWideH - 8);
    p.restore();
    const int hoehe = (innenH - 6) / 2;
    for (int i = 0; i < qMin(2, rotoren.size()); ++i) {
        p.drawPixmap(kWideW - spalte - 6, kHeaderH + 6 + i * (hoehe + 6),
                     kompass(rotoren.at(i), QSize(spalte, hoehe)));
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
    const QVector<Blatt> blaetter{{QStringLiteral("A-spalte"), entwurfA},
                                   {QStringLiteral("B-eingesetzt"), entwurfB},
                                   {QStringLiteral("C-nur-zahlen"), entwurfC}};

    {
        QPixmap bild(kWideW, kWideH);
        bild.fill(QColor(16, 20, 26));
        QPainter p(&bild);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        entwurfD(p, zwei);
        p.end();
        const QString pfad = QStringLiteral("%1/rotor-D-karte-links.png").arg(QString::fromLocal8Bit(dir));
        QVERIFY(bild.save(pfad));
        qInfo().noquote() << "Blatt:" << pfad;
    }

    for (const Blatt& blatt : blaetter) {
        for (const auto& satz :
             {qMakePair(QStringLiteral("2rotoren"), zwei), qMakePair(QStringLiteral("4rotoren"), vier)}) {
            QPixmap bild(kSheetW, kSheetH);
            bild.fill(QColor(16, 20, 26));
            QPainter p(&bild);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::TextAntialiasing, true);
            blatt.zeichnen(p, satz.second);
            p.end();
            const QString pfad = QStringLiteral("%1/rotor-%2-%3.png")
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
