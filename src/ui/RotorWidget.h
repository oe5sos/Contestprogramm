#pragma once

#include "core/RotorDialStyle.h"
#include "core/terrain/LineOfSight.h"

#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

class QLineEdit;
class QMouseEvent;
class QPainter;
class QPushButton;
class QResizeEvent;
class QTimer;

namespace Contestprogramm {

// Compass-dial widget for one rotor slot, per the plan's Phase 3 UI
// direction ("die UI zeigt entsprechend zwei Kompass-Widgets nebeneinander,
// je mit Bandbeschriftung"). Instantiated on demand by MainWindow -- one
// bound to AppController::rotor1Client(), one to rotor2Client(), only
// while that slot's ContestSettings::rotor1Enabled/rotor2Enabled is true
// -- each told its own free-text label (ContestSettings::rotor1Label/
// rotor2Label -- operator text, not a fixed band identity), second-
// antenna settings, and (for whichever slot ContestSettings::
// band1296RotorSlot points at) an extra "+23cm" badge.
//
// Phase 2 terrain data landed 2026-09-12 -- see setTerrainSectors()
// below: a red/amber wash painted on the ring itself, marking which
// directions have known-poor line-of-sight. Deliberately a pure
// INFORMATIONAL marker, not a restriction of any kind -- it never
// touches the needle, the live azimuth readout, or the caption text
// (operator, same day: "rotor soll nicht blockiert werden, nur
// markiert werden, welche richtung nicht gut ist, keine sperre!!!").
// Those three stay reserved for isInMechanicalStopZone() alone, the
// one condition that genuinely IS a restriction (the rotor cannot
// normally arrive there on its own). An earlier version of this file
// had a "deliberate scope cut" note here saying the line-of-sight
// calculation did not exist yet; it does now (core/terrain/*).
class RotorWidget : public QWidget {
    Q_OBJECT

public:
    explicit RotorWidget(const QString& bandLabel, QWidget* parent = nullptr);

    // Free-text label shown in the panel header (ContestSettings::
    // rotor1Label / rotor2Label) -- no longer a fixed band identity, see
    // the class comment. Live-editable via SettingsDialog without
    // recreating the widget (MainWindow only recreates a RotorWidget
    // when its slot's enabled flag itself changes).
    void setBandLabel(const QString& label);
    QString bandLabel() const { return m_bandLabel; }

    // Current azimuth, degrees true (0..360), from RotctldClient::
    // azimuthChanged OR startSimulatedTurn()'s own timer (see its doc
    // comment). update()s the paint and emits azimuthDegChanged() below
    // either way -- MainWindow relies on that signal (not a fresh read
    // of RotctldClient::azimuthDeg()) to mirror this widget's heading
    // onto MapWidget, precisely so a simulated turn (which never touches
    // RotctldClient at all) still reaches the map (operator, 2026-09-14:
    // "zeiger sollte sich parallel auch in der karte drehen").
    void setAzimuthDeg(double azimuthDeg);
    double azimuthDeg() const { return m_azimuthDeg; }

    void setConnected(bool connected);

    // The currently selected/clicked candidate's bearing, per the plan's
    // "Turn Antenna" direction -- only ever set from an actual chat-feed
    // click (see MainWindow::handleCandidateActivated); there is no
    // invented default target.
    void setTargetBearing(double bearingDeg, double distanceKm, const QString& callsign = QString(),
                           const QString& grid = QString());
    void clearTargetBearing();
    bool hasTargetBearing() const { return m_hasTarget; }
    // Accessors added alongside the readout pass that draws these values
    // (drawReadout()'s AKTUELL/ZIEL/ENTFERNUNG block + target-station
    // caption line) -- same reasoning the second-antenna accessors below
    // already give: RotorWidget had no getter for any of this before,
    // only the setter, so a test could not confirm what actually reached
    // the paint code versus what MainWindow thought it sent.
    double targetBearingDeg() const { return m_targetBearingDeg; }
    double targetDistanceKm() const { return m_targetDistanceKm; }
    QString targetCallsign() const { return m_targetCallsign; }
    QString targetGrid() const { return m_targetGrid; }

