#include "ui/SiteComparisonDialog.h"

#include "core/Maidenhead.h"
#include "core/terrain/HorizonSweep.h"
#include "core/terrain/KnownSites.h"
#include "core/terrain/SiteHorizonManager.h"
#include "ui/HorizonCompassWidget.h"
#include "ui/StyleKit.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace Contestprogramm {

namespace {
// Combo item data values -- see SiteComparisonDialog::
// applyComboSelection()'s own doc comment for what each one means.
// >= 0 is a real index into knownSites().
constexpr int kComboDataNone = -2;
constexpr int kComboDataOwnStation = -3;
constexpr int kComboDataManual = -1;

// One legend swatch + label -- operator, 2026-09-14, after seeing the
// compass live for the first time: "was ist schwarz, was ist grün, was
// ist rot" -- there was no green to begin with (HorizonCompassWidget's
// Clear state is a deliberately neutral near-background tone, not a
// colour, matching RotorWidget's own "only mark problems" convention),
// but nothing in this dialog explained ANY of the four states, so the
// question was entirely fair. `fill`/`stroke` must be the exact same
// Style:: colours HorizonCompassWidget::paintEvent() uses for this
// classification, kept in sync by hand since the widget computes its
// own QColor locally rather than exposing a shared palette accessor --
// if that widget's colours ever change, this legend needs to change
// with it.
QWidget* buildLegendSwatch(QWidget* parent, const QColor& fill, const QColor& stroke, const QString& label)
{
    auto* row = new QWidget(parent);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(6);

    auto* swatch = new QLabel(row);
    swatch->setFixedSize(14, 14);
    swatch->setStyleSheet(QStringLiteral("background-color: rgba(%1,%2,%3,%4); border: 1px solid %5;")
                               .arg(fill.red())
                               .arg(fill.green())
                               .arg(fill.blue())
                               .arg(fill.alpha())
                               .arg(stroke.name()));
    rowLayout->addWidget(swatch);

    auto* text = new QLabel(label, row);
    text->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary()));
    rowLayout->addWidget(text);

    return row;
}

// The four HorizonCompassWidget/LineOfSightClass states, left to right
// in reading order from "worst" to "best" plus the one non-terrain
// state (Unknown) -- see buildLegendSwatch()'s own doc comment on why
// this must track that widget's colours by hand.
QWidget* buildLegendRow(QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(buildLegendSwatch(row, QColor(Style::kRedBg()), QColor(Style::kRedBorder()),
                                         QStringLiteral("Blockiert -- echtes Hindernis")));
    layout->addWidget(buildLegendSwatch(row, QColor(Style::kAmberBg()), QColor(Style::kAmberBorder()),
                                         QStringLiteral("Grenzwertig -- knapp unter Sichtlinie")));
    layout->addWidget(buildLegendSwatch(row, QColor(Style::kInsetBg()), QColor(Style::kBorderSubtle()),
                                         QStringLiteral("Offen -- kein Hindernis gefunden")));
    {
        QColor unknownFill(Style::kTextInactive());
        unknownFill.setAlpha(60);
        layout->addWidget(buildLegendSwatch(row, unknownFill, QColor(Style::kBorderSubtle()),
                                             QStringLiteral("Unbekannt -- Kacheldaten laden noch")));
    }
    layout->addStretch(1);
    return row;
}
} // namespace

