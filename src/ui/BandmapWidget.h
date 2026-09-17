#pragma once

#include "core/BandmapModel.h"

#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>

namespace Contestprogramm {

// The bandmap: one band's spots on a vertical frequency axis, lowest
// frequency at the top (N1MM+'s reading order), each as a clickable
// callsign beside its frequency, the rig's own frequency as an amber
// marker. Worked stations are dimmed and struck through, everything
// else bright; labels that would overlap are pushed down and joined
// to their frequency by a leader line, so a pile of spots within a few
// kHz stays readable. A click on a label emits spotActivated() --
// MainWindow tunes the rig and fills the entry row (a QSY, the whole
// point of a bandmap).
//
// Range: the spots' span plus a margin, at least 60 kHz, always
// including the own frequency; with nothing to show, ±50 kHz around
// the rig, or the band's usual SSB/CW segment when the rig is silent.
class BandmapWidget : public QWidget {
    Q_OBJECT

public:
    explicit BandmapWidget(QWidget* parent = nullptr);

    void setBand(const QString& band);
    void setSpots(const QVector<BandmapSpot>& spots);
    void setOwnFrequencyHz(qint64 hz); // 0 = unknown

    // Label rectangles as last painted, in widget coordinates, one per
    // spot in setSpots() order -- what a click is tested against, and
    // what a test can inspect without a screen.
    QVector<QRect> labelRects() const { return m_labelRects; }

    // The painted frequency span, Hz.
    qint64 rangeLowHz() const { return m_lowHz; }
    qint64 rangeHighHz() const { return m_highHz; }

signals:
    void spotActivated(const QString& callsign, const QString& grid, qint64 freqHz);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void recomputeRange();
    void layoutLabels();
    int yForHz(qint64 hz) const;

    QString m_band = QStringLiteral("144");
    QVector<BandmapSpot> m_spots;
    qint64 m_ownHz = 0;
    qint64 m_lowHz = 0;
    qint64 m_highHz = 0;
    QVector<QRect> m_labelRects;
};

} // namespace Contestprogramm