    // Second antenna at a fixed angular offset from this rotor's actual
    // heading (ContestSettings::rotor1SecondAntennaEnabled/-OffsetDeg or
    // the slot-2 equivalent) -- draws a second needle + its own
    // numbered ring-edge marker when enabled.
    void setSecondAntenna(bool enabled, double offsetDeg);
    // Accessors added alongside the dial-style property above so a test
    // can confirm second-antenna state survives a style switch (see
    // tests/test_rotorwidget.cpp) -- RotorWidget had no getter for this
    // at all before, only the setter.
    bool secondAntennaEnabled() const { return m_secondAntennaEnabled; }
    double secondAntennaOffsetDeg() const { return m_secondAntennaOffsetDeg; }

    // Half-power beamwidth of the antenna(s) on this rotor, degrees
    // (5..120, default 30) -- the translucent cone the dial draws
    // around each needle (design sheet "Rotoren: Kegel", operator
    // 2026-09-21: "4 bitte erledigen"). One value per rotor, shared by
    // both antennas of a stacked pair, and the same number the map's
    // own "Öffnungswinkel Rotor N" preference holds -- MainWindow feeds
    // it from there so the two instruments always agree.
    void setBeamwidthDeg(double degrees);
    double beamwidthDeg() const { return m_beamwidthDeg; }

    // "+23cm" (or similar) badge shown next to the band label when this
    // rotor's slot is also the one ContestSettings::band1296RotorSlot
    // points at. Empty clears it.
    void setExtraBandBadge(const QString& badgeText);

    // Terrain line-of-sight sectors around the compass, one entry per
    // integer degree (index 0 = 0deg/North, ..., 359 = 359deg -- see
    // TerrainDataManager::sectorSweep(), which is the one intended
    // producer of this data). Paints a red (Blocked) / amber (Marginal)
    // wash on the ring itself (FullCompass/PartialArc styles -- the two
    // with an actual ring to paint on) via drawTerrainSectorWash() --
    // and ONLY that. Deliberately does NOT touch the needle, the live
    // azimuth readout, or the caption text, even when the current
    // azimuth itself sits in a Blocked sector -- this is a pure
    // informational marker, never a restriction (operator, 2026-09-12:
    // "rotor soll nicht blockiert werden, nur markiert werden, welche
    // richtung nicht gut ist, keine sperre!!!"); isInMechanicalStopZone()
    // remains the only thing that changes those three, since it alone
    // is a genuine restriction. An empty vector (the default) disables
    // the ring wash entirely, matching this widget's behaviour before
    // Phase 2 terrain data existed at all -- MainWindow only calls this
    // once AppController::terrainDataManager() actually has a sweep to
    // give it.
    void setTerrainSectors(const QVector<LineOfSightClass>& sectorsByDegree);

    // Which of the four paint styles this widget renders (see
    // core/RotorDialStyle.h) -- one operator-wide setting
    // (ContestSettings::rotorDialStyle), so MainWindow calls this on
    // both RotorWidget instances together (see
    // MainWindow::applyRotorWidgetSettings()), never per-slot. Every
    // style shares the exact same underlying state (azimuth, connected,
    // target, second antenna, labels) -- only the paintEvent() path
    // differs. Default FullCompass matches this widget's only rendering
    // before this property existed.
    void setDialStyle(RotorDialStyle style);
    RotorDialStyle dialStyle() const { return m_dialStyle; }

    // wrap360(currentAzimuthDeg + offsetDeg) -- the second antenna's
    // actual physical bearing. Exposed as a standalone static so it is
    // directly unit-testable without constructing a widget.
    static double secondAntennaBearing(double currentAzimuthDeg, double offsetDeg);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

signals:
    // Fires every time setAzimuthDeg() runs, whatever drove it (a real
    // RotctldClient reading or startSimulatedTurn()'s own timer) -- see
    // setAzimuthDeg()'s own doc comment for why MainWindow listens to
    // THIS instead of re-reading RotctldClient::azimuthDeg().
    void azimuthDegChanged(double azimuthDeg);