SiteComparisonDialog::SiteComparisonDialog(const ContestSettings& settings, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_horizonManager(new SiteHorizonManager(this))
{
    setWindowTitle(QStringLiteral("Standortvergleich"));
    resize(920, 620);

    auto* introLabel = new QLabel(
        QStringLiteral(
            "Vergleicht die reale Geländeoffenheit von bis zu zwei Standorten -- unabhängig von Karte und Rotor, "
            "mit echten SRTM-Höhendaten bis auf rund 180 km statt der festen 50-km-Nahbereichsprüfung, die der "
            "Rotor-/Kartenkompass laufend zeigt (siehe dazu die Klassenkommentare in HorizonSweep.h und "
            "SiteComparisonDialog.h). Das Laden weit entfernter Kachel-Daten kann beim ersten Aufruf etwas "
            "dauern; die Kompassringe füllen sich dann von selbst nach."),
        this);
    introLabel->setWordWrap(true);

    auto* slotsLayout = new QHBoxLayout;
    slotsLayout->addWidget(buildSlotGroup(QStringLiteral("Standort A"), m_slotA));
    slotsLayout->addWidget(buildSlotGroup(QStringLiteral("Standort B"), m_slotB));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(introLabel);
    mainLayout->addWidget(buildLegendRow(this));
    mainLayout->addLayout(slotsLayout, 1);
    mainLayout->addWidget(buttons);

    // A tile finishing its fetch resolves more bearings for whichever
    // slot(s) actually share that exact origin -- comparing the
    // spin-box values directly rather than tracking a separate "which
    // slot asked for this" map, since SiteHorizonManager's own cache
    // key is exactly this same (lat, lon) pair (see its own doc
    // comment) and a QDoubleSpinBox always reports back the identical
    // rounded value it was given.
    connect(m_horizonManager, &SiteHorizonManager::sweepChanged, this, [this](double lat, double lon) {
        if (m_slotAActive && m_slotA.latSpin->value() == lat && m_slotA.lonSpin->value() == lon) {
            refreshSlot(m_slotA);
        }
        if (m_slotBActive && m_slotB.latSpin->value() == lat && m_slotB.lonSpin->value() == lon) {
            refreshSlot(m_slotB);
        }
    });

    // Slot A defaults to the operator's own current station, Slot B to
    // "Kein Vergleich" -- see this class's own constructor doc comment
    // in the header for why.
    const int ownStationIndex = m_slotA.combo->findData(kComboDataOwnStation);
    m_slotA.combo->setCurrentIndex(ownStationIndex);
    applyComboSelection(m_slotA, ownStationIndex);

    const int noneIndexB = m_slotB.combo->findData(kComboDataNone);
    m_slotB.combo->setCurrentIndex(noneIndexB);
    applyComboSelection(m_slotB, noneIndexB);
}

SiteComparisonDialog::~SiteComparisonDialog() = default;

QWidget* SiteComparisonDialog::buildSlotGroup(const QString& title, Slot& slot)
{
    auto* group = new QGroupBox(title, this);

    slot.combo = new QComboBox(group);
    slot.combo->addItem(QStringLiteral("Kein Vergleich"), kComboDataNone);
    // Offered in BOTH slots (not just Slot A -- despite the constructor
    // only defaulting Slot A to it) so the operator can just as well
    // put "Eigener Standort" into Slot B while comparing two named
    // candidates in the other direction; it costs nothing to offer it
    // twice.
    slot.combo->addItem(QStringLiteral("Eigener Standort (aus den Einstellungen)"), kComboDataOwnStation);
    slot.combo->addItem(QStringLiteral("-- Manuell (Koordinaten unten) --"), kComboDataManual);
    const QVector<KnownSite>& sites = knownSites();
    for (int i = 0; i < sites.size(); ++i) {
        slot.combo->addItem(sites.at(i).name, i);
    }

    // Same QLocale::c()/6-decimal/°N-°O convention SettingsDialog's own
    // exact-location and GPS-paste spin boxes already use -- see that
    // class's own m_exactLatSpin/m_gpsLatSpin comments for why (period-
    // decimal GPS coordinates must be accepted regardless of this
    // dialog's own German locale).
    slot.latSpin = new QDoubleSpinBox(group);
    slot.latSpin->setRange(-90.0, 90.0);
    slot.latSpin->setDecimals(6);
    slot.latSpin->setSuffix(QStringLiteral(" °N"));
    slot.latSpin->setLocale(QLocale::c());

    slot.lonSpin = new QDoubleSpinBox(group);
    slot.lonSpin->setRange(-180.0, 180.0);
    slot.lonSpin->setDecimals(6);
    slot.lonSpin->setSuffix(QStringLiteral(" °O"));
    slot.lonSpin->setLocale(QLocale::c());

    slot.elevationSpin = new QDoubleSpinBox(group);
    slot.elevationSpin->setRange(-500.0, 9000.0);
    slot.elevationSpin->setSuffix(QStringLiteral(" m"));

    slot.antennaHeightSpin = new QDoubleSpinBox(group);
    slot.antennaHeightSpin->setRange(0.0, 200.0);
    slot.antennaHeightSpin->setSuffix(QStringLiteral(" m"));
    // Pre-filled from the operator's own current antenna-height setting
    // as a reasonable starting guess for either slot, regardless of
    // which site ends up chosen there -- a scouted KnownSites.h entry
    // only ever carries a GROUND elevation (see that struct's own doc
    // comment), never a mast height, so there is nothing more specific
    // to default to. Freely editable afterwards, same "convenience pre-
    // fill, not a lock-in" posture SettingsDialog::
    // applyKnownSiteSelection() already documents for its own elevation
    // field.
    slot.antennaHeightSpin->setValue(m_settings.antennaHeightM);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Standort:"), slot.combo);
    form->addRow(QStringLiteral("Breite:"), slot.latSpin);
    form->addRow(QStringLiteral("Länge:"), slot.lonSpin);
    form->addRow(QStringLiteral("Höhe Grund:"), slot.elevationSpin);
    form->addRow(QStringLiteral("Antennenhöhe:"), slot.antennaHeightSpin);

    slot.compass = new HorizonCompassWidget(group);
    slot.summaryLabel = new QLabel(group);
    slot.summaryLabel->setWordWrap(true);

    auto* layout = new QVBoxLayout(group);
    layout->addLayout(form);
    layout->addWidget(slot.compass, 1);
    layout->addWidget(slot.summaryLabel);

    // `&slot` binds directly to whichever of m_slotA/m_slotB this call
    // is for (a reference-to-a-reference-parameter aliases the same
    // object, not a copy) -- safe to capture for the dialog's own
    // lifetime, exactly like m_horizonManager's lambda above.
    connect(slot.combo, QOverload<int>::of(&QComboBox::activated), this,
            [this, &slot](int index) { applyComboSelection(slot, index); });
    for (QDoubleSpinBox* spin : {slot.latSpin, slot.lonSpin, slot.elevationSpin, slot.antennaHeightSpin}) {
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, &slot](double) { refreshSlot(slot); });
    }

    return group;
}

