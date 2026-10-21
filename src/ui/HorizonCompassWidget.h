#pragma once

#include "core/terrain/HorizonSweep.h"

#include <QString>
#include <QVector>
#include <QWidget>

namespace Contestprogramm {

// Standalone, custom-painted 360-degree polar view of one
// computeHorizonSweep() result -- Contestprogramm-original (2026-09-14),
// built for SiteComparisonDialog's own site-scouting feature (see that
// class's own doc comment for the motivating problem: RotorWidget's/
// MapWidget's existing terrain wash is a fixed 50km near-field safety
// check and structurally cannot show a real obstruction like the Alps
// at 65-100km from a candidate site such as Jauerling). This widget
// carries no bearing needle, no rotor state, no click-to-aim input --
// unlike RotorWidget, it exists purely to LOOK AT one static sweep
// result, so it is a much smaller, purpose-built painter rather than a
// reuse of that class. It DOES reuse RotorWidget's own compass-angle
// convention (0deg = north/straight up, clockwise -- see this .cpp's
// own pointOnCircle()) and this app's dark-theme palette (Style::
// k...()), and paints Blocked/Marginal in the exact same two named
// colours (kRedBorder/kAmberWarn) RotorWidget's own (currently dormant,
// see RotorWidget::drawTerrainSectorWash()'s own 2026-09-14 comment)
// terrain-wash painter already established for those two states, so
// this reads as the same visual language rather than a second,
// competing one.
class HorizonCompassWidget : public QWidget {
    Q_OBJECT

public:
    explicit HorizonCompassWidget(QWidget* parent = nullptr);

    // Replaces the sweep this widget paints. An empty vector (the
    // default before setSectors() is ever called, and whatever
    // SiteComparisonDialog passes for a slot currently set to "Kein
    // Vergleich") paints a plain, unmarked ring with no wedges and
    // shows placeholderText() in the centre instead -- deliberately
    // distinct from a real sweep whose every bearing happens to be
    // Unknown (which paints 36 real, if uninformative, grey wedges),
    // so "no site picked for this slot" never looks like "a site was
    // picked but its terrain data hasn't loaded yet".
    void setSectors(const QVector<HorizonSector>& sectors);

    // Text shown centred inside the ring while sectors() is empty --
    // SiteComparisonDialog sets this to "Kein Standort ausgewählt" for
    // an inactive slot and to a short "wird geladen"-style message
    // right after a site is first picked but before setSectors() has
    // been called even once.
    void setPlaceholderText(const QString& text);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<HorizonSector> m_sectors;
    QString m_placeholderText;
};

} // namespace Contestprogramm