    // Per-panel quick-options affordance (⚙, see the header's own
    // m_optionsButton) -- emitted when the operator picks a style from
    // showOptionsPopup()'s menu. Mirrors PanelHeaderBar's own lock
    // button (setLocked(...) then emit lockToggled(...)): this widget
    // applies the chosen style to ITSELF immediately (showOptionsPopup()
    // calls setDialStyle() before emitting), so it behaves correctly
    // stand-alone; this signal is for the owner to sync anything this
    // widget cannot reach by itself -- here, the OTHER rotor compass
    // and ContestSettings::rotorDialStyle, since that field is one
    // operator-wide setting shared by both (see its own comment).
    // MainWindow::applyRotorSlot() connects this the same way it already
    // wires RotctldClient's signals into a freshly created widget.
    void dialStyleRequested(RotorDialStyle style);

    // A click (single OR double) inside the dial ring, at the bearing
    // under the cursor -- operator, 2026-09-14, first asking for
    // double-click ("dies haben wir bei longpath", Longpath's own
    // RotorDialWidget::mouseDoubleClickEvent()), then the same day
    // making plain a single click should already move it: "klicken auf
    // die gradanzeige und der rotor muss sich dahin drehen." A double-
    // click still fires this twice in a row (once per press) -- harmless,
    // the second click just re-commands the same bearing. This widget
    // does not itself know how to command hardware (RotctldClient lives
    // in MainWindow, one level up, shared with the map-click/suggestion-
    // accept path) -- it only reports the bearing the operator pointed
    // at; MainWindow wires this the same way it already wires
    // RotctldClient's own signals into a freshly created widget (see
    // applyRotorSlot()).
    void rotateRequested(double bearingDeg);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    static QPointF pointOnCircle(const QPointF& center, double radius, double angleDeg);
    // Bearing under `pos` (widget-local coordinates), or a negative value
    // inside the hub's own dead zone or when the active style has no
    // circular dial to aim at (LinearScale) -- same "-1 == not a usable
    // click" convention Longpath's own RotorDialWidget::bearingAt() uses.
    // Reuses FullCompass/Digital's own centre/radius formula (paintEvent()'s
    // switch) even for PartialArc, whose real drawn radius is a few
    // pixels smaller (the caption-allowance cut, see that case's own
    // comment) -- close enough for a click target, not worth a second,
    // near-identical geometry calculation just for hit-testing.
    double bearingAt(const QPointF& pos) const;
    // Shared by mousePressEvent()/mouseDoubleClickEvent() and
    // m_targetInput's own returnPressed handler -- operator, 2026-09-14:
    // "wenn ich dort im ziel meine zielrichtung eingebe ... sollte
    // automatisch der zeiger drehen", the same outcome a dial click
    // already produces, just from typed input instead of a click
    // position. Sets ZIEL (distance unknown, same -1.0 sentinel a manual
    // click uses), asks MainWindow to command the hardware via
    // rotateRequested(), and starts the offline simulated turn when
    // nothing is connected -- the exact three steps mousePressEvent()
    // already does inline.
    void commitManualTarget(double bearingDeg);
    // Keeps m_targetInput's displayed text in sync with m_targetBearingDeg/
    // m_hasTarget -- called from setTargetBearing()/clearTargetBearing().
    // Skipped while the field has focus so a live keystroke never gets
    // clobbered by the widget's own state (e.g. a real RotctldClient
    // update landing mid-edit).
    void syncTargetInputText();
    // Sizes/positions m_targetInput over the ZIEL column of whichever
    // dial style is currently active (columnRect(valuesRow, 1) in
    // drawReadout()'s own coordinate space, replicated here since that
    // computation lives inside a paint method) -- called from the
    // constructor, resizeEvent(), and setDialStyle(). Hidden entirely for
    // RotorDialStyle::Digital, which has its own separate glass-panel
    // ZIEL readout (drawDigitalReadout()) with no equivalent slot yet.
    void updateTargetInputGeometry();
    // Steps m_azimuthDeg toward m_targetBearingDeg by a fixed per-tick
    // amount, purely in software -- operator, 2026-09-14, after a manual
    // dial click set ZIEL but nothing visibly moved with no rotor
    // connected: "dreht noch nicht". No real rotor to ask, so there is no
    // hardware speed/stop-awareness to respect here (unlike
    // MainWindow::pushRotorHeadingToMap()'s real BeamHeading::plan()
    // path) -- this is a demo/proof-of-life animation, not a claim about
    // real hardware, which is exactly why m_simulated (see its own
    // comment) marks every value it touches as such rather than letting
    // it read as a real position.
    void stepSimulatedTurn();
    // Starts (or restarts, if the target moved again mid-turn) the
    // simulated-turn timer -- called from mousePressEvent()/
    // mouseDoubleClickEvent() only while !m_connected.
    void startSimulatedTurn();
    void drawPanelHeader(QPainter& painter) const;
    // Real QPushButton child overlaid on the painted header -- RotorWidget
    // draws its own chrome (see the class comment above) rather than
    // using PanelHeaderBar, but the ⚙ affordance itself still has to be
    // a real, clickable widget, not something hand-painted and manually
    // hit-tested (same reasoning MapWidget's class comment gives for its
    // own real QCheckBox/QPushButton row). updateOptionsButtonGeometry()
    // keeps it pinned to the header's top-right corner across resizes.
    void updateOptionsButtonGeometry();
    // Builds and shows the three-entry style menu, positioned below the
    // ⚙ button the same way the real Longpath TxApplet::showFinePopup()
    // positions its own gear-triggered popup (below, right-aligned).
    void showOptionsPopup();
    void drawGlow(QPainter& painter, const QPointF& center, double radius) const;
    void drawTicks(QPainter& painter, const QPointF& center, double radius) const;
    void drawCompassLabel(QPainter& painter, const QPointF& center, double radius, double angleDeg, const QString& text) const;
    void drawDegreeLabel(QPainter& painter, const QPointF& center, double radius, double angleDeg) const;
    // `alpha` dims the needle (0..255) -- used to mark a stale/unknown
    // reading (rotor not connected) without hiding it outright, per
    // HAUSSTIL rule 7 ("Unbekannt ist ein Strich, keine Null" --
    // applied here as "unknown fades, not vanishes or reads as zero").
    // `blocked` overrides the normal amber "measured" colour with the
    // warning-red HAUSSTIL reserves for genuine warnings -- see
    // isInMechanicalStopZone() above.
    void drawNeedle(QPainter& painter, const QPointF& center, double radius, double angleDeg,
                     Qt::PenStyle style = Qt::SolidLine, int alpha = 255, bool blocked = false) const;
    void drawTargetMarker(QPainter& painter, const QPointF& center, double radius, double angleDeg) const;
    // The beamwidth wedge under a needle: `color` at the hub fading to
    // nothing at the ring, `alpha` dimmed like the needle's own.
    void drawBeamCone(QPainter& painter, const QPointF& center, double radius, double angleDeg,
                      const QColor& color, int alpha) const;
    // The same wedge on the linear track: a translucent band of the
    // beamwidth around `x`, split in two when it wraps past 0/360.
    void drawLinearBeamBand(QPainter& painter, double trackLeft, double trackWidth, double trackY,
                            double angleDeg, const QColor& color, int alpha) const;
    // The three readout cells' geometry (label + value block, its
    // value row), shared by drawReadout() and updateTargetInputGeometry()
    // so the editable ZIEL field sits exactly on its painted cell.
    QRect readoutBlockRect() const;
    QRect readoutValuesRow() const;
    // The big readout number's size: kFontDisplay when a zero-padded
    // "000°" fits a cell with air around it, else the Digital style's
    // 28px -- a 300px-wide panel is one cell short of the big size.
    int readoutValueFontPx() const;
    // The sunken glass a readout cell sits in -- kInsetBg, subtle
    // border, inset shadow along the top (the same treatment
    // drawGlassPanel() gives the Digital style's panels).
    void drawReadoutInset(QPainter& painter, const QRect& box) const;
    // Polar (ring-edge) numbered mark, used by FullCompass/PartialArc --
    // computes the point and delegates to drawNumberedMarkAt() below,
    // which LinearScale's own (non-polar) marks also share.
    void drawNumberedMark(QPainter& painter, const QPointF& center, double radius, double angleDeg,
                           const QString& number) const;
    void drawNumberedMarkAt(QPainter& painter, const QPointF& pos, const QString& number) const;
    void drawReadout(QPainter& painter, const QRect& area) const;
    // Shared by drawReadout() and drawDigitalReadout() -- the target-
    // station caption line (callsign/grid/distance, or the SPERRZONE
    // warning, or the plain unknown-dash fallback) is identical logic
    // regardless of which readout style paints it; only the
    // surrounding layout differs. Out-parameters rather than a little
    // return struct, matching this file's existing preference for
    // plain helpers over new types (see TextSegment in the .cpp, which
    // stays file-local precisely because nothing outside this file
    // needs it either).
    void computeCaptionLine(QString& text, QColor& color, bool& show) const;
    // Shared by drawReadout() and drawDigitalReadout() -- the
    // connection-status dot + caps text row is identical in both.
    void drawConnectionStatusRow(QPainter& painter, const QRect& row) const;