bool SiteComparisonDialog::isSlotActive(const Slot& slot) const
{
    return (&slot == &m_slotA) ? m_slotAActive : m_slotBActive;
}

void SiteComparisonDialog::applyComboSelection(Slot& slot, int index)
{
    const int itemData = slot.combo->itemData(index).toInt();
    bool& activeFlag = (&slot == &m_slotA) ? m_slotAActive : m_slotBActive;

    if (itemData == kComboDataNone) {
        activeFlag = false;
        slot.latSpin->setEnabled(false);
        slot.lonSpin->setEnabled(false);
        slot.elevationSpin->setEnabled(false);
        slot.antennaHeightSpin->setEnabled(false);
        refreshSlot(slot); // clears the compass/summary via the inactive branch below
        return;
    }

    activeFlag = true;
    slot.latSpin->setEnabled(true);
    slot.lonSpin->setEnabled(true);
    slot.elevationSpin->setEnabled(true);
    slot.antennaHeightSpin->setEnabled(true);

    // Operator, 2026-09-14, live-tested Feuerkogelhaus as "Eigener
    // Standort" and got a compass showing every one of the 36 sectors
    // Blocked -- independently re-verified against real SRTM data (only
    // ~13/36 should classify Blocked from that origin) and traced to
    // this exact spot: the four setValue() calls below each fire
    // m_slotX's own valueChanged->refreshSlot() connection (built in
    // buildSlotGroup()) INDIVIDUALLY, so the very first one (latSpin)
    // calls SiteHorizonManager::sweep() with the NEW lat/lon but
    // whatever OLD elevation/antenna-height happened to still be sitting
    // in those two spin boxes from the PREVIOUSLY selected site. That
    // premature, inconsistent call gets cached forever under the new
    // lat/lon key (SiteHorizonManager::sweep()'s own doc comment: same
    // key returns the FIRST call's result, by design, the same posture
    // TerrainDataManager::classification() already established) --
    // correct values arriving in the later setValue() calls can never
    // dislodge it. Blocking these four spins' signals for the whole
    // selection keeps every intermediate state private to this method,
    // so the explicit refreshSlot() call at the end below is the ONLY
    // one that ever reaches SiteHorizonManager, always with fully
    // self-consistent lat/lon/elevation/height.
    const QSignalBlocker blockLat(slot.latSpin);
    const QSignalBlocker blockLon(slot.lonSpin);
    const QSignalBlocker blockElevation(slot.elevationSpin);
    const QSignalBlocker blockAntennaHeight(slot.antennaHeightSpin);

    if (itemData == kComboDataOwnStation) {
        // Same "which origin wins" precedence AppController::
        // applyNetworkSettings() already uses when feeding
        // TerrainDataManager: the exact fields if useExactOwnLocation
        // is on AND actually filled in (0.0/0.0 is "not really set",
        // per ContestSettings::ownExactLatitude's own sentinel
        // comment), otherwise the grid centroid.
        double lat = 0.0;
        double lon = 0.0;
        const bool hasExactOwnLocation = m_settings.useExactOwnLocation
            && (m_settings.ownExactLatitude != 0.0 || m_settings.ownExactLongitude != 0.0);
        if (hasExactOwnLocation) {
            lat = m_settings.ownExactLatitude;
            lon = m_settings.ownExactLongitude;
        } else if (isValidGridSquare(m_settings.ownGrid)) {
            calculateLatLonFromGridSquare(m_settings.ownGrid, lat, lon);
        }
        slot.latSpin->setValue(lat);
        slot.lonSpin->setValue(lon);
        slot.elevationSpin->setValue(m_settings.ownElevationM);
        slot.antennaHeightSpin->setValue(m_settings.antennaHeightM);
    } else if (itemData >= 0) {
        const QVector<KnownSite>& sites = knownSites();
        if (itemData < sites.size()) {
            const KnownSite& site = sites.at(itemData);
            slot.latSpin->setValue(site.latitude);
            slot.lonSpin->setValue(site.longitude);
            slot.elevationSpin->setValue(site.elevationM);
            // antennaHeightSpin deliberately left untouched -- same
            // reasoning as its own setValue() above in the "Eigener
            // Standort" branch: a scouted site's GROUND elevation is
            // known, a mast height is not.
        }
    }
    // itemData == kComboDataManual ("-- Manuell --"): leave whatever is
    // already in the coordinate fields exactly as it is -- that is the
    // entire point of this option.

    refreshSlot(slot);
}

