// Kein Test -- ein Werkzeug. Rendert das ECHTE, bereits gebaute
// MapWidget mit frei erfundenen Beispiel-Stationen als Entwurfsblaetter
// in wirklicher Groesse, damit ueber Gestaltung an einem Bild
// entschieden wird und nicht an einer Beschreibung -- gleiche
// Begruendung/gleicher Aufbau wie Longpath/NereusSDR's eigenes
// tst_stilblatt.cpp (docs/design/HAUSSTIL.md-Praxis dort: "Martin
// beurteilt Oberflaechen-Entwuerfe an gerenderten PNG-Blaettern").
//
// Anders als ein von Hand nachgemalter HTML-Entwurf ist das hier kein
// Naeherungsbild -- es ist buchstaeblich derselbe QPainter-Code, der
// im laufenden Programm zeichnet, nur mit synthetischen statt echten
// Log-Daten befuellt, damit dieses Blatt unabhaengig vom aktuellen
// echten Contest-Log jederzeit neu erzeugt werden kann. Operator,
// 2026-09-13: "erstelle mockup fuer karte verbindungen", dann "erstelle
// weitere" -- mehrere Betriebsfaelle je Entwurf (die gleiche Regel wie
// beim SWR-Entwurfsblatt: "ein Entwurf, der nur im Gutfall ueberzeugt,
// taugt nicht"), je EIN eigenes PNG pro Fall (nie mehrere Varianten in
// einem Bild -- "am pdf kann man keinen unterschied erkennen, bitte
// jeweils 1 pro gruppe").
//
// Standort (JN67VV/OE5SOS) ist der echte Feuerkogel-Standort aus den
// Contestprogramm-Standardeinstellungen -- nur die Stationen darunter
// sind frei erfunden, klar erkennbar an ihren Platzhalter-Rufzeichen,
// nie mit echten geloggten Kontakten verwechselbar.

#include <QtTest>

#include <QDir>

#include <QApplication>
#include <QImage>
#include <QtMath>

#include <cmath>

#include "core/ColorTheme.h"
#include "ui/MapWidget.h"
#include "ui/StyleKit.h"

using namespace Contestprogramm;

namespace {

struct StationSeed {
    const char* call;
    const char* grid;
    bool worked;
};

// Renders one scenario and saves it to its own PNG -- see the file's
// own doc comment for why this is one-image-per-variant, not a
// combined sheet. Resets the active theme back to Bernstein before
// returning so later scenarios in the same run always start from a
// known state regardless of what a given scenario itself picked.
// Wahlweise abweichender Zuschnitt und Rand: `fullBleed` schaltet die
// Scheibe ab (die Panelkanten sind dann der Rand), `size` setzt ein
// breites Panelformat statt des quadratnahen Standardblatts.
struct SheetOptions {
    bool fullBleed = false;
    QSize size;
    bool withHorizonAndRotor = false;
    bool withBordersAndCities = false;
};

void renderScenario(const QString& path, ColorTheme theme, double visibleRangeKm,
                     const QVector<StationSeed>& seeds, const SheetOptions& options = SheetOptions())
{
    Style::setActiveTheme(theme);

    MapWidget widget;
    widget.setOwnGrid(QStringLiteral("JN67VV"));
    widget.setOwnLabel(QStringLiteral("OE5SOS"));
    widget.setVisibleRangeKm(visibleRangeKm);

    QVector<MapWidget::Station> stations;
    stations.reserve(seeds.size());
    for (const StationSeed& seed : seeds) {
        MapWidget::Station s;
        s.callsign = QLatin1String(seed.call);
        s.grid = QLatin1String(seed.grid);
        s.worked = seed.worked;
        stations.append(s);
    }
    widget.setStations(stations);
    widget.setFullBleedEnabled(options.fullBleed);
    if (options.withBordersAndCities) {
        // Martin, 2026-10-18: "man wuerde dann die karte mit grenzen,
        // spots, staedte rechteckig groesser und formatfuellend sehen"
        // -- also gehoeren genau die auf das Blatt.
        widget.setBordersLayerVisible(true);
        widget.setCitiesLayerVisible(true);
    }
    if (options.withHorizonAndRotor) {
        // Ein erfundener, aber gebirgsaehnlicher Horizont (Feuerkogel
        // schaut nach Nordosten frei, nach Sueden steht der Berg) und
        // ein gedrehter Rotor -- die beiden Dinge, die am Rand kleben
        // und deshalb beim Randwechsel als Erstes brechen wuerden.
        QVector<double> horizon(360, 0.0);
        for (int deg = 0; deg < 360; ++deg) {
            const double south = std::cos(qDegreesToRadians(static_cast<double>(deg) - 200.0));
            horizon[deg] = std::max(0.0, 5.5 * south);
        }
        widget.setHorizonProfile(horizon);
        widget.setRotor1Heading(true, 35.0, QStringLiteral("2m"));
        widget.setRotor2Heading(true, 300.0, QStringLiteral("70cm"));
    }

    // Wirkliche Groesse -- wie das Panel im echten Fenster typisch
    // aussieht (sizeHint(), auf eine vorzeigbare Mindestgroesse
    // angehoben), nicht kuenstlich vergroessert oder verkleinert.
    const QSize size = options.size.isValid() ? options.size : widget.sizeHint().expandedTo(QSize(760, 640));
    widget.resize(size);

    QImage sheet(size * 2, QImage::Format_ARGB32);
    sheet.setDevicePixelRatio(2.0);
    sheet.fill(Qt::transparent);
    widget.render(&sheet);

    QVERIFY(sheet.save(path));
    qInfo() << "Entwurfsblatt geschrieben:" << path << sheet.size();

    Style::setActiveTheme(ColorTheme::Bernstein);
}

} // namespace