    // -- Dial-style dispatch (RotorDialStyle, core/RotorDialStyle.h) --
    // four paint paths sharing the same state above; called from
    // paintEvent() based on m_dialStyle.
    void paintFullCompassDial(QPainter& painter, const QPointF& center, double radius) const;
    void paintPartialArcDial(QPainter& painter, const QPointF& center, double radius) const;
    void paintLinearScaleDial(QPainter& painter, const QRect& area) const;
    // Digital-only: the ring itself -- same size and same center/radius
    // math as paintFullCompassDial() (operator, 2026-09-11: "rotor
    // gleich groß wie die anderen" -- an earlier version gave Digital a
    // deliberately smaller, secondary ring; corrected here to share the
    // exact same dial-area split every other style uses, not a shrunk
    // one). Reuses drawGlow()/drawTicks()/drawNeedlesAndTarget() exactly
    // like paintFullCompassDial() does.
    void paintDigitalDial(QPainter& painter, const QPointF& center, double radius) const;
    // Digital-only: REPLACES drawReadout() for this style -- glowing
    // monospace digits inside a "black glass" panel instead of
    // drawReadout()'s plain text block (see RotorDialStyle::Digital's
    // own comment and Design2.dc.html), but sized to fit the SAME
    // kTextAreaHeight budget every other style's drawReadout() uses (an
    // earlier version had its own taller budget with much bigger digits
    // and a third Entfernung panel; operator, 2026-09-11: "die anzahlen
    // viel kleiner, entfernung muss weg" -- both cut here).
    void drawDigitalReadout(QPainter& painter, const QRect& area) const;
    // One "black glass" readout panel (label + near-black inset box +
    // glowing value) -- drawDigitalReadout()'s own building block,
    // used for both of Aktuell/Ziel.
    void drawGlassPanel(QPainter& painter, const QRect& box, const QString& label, const QString& value,
                         const QColor& valueColor, int valueFontPx) const;
    // Approximates build2.py's CSS `text-shadow: 0 0 18px <color>55`
    // glow, which QPainter has no direct primitive for -- same
    // technique drawNeedle()'s own halo pen already uses elsewhere in
    // this file (a duller/wider pass underneath the crisp one), here as
    // a ring of low-alpha offset copies behind the crisp text instead
    // of a stroke.
    void drawGlowingValueText(QPainter& painter, const QRect& box, const QString& text, const QColor& color,
                               int fontPx) const;