void SiteComparisonDialog::refreshSlot(Slot& slot)
{
    if (!isSlotActive(slot)) {
        slot.compass->setSectors({});
        slot.compass->setPlaceholderText(QStringLiteral("Kein Standort ausgewählt"));
        slot.summaryLabel->clear();
        return;
    }

    const QVector<HorizonSector> sectors = m_horizonManager->sweep(
        slot.latSpin->value(), slot.lonSpin->value(), slot.elevationSpin->value(), slot.antennaHeightSpin->value());
    slot.compass->setSectors(sectors);
    slot.summaryLabel->setText(summarize(sectors));
}

QString SiteComparisonDialog::compassLabel(int bearingDeg)
{
    // German 8-point compass abbreviations -- summarize()'s own small
    // helper, matching this app's own German-language UI throughout
    // rather than the English N/NE/E/SE convention.
    static const QVector<QString> labels
        = {QStringLiteral("N"),  QStringLiteral("NO"), QStringLiteral("O"),  QStringLiteral("SO"),
           QStringLiteral("S"),  QStringLiteral("SW"), QStringLiteral("W"),  QStringLiteral("NW")};
    int index = static_cast<int>(std::lround(bearingDeg / 45.0)) % 8;
    if (index < 0) {
        index += 8;
    }
    return labels.at(index);
}