class TestMapMockup : public QObject
{
    Q_OBJECT

private slots:
    // Der urspruengliche Fall: gemischter Normalbetrieb, 500 km,
    // Bernstein-Grundfarbe -- bereits einmal verschickt, hier nur zur
    // Vollstaendigkeit des Werkzeugs erneut mitgezeichnet.
    void drawStandardScenario();
    // "Ruhiger Start": kaum Aktivitaet, damit das Blatt nicht nur im
    // vollen Zustand ueberzeugt.
    void drawQuietScenario();
    // "Voller Betrieb, weiter gezoomt": deutlich mehr Stationen ueber
    // 1500 km verteilt (bis nach London/Warschau/Athen/Kiew) -- prueft
    // Dichte UND wie Grenzen/Staedte bei starkem Herauszoomen wirken.
    void drawBusyWideScenario();
    // Gleicher Stationssatz wie der Standardfall, aber im Gruen-Thema
    // (2026-09-21, die einzige Alternative zu Bernstein) -- direkter
    // Vergleich zur Bernstein-Fassung, gleiche Daten, nur die Farbe
    // wechselt.
    void drawGruenScenario();
    // Die Zusatzoption vom 2026-10-18 ("optional flaechenfuellend ...
    // die linien des fensters als begrenzer"), im breiten Panelformat,
    // in dem sie ueberhaupt erst etwas bringt -- und derselbe Fall als
    // Scheibe, damit der Unterschied an zwei Blaettern entschieden
    // wird und nicht an einer Beschreibung.
    void drawWidePanelAsDisc();
    void drawWidePanelFullBleed();
};

void TestMapMockup::drawStandardScenario()
{
    const QVector<StationSeed> seeds = {
        {"OE5DEMO", "JN78CD", true},
        {"OE1TEST", "JN77QT", true},
        {"OM3ABC", "JN88WV", false},
        {"S57DEF", "JN65WM", true},
        {"9A2GHI", "JN75RS", false},
        {"DL4JKL", "JN58QD", true},
        {"HB9MNO", "JN47GH", false},
        {"I2PQR", "JN45UV", false},
        {"HA5STU", "JN86EF", true},
    };
    renderScenario(QDir::temp().filePath(QStringLiteral("contestprogramm-map-mockup.png")), ColorTheme::Bernstein, 500.0, seeds);
}

void TestMapMockup::drawQuietScenario()
{
    const QVector<StationSeed> seeds = {
        {"OE5DEMO", "JN78CD", true},
        {"OE1TEST", "JN77QT", true},
        {"OM3ABC", "JN88WV", false},
    };
    renderScenario(QDir::temp().filePath(QStringLiteral("contestprogramm-map-mockup-ruhig.png")), ColorTheme::Bernstein, 500.0, seeds);
}