    // Shared by FullCompass and PartialArc -- both draw needles/target
    // marker/pivot dot identically from a center+radius, only the
    // ring/ticks behind them differ (full ring vs. arc-with-gap).
    void drawNeedlesAndTarget(QPainter& painter, const QPointF& center, double radius) const;

    // PartialArc-only: the 300-degree arc outline itself (replaces
    // FullCompass's full-circle ring), its own filtered tick/label set
    // (skips the gap interior, fewer degree numbers than FullCompass --
    // see Rotor-C.dc.html), and the amber gap-boundary marks + caption.
    // See RotorWidget.cpp's drawArcGapMarkers() for the gap's own scope
    // note (fixed visual convention, not live stop data).
    void drawPartialArcRing(QPainter& painter, const QPointF& center, double radius) const;
    void drawPartialArcTicks(QPainter& painter, const QPointF& center, double radius) const;
    void drawArcGapMarkers(QPainter& painter, const QPointF& center, double radius) const;
    // True only for PartialArc when the current heading itself sits
    // inside the mechanical no-go gap (kArcGapStartDeg..kArcGapEndDeg) --
    // a rotor cannot normally arrive there on its own, so this is a
    // fault/manual-override indicator, not a routine reading. Drives the
    // warning-red needle/wash/caption treatment (per Rotor-C.dc.html's
    // "SPERRZONE" state) in drawReadout()/drawNeedle()/drawArcGapMarkers().
    bool isInMechanicalStopZone() const;
    // Paints the ring itself where m_terrainSectors marks Blocked/
    // Marginal -- FullCompass/PartialArc only (the two dial styles with
    // an actual ring to paint the wash onto), called right after
    // drawTicks() in each so the wash sits behind the needles. Same
    // "loop a degree range, pointOnCircle(), build a QPainterPath,
    // stroke once" technique drawArcGapMarkers() already uses for its
    // own (fixed, mechanical-only) wash.
    void drawTerrainSectorWash(QPainter& painter, const QPointF& center, double radius) const;