QString SiteComparisonDialog::summarize(const QVector<HorizonSector>& sectors)
{
    if (sectors.isEmpty()) {
        return QString();
    }

    const bool allUnknown = std::all_of(sectors.cbegin(), sectors.cend(), [](const HorizonSector& sector) {
        return sector.classification == LineOfSightClass::Unknown;
    });
    if (allUnknown) {
        return QStringLiteral("Geländedaten werden geladen…");
    }

    // Longest contiguous run of Clear sectors -- linear scan, NOT
    // circular (a run that wraps past 359deg->0deg is reported as its
    // own two separate runs instead of one merged one). Same "not worth
    // the extra bookkeeping for a purely cosmetic difference" call
    // RotorWidget::drawTerrainSectorWash() already makes for its own
    // analogous wrap-around case.
    int bestStart = -1;
    int bestEnd = -1;
    int bestLen = 0;
    {
        int i = 0;
        const int n = sectors.size();
        while (i < n) {
            if (sectors.at(i).classification != LineOfSightClass::Clear) {
                ++i;
                continue;
            }
            int j = i;
            while (j < n && sectors.at(j).classification == LineOfSightClass::Clear) {
                ++j;
            }
            if (j - i > bestLen) {
                bestLen = j - i;
                bestStart = i;
                bestEnd = j - 1;
            }
            i = j;
        }
    }

    // The single worst obstruction across the whole sweep: Blocked
    // always outranks Marginal (a real obstruction is more worth
    // calling out than a graze, regardless of exact angle), and among
    // two sectors of the same classification the one with the higher
    // elevation angle -- the one rising further above eye level -- wins.
    const HorizonSector* worst = nullptr;
    for (const HorizonSector& sector : sectors) {
        if (sector.classification != LineOfSightClass::Blocked && sector.classification != LineOfSightClass::Marginal) {
            continue;
        }
        const bool worstIsBlocked = worst && worst->classification == LineOfSightClass::Blocked;
        const bool sectorIsBlocked = sector.classification == LineOfSightClass::Blocked;
        if (!worst || (sectorIsBlocked && !worstIsBlocked)
            || (sectorIsBlocked == worstIsBlocked && sector.maxElevationAngleDeg > worst->maxElevationAngleDeg)) {
            worst = &sector;
        }
    }

    QString text;
    if (bestLen > 0) {
        const HorizonSector& first = sectors.at(bestStart);
        const HorizonSector& last = sectors.at(bestEnd);
        text = QStringLiteral("Offen: %1° (%2) bis %3° (%4).")
                   .arg(first.bearingDeg)
                   .arg(compassLabel(first.bearingDeg))
                   .arg(last.bearingDeg)
                   .arg(compassLabel(last.bearingDeg));
    } else {
        text = QStringLiteral("Kein durchgehend offener Sektor gefunden.");
    }
    if (worst) {
        text += QStringLiteral(" Nächstes Hindernis: %1° (%2), ca. %3 km (%4).")
                    .arg(worst->bearingDeg)
                    .arg(compassLabel(worst->bearingDeg))
                    .arg(qRound(worst->obstructionDistanceKm))
                    .arg(worst->classification == LineOfSightClass::Blocked ? QStringLiteral("blockiert")
                                                                             : QStringLiteral("grenzwertig"));
    }
    return text;
}

} // namespace Contestprogramm