void TestMapMockup::drawBusyWideScenario()
{
    const QVector<StationSeed> seeds = {
        {"G4DEMO", "IO91MX", true},     // London
        {"F5TEST", "JN18AU", true},     // Paris
        {"DL2ABC", "JO62QC", true},     // Berlin
        {"SP3DEF", "JO92BR", false},    // Warschau
        {"SM6GHI", "JO89WX", false},    // Stockholm
        {"OZ1JKL", "JO65XU", true},     // Kopenhagen
        {"IK0MNO", "JN61AB", true},     // Rom
        {"SV1PQR", "KM17TN", false},    // Athen
        {"UR5STU", "KO50UU", false},    // Kiew
        {"HB9VWX", "JN47GH", true},     // Zuerich
        {"PA3YZA", "JO22WA", true},     // Amsterdam
        {"OM3BCD", "JN88WV", true},     // Bratislava
        {"9A3EFG", "JN75RS", true},     // Zagreb
        {"OK1HIJ", "JO70QK", false},    // Bruenn
    };
    renderScenario(QDir::temp().filePath(QStringLiteral("contestprogramm-map-mockup-voll-weit.png")), ColorTheme::Bernstein, 1500.0,
                    seeds);
}

void TestMapMockup::drawGruenScenario()
{
    const QVector<StationSeed> seeds = {
        {"OE5DEMO", "JN78CD", true},
        {"OE1TEST", "JN77QT", true},
        {"OM3ABC", "JN88WV", false},
        {"S57DEF", "JN65WM", true},
        {"9A2GHI", "JN75RS", false},
        {"DL4JKL", "JN58QD", true},
        {"HB9MNO", "JN47GH", false},
        {"I2PQR", "JN45UV", false},
        {"HA5STU", "JN86EF", true},
    };
    renderScenario(QDir::temp().filePath(QStringLiteral("contestprogramm-map-mockup-gruen.png")), ColorTheme::Gruen, 500.0,
                    seeds);
}

namespace {
// Breites Panel wie in Martins Fenster (Karte links, Zahlenspalte
// rechts) -- im quadratnahen Standardblatt faellt kaum auf, wie viel
// Flaeche die Scheibe liegen laesst.
const QVector<StationSeed>& widePanelSeeds()
{
    static const QVector<StationSeed> seeds = {
        {"DL2ABC", "JO62QC", true},     // Berlin
        {"OK1HIJ", "JO70QK", false},    // Bruenn
        {"OM3BCD", "JN88WV", true},     // Bratislava
        {"HA5STU", "JN86EF", true},     // Budapest
        {"9A3EFG", "JN75RS", true},     // Zagreb
        {"S57DEF", "JN65WM", false},    // Ljubljana
        {"I2PQR", "JN45UV", true},      // Mailand
        {"HB9MNO", "JN47GH", false},    // Zuerich
        {"DL4JKL", "JN58QD", true},     // Frankfurt
        {"F5TEST", "JN18AU", false},    // Paris
        {"PA3YZA", "JO22WA", false},    // Amsterdam
        {"SP3DEF", "JO92BR", false},    // Warschau
    };
    return seeds;
}
constexpr int kWidePanelWidth = 1240;
constexpr int kWidePanelHeight = 700;
} // namespace

void TestMapMockup::drawWidePanelAsDisc()
{
    SheetOptions options;
    options.size = QSize(kWidePanelWidth, kWidePanelHeight);
    options.withHorizonAndRotor = true;
    options.withBordersAndCities = true;
    renderScenario(QDir::temp().filePath(QStringLiteral("contestprogramm-map-breit-scheibe.png")), ColorTheme::Bernstein,
                    800.0, widePanelSeeds(), options);
}

void TestMapMockup::drawWidePanelFullBleed()
{
    SheetOptions options;
    options.size = QSize(kWidePanelWidth, kWidePanelHeight);
    options.withHorizonAndRotor = true;
    options.withBordersAndCities = true;
    options.fullBleed = true;
    renderScenario(QDir::temp().filePath(QStringLiteral("contestprogramm-map-breit-vollflaeche.png")), ColorTheme::Bernstein,
                    800.0, widePanelSeeds(), options);
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    TestMapMockup t;
    return QTest::qExec(&t, argc, argv);
}
#include "test_map_mockup.moc"