    // LinearScale-only: a horizontal azimuth track instead of a dial.
    void drawLinearTicks(QPainter& painter, double trackLeft, double trackRight, double trackY) const;
    void drawLinearCardinalTick(QPainter& painter, double x, double trackY, const QString& label) const;
    void drawLinearIntercardinalTick(QPainter& painter, double x, double trackY, double labelDeg) const;
    // `isSecondAntenna` picks the shorter/dashed variant for the second
    // antenna's handle -- same "which needle is which" distinction
    // FullCompass/PartialArc make via solid-vs-dashed + numbered marks.
    void drawLinearHandle(QPainter& painter, double x, double trackY, int alpha, bool isSecondAntenna) const;
    void drawLinearTargetMarker(QPainter& painter, double x, double trackY) const;

    QString m_bandLabel;
    QString m_extraBandBadge;

    double m_azimuthDeg = 0.0;
    bool m_connected = false;
    // True once a simulated turn has ever run (see startSimulatedTurn())
    // and cleared the moment a real connection lands (setConnected(true))
    // -- marks m_azimuthDeg as a software demo value, not a real rotor
    // reading, everywhere the readout/badge would otherwise hide it for
    // being unconnected. Longpath's own RotorDialWidget has the same
    // concept under the same name (m_simulated) for the same reason.
    bool m_simulated = false;
    QTimer* m_simTimer = nullptr;

    bool m_hasTarget = false;
    double m_targetBearingDeg = 0.0;
    double m_targetDistanceKm = 0.0;
    QString m_targetCallsign;
    QString m_targetGrid;

    bool m_secondAntennaEnabled = false;
    double m_secondAntennaOffsetDeg = 0.0;
    double m_beamwidthDeg = 30.0;

    RotorDialStyle m_dialStyle = RotorDialStyle::FullCompass;

    // Empty (the default) means terrain sectors are disabled entirely --
    // see setTerrainSectors()'s own comment. Always either empty or
    // exactly 360 entries; isCurrentAzimuthTerrainBlocked()/
    // drawTerrainSectorWash() both guard on the exact size rather than
    // trusting an out-of-band "enabled" flag.
    QVector<LineOfSightClass> m_terrainSectors;

    QPushButton* m_optionsButton = nullptr;
    // Real, editable ZIEL field -- operator, 2026-09-14: "ich habe jetzt
    // aktuell dreihundertsechsunddreißig grad auf antenne eins, darunter
    // ... sollte ziel sein und das sollte leer sein. wenn ich dort im
    // ziel meine zielrichtung eingebe ... sollte automatisch der zeiger
    // drehen." Replaces drawReadout()'s own painted ZIEL value (the
    // "Ziel" caps LABEL above it stays painted) -- see
    // updateTargetInputGeometry()/commitManualTarget()/
    // syncTargetInputText() for how it's positioned, committed, and kept
    // in sync.
    QLineEdit* m_targetInput = nullptr;
};

} // namespace Contestprogramm
