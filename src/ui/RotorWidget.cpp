#include "ui/RotorWidget.h"

#include "core/BeamHeading.h"
#include "ui/StyleKit.h"

#include <QAction>
#include <QFont>
#include <QFontMetrics>
#include <QIntValidator>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QPushButton>
#include <QRadialGradient>
#include <QRectF>
#include <QResizeEvent>
#include <QStringList>
#include <QTimer>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace Contestprogramm {

namespace {
constexpr int kHeaderHeight = 28;
constexpr int kAccentBarWidth = 3;
constexpr int kDialMargin = 18;

// Readout block geometry (drawReadout(), below the dial in every style)
// -- per the design mockup's three-column AKTUELL/ZIEL/ENTFERNUNG block
// (Rotor-A.dc.html/Rotor-Dual.dc.html) plus a target-station caption
// line and the connection-status row this widget already had. Broken
// into named sub-heights (rather than one opaque total) so
// kTextAreaHeight below is the actual sum of what gets drawn, not a
// separately-eyeballed number that could silently drift out of sync.
constexpr int kReadoutTopPad = 6;
constexpr int kReadoutLabelHeight = 14;  // "Aktuell"/"Ziel"/"Entfernung" caps labels
constexpr int kReadoutValueHeight = 46;  // kFontDisplay (38px) big numbers + leading
constexpr int kReadoutRowGap = 4;
constexpr int kReadoutLineHeight = 18;   // one generic text line -- caption row and connection-status row both use this
constexpr int kReadoutBottomPad = 4;
constexpr int kTextAreaHeight = kReadoutTopPad + kReadoutLabelHeight + kReadoutValueHeight + kReadoutRowGap
    + kReadoutLineHeight + kReadoutRowGap + kReadoutLineHeight + kReadoutBottomPad;

// The dial keeps at least this much height; the readout block below it
// gives way first (see textAreaHeight()). Found on a real layout,
// 2026-09-21: a rotor row dragged lower than the widget's old fixed
// minimum was simply clipped at the panel's bottom edge, readout and
// all, instead of adapting.
constexpr int kMinDialAreaHeight = 150;

// Minimum widget width the three-column readout needs to avoid its own
// columns overlapping -- measured, not guessed: QFontMetrics on this
// ladder's kFontDisplay (38px) mono font puts a zero-padded "300°" at
// ~92px and "471"+" km" at ~89px (Style::monoFont()/capsFont(), the
// exact fonts drawReadout() itself uses) -- three columns of that need
// ~90px each plus breathing room around the column dividers, comfortably
// under 100px/column. The dial itself only ever needed 220px (the old
// minimumSizeHint() width, before this readout existed); this readout
// need is now the larger of the two, so it -- not the dial -- decides
// the widget's minimum width.
constexpr int kReadoutMinWidth = 300;

// The widget's real minimum width is the dial's, not the readout's --
// operator, 2026-09-21, two rotors in a 540px panel clipped at the
// right edge: "hier sollten die rotoren auch kleiner werden und nicht
// abgeschnitten sein". Below kReadoutMinWidth the readout first drops
// to its smaller value font (readoutValueFontPx()), and when even that
// no longer fits three columns it gives way entirely
// (readoutFitsWidth() -> textAreaHeight() 0), the same "give way, never
// clip" rule the readout already follows vertically. The dial itself
// only needs its minimum area plus its margins.
constexpr int kDialMinWidth = kMinDialAreaHeight + kDialMargin * 2;

// ⚙ options affordance, overlaid on the painted header -- sized to sit
// comfortably inside kHeaderHeight, same right-margin logic as the
// accent bar's own left-side 9px text inset.
constexpr int kOptionsButtonW = 20;
constexpr int kOptionsButtonH = 18;
constexpr int kOptionsButtonRightMargin = 8;

// How close counts as "arrived" for the header's turn-status badge --
// same default Longpath's own real rotor dial uses (m_tolerance{3.0},
// gui/widgets/RotorDialWidget.h).
constexpr double kArrivalToleranceDeg = 3.0;

// PartialArc: a fixed 60-degree gap centred on south (180 degrees),
// representing a mechanical stop / cable-wrap zone -- per Rotor-C.dc.html
// and this style's own scope note (see drawArcGapMarkers() below): a
// static visual convention of this one dial style, not live
// BeamHeading::Stop data or a per-rotor ContestSettings field, neither of
// which exists yet.
constexpr double kArcGapCenterDeg = 180.0;
constexpr double kArcGapHalfWidthDeg = 30.0;
constexpr double kArcGapStartDeg = kArcGapCenterDeg - kArcGapHalfWidthDeg; // 150
constexpr double kArcGapEndDeg = kArcGapCenterDeg + kArcGapHalfWidthDeg;   // 210

// LinearScale geometry -- see paintLinearScaleDial().
constexpr double kLinearMarginPx = 26.0;
constexpr double kLinearTrackHeightPx = 6.0;

// Needle half-width as a fraction of the dial radius, rather than a
// flat pixel constant -- derived from Main.dc.html/Design3.dc.html's
// own needle_polygon(half_w=6) at their r=148 canvas (gen.py, build.py/
// build3.py): 6/148 = 0.0405. A flat pixel width (this file's original
// 4.2, tuned for one particular radius) would come out too thin at
// FullCompass/PartialArc's normal size and, worse, way too thick
// relative to Digital's own small ring (RotorDialStyle::Digital,
// roughly half the radius) -- Design2.dc.html's own needle there uses
// a proportionally thinner half_w=4 at its own smaller r=84 precisely
// because of this. A radius-proportional width reproduces both mockups
// correctly from the one formula instead of needing a per-style
// override.
constexpr double kNeedleHalfWidthFraction = 6.0 / 148.0;

// Digital dial style readout geometry (drawDigitalReadout()) -- Design2.
// dc.html ("Option 2 -- Digital") was the original ground truth (big
// 54px/30px glowing digits, a small secondary ring, a third Entfernung
// panel), but the operator corrected that after seeing it live,
// 2026-09-11: "rotor gleich groß wie die anderen, die anzahlen viel
// kleiner, entfernung muss weg" -- ring now shares the other three
// styles' own dial area (see paintDigitalDial()), Entfernung is gone,
// and these sub-heights are sized to fit the same shared kTextAreaHeight
// footer every other style's drawReadout() already uses (not a second,
// taller budget of its own) -- summed into kDigitalTextAreaHeight only
// so a test can assert it is <= kTextAreaHeight, the actual constraint.
constexpr int kDigitalSidePad = 14;
constexpr int kDigitalColumnGap = 10;
constexpr int kDigitalTopPad = 4;
constexpr int kDigitalLabelHeight = 11;
constexpr int kDigitalLabelGap = 3;
constexpr int kDigitalGlassHeight = 38; // Aktuell/Ziel: 28px glow digits + tight top/bottom padding
// "viel kleiner" than the original 54px, but per the operator's very
// next correction ("dürfte aber vor allem bei digital Zahlen sein" --
// the numbers should still be what defines this style) NOT shrunk down
// to blend into the other styles' own kFontDisplay(38) either -- kept
// clearly its own, still-prominent glowing size.
constexpr int kDigitalValueFontPx = 28;
constexpr int kDigitalRowGap = 5;
constexpr int kDigitalCaptionHeight = 15;
constexpr int kDigitalStatusHeight = 16;
constexpr int kDigitalBottomPad = 4;
constexpr int kDigitalTextAreaHeight = kDigitalTopPad
    + kDigitalLabelHeight + kDigitalLabelGap + kDigitalGlassHeight
    + kDigitalRowGap
    + kDigitalCaptionHeight
    + kDigitalRowGap
    + kDigitalStatusHeight
    + kDigitalBottomPad;

// A short run of differently-styled text, drawn as one centered group.
// RotorWidget is a hand-painted instrument (no QLabel tree beneath the
// dial), so mixing fonts/colors on one readout line means drawing each
// segment separately rather than styling a rich-text QLabel the way
// e.g. FrequencyInstrument::refreshVfoRow() does.
struct TextSegment {
    QString text;
    QFont font;
    QColor color;
};

void drawCenteredSegments(QPainter& painter, const QRect& row, const QVector<TextSegment>& segments)
{
    int total = 0;
    for (const TextSegment& seg : segments) {
        total += QFontMetrics(seg.font).horizontalAdvance(seg.text);
    }
    int x = row.left() + (row.width() - total) / 2;
    for (const TextSegment& seg : segments) {
        painter.setFont(seg.font);
        painter.setPen(seg.color);
        const QFontMetrics fm(seg.font);
        const int w = fm.horizontalAdvance(seg.text);
        painter.drawText(QRect(x, row.top(), w, row.height()), Qt::AlignVCenter | Qt::AlignLeft, seg.text);
        x += w;
    }
}

// Slices a row into `count` equal-width columns and returns the one at
// `index` -- the three-column AKTUELL/ZIEL/ENTFERNUNG readout (see
// drawReadout()) uses this for both its label row and its value row, so
// a label and the big number underneath it always land in the exact
// same horizontal band.
QRect columnRect(const QRect& row, int index, int count = 3)
{
    const int colWidth = row.width() / count;
    return QRect(row.left() + index * colWidth, row.top(), colWidth, row.height());
}
} // namespace

RotorWidget::RotorWidget(const QString& bandLabel, QWidget* parent)
    : QWidget(parent)
    , m_bandLabel(bandLabel)
{
    setMinimumSize(minimumSizeHint());

    // Per-panel quick-options affordance (⚙) -- same glyph, hover
    // styling and tooltip wording as the real Longpath GridCellWidget
    // precedent (~/Longpath/NereusSDR/src/gui/applets/
    // GridCellWidget.cpp buildCellButtons()). Opens showOptionsPopup()
    // below rather than raising GridCellWidget's settingsRequested
    // signal, because this widget paints its own panel chrome instead
    // of using PanelHeaderBar (see this class's own comment) -- there
    // is no shared "hasExtendedSettings" host to ask, so the button
    // lives here and is always visible: RotorWidget always has three
    // selectable dial styles to offer, unlike a generic applet which
    // usually has nothing extra. Martin, 2026-09-10: "ich will das
    // design von den rotoren als option in der taskleiste ändern
    // können" -- this is that option, reached directly from the panel
    // instead of only through SettingsDialog's "Rotor-Anzeige" combo
    // (which stays, for discoverability -- see the class's own note).
    m_optionsButton = new QPushButton(QString::fromUtf8("⚙"), this);
    // Stable objectName -- lets a test find this exact button (same
    // convention PanelHeaderBar's own m_optionsButton now uses).
    m_optionsButton->setObjectName(QStringLiteral("rotorWidgetOptionsButton"));
    m_optionsButton->setFixedSize(kOptionsButtonW, kOptionsButtonH);
    m_optionsButton->setCursor(Qt::PointingHandCursor);
    m_optionsButton->setToolTip(QStringLiteral("Weitere Einstellungen"));
    m_optionsButton->setStyleSheet(Style::iconButtonStyle());
    connect(m_optionsButton, &QPushButton::clicked, this, &RotorWidget::showOptionsPopup);
    updateOptionsButtonGeometry();

    // Real, editable ZIEL field -- see this class's own m_targetInput
    // doc comment. Plain "336", no "°" suffix (a validator-friendly
    // numeric field, not a styled display) and no visible frame except a
    // focus underline, so it reads as part of the painted readout rather
    // than an obviously bolted-on control.
    m_targetInput = new QLineEdit(this);
    m_targetInput->setObjectName(QStringLiteral("rotorWidgetTargetInput"));
    m_targetInput->setAlignment(Qt::AlignCenter);
    m_targetInput->setFont(Style::monoFont(font(), Style::kFontDisplay, QFont::Bold));
    m_targetInput->setPlaceholderText(QStringLiteral("—"));
    m_targetInput->setToolTip(QStringLiteral("Zielrichtung eingeben (0–360°) und Enter drücken"));
    // 0-360 inclusive -- BeamHeading::wrap360() in commitManualTarget()
    // still normalizes it, this just keeps "9999" or "-40" from ever
    // being typeable in the first place.
    m_targetInput->setValidator(new QIntValidator(0, 360, m_targetInput));
    m_targetInput->setStyleSheet(QStringLiteral(
        "QLineEdit { background: transparent; border: none; color: %1; padding: 0; }"
        "QLineEdit:focus { border-bottom: 1px solid %1; }")
        .arg(Style::kBlueBg()));
    connect(m_targetInput, &QLineEdit::returnPressed, this, [this]() {
        bool ok = false;
        const double deg = m_targetInput->text().toDouble(&ok);
        if (!ok) {
            return;
        }
        commitManualTarget(deg);
        // Give the dial focus back -- operator, 2026-09-14, describing
        // the very next thing wanted after Enter is pressed: seeing the
        // rotor actually turn, not still be sitting in a text field with
        // its cursor blinking.
        setFocus();
    });
    updateTargetInputGeometry();
}

void RotorWidget::setAzimuthDeg(double azimuthDeg)
{
    m_azimuthDeg = BeamHeading::wrap360(azimuthDeg);
    update();
    emit azimuthDegChanged(m_azimuthDeg);
}

void RotorWidget::setBandLabel(const QString& label)
{
    if (m_bandLabel == label) {
        return;
    }
    m_bandLabel = label;
    update();
}

void RotorWidget::setConnected(bool connected)
{
    if (m_connected == connected) {
        return;
    }
    m_connected = connected;
    if (connected) {
        // A real link just came up -- any simulated demo turn stops
        // being relevant (and, worse, would otherwise fight the real
        // RotctldClient::azimuthChanged updates for control of
        // m_azimuthDeg). See m_simulated's own doc comment.
        m_simulated = false;
        if (m_simTimer) {
            m_simTimer->stop();
        }
    }
    update();
}

void RotorWidget::setTargetBearing(double bearingDeg, double distanceKm, const QString& callsign,
                                    const QString& grid)
{
    m_hasTarget = true;
    m_targetBearingDeg = BeamHeading::wrap360(bearingDeg);
    m_targetDistanceKm = distanceKm;
    m_targetCallsign = callsign;
    m_targetGrid = grid;
    update();
    syncTargetInputText();
}

void RotorWidget::clearTargetBearing()
{
    m_hasTarget = false;
    update();
    syncTargetInputText();
}

void RotorWidget::setSecondAntenna(bool enabled, double offsetDeg)
{
    const bool enabledChanged = m_secondAntennaEnabled != enabled;
    m_secondAntennaEnabled = enabled;
    m_secondAntennaOffsetDeg = offsetDeg;
    update();
    if (enabledChanged) {
        // m_targetInput's own geometry/font depend on this flag -- see
        // updateTargetInputGeometry()'s own comment.
        updateTargetInputGeometry();
    }
}

void RotorWidget::setBeamwidthDeg(double degrees)
{
    // The same 5..120 range the map's own "Öffnungswinkel" menu offers.
    const double clamped = std::clamp(degrees, 5.0, 120.0);
    if (qFuzzyCompare(clamped, m_beamwidthDeg)) {
        return;
    }
    m_beamwidthDeg = clamped;
    update();
}

void RotorWidget::setExtraBandBadge(const QString& badgeText)
{
    if (m_extraBandBadge == badgeText) {
        return;
    }
    m_extraBandBadge = badgeText;
    update();
}

void RotorWidget::setDialStyle(RotorDialStyle style)
{
    if (m_dialStyle == style) {
        return;
    }
    m_dialStyle = style;
    update();
    updateTargetInputGeometry();
}

double RotorWidget::secondAntennaBearing(double currentAzimuthDeg, double offsetDeg)
{
    return BeamHeading::wrap360(currentAzimuthDeg + offsetDeg);
}

QSize RotorWidget::minimumSizeHint() const
{
    // All four styles now share the exact same dial-area/text-area
    // split (Digital's ring and readout budget both match the other
    // three, per the operator's 2026-09-11 correction -- see
    // paintDigitalDial()'s and drawDigitalReadout()'s own comments), so
    // one flat minimum for every style, same as before Digital existed.
    // The readout is optional below that (textAreaHeight()) and
    // below kReadoutMinWidth (readoutFitsWidth()): the smallest legible
    // widget is the header and a small dial.
    return QSize(kDialMinWidth, kHeaderHeight + kMinDialAreaHeight);
}

bool RotorWidget::readoutFitsWidth() const
{
    // Three columns of the SMALLER value font (the one readoutValueFontPx()
    // already falls back to) must fit side by side -- same inset/column
    // arithmetic as readoutBlockRect()/columnRect(), spelled out here
    // because textAreaHeight() (which readoutBlockRect() needs) asks
    // this first.
    const int cellWidth = (width() - 12) / 3 - 3;
    const QFontMetrics small(Style::monoFont(font(), kDigitalValueFontPx));
    return small.horizontalAdvance(QStringLiteral("000°")) + 10 <= cellWidth;
}

QSize RotorWidget::sizeHint() const
{
    // Room for the full readout under a comfortable dial -- the size
    // the default panel layout gives each rotor.
    return QSize(kReadoutMinWidth, kHeaderHeight + 220 + kTextAreaHeight);
}

int RotorWidget::textAreaHeight() const
{
    // The readout block below the dial gives way before the dial does:
    // too low for all of it, the widget drops the connection row, then
    // the station caption, then the block itself, and never paints
    // past its own bottom edge.
    if (!readoutFitsWidth()) {
        return 0;
    }
    const int available = height() - kHeaderHeight - kMinDialAreaHeight;
    const bool digital = m_dialStyle == RotorDialStyle::Digital;
    const int full = digital ? kDigitalTextAreaHeight : kTextAreaHeight;
    const int withoutStatus = full - (digital ? kDigitalRowGap + kDigitalStatusHeight : kReadoutRowGap + kReadoutLineHeight);
    const int withoutCaption = withoutStatus - (digital ? kDigitalRowGap + kDigitalCaptionHeight : kReadoutRowGap + kReadoutLineHeight);
    for (int candidate : {full, withoutStatus, withoutCaption}) {
        if (candidate <= available) {
            return candidate;
        }
    }
    return 0;
}

void RotorWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateOptionsButtonGeometry();
    updateTargetInputGeometry();
}

void RotorWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const double deg = bearingAt(event->position());
    if (deg < 0.0) {
        QWidget::mousePressEvent(event);
        return;
    }
    commitManualTarget(deg);
}

void RotorWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Not "else" of mousePressEvent()'s own hit -- Qt delivers a
    // press+release pair for EACH click of a double-click, so the first
    // click already fired mousePressEvent() above (already re-commanding
    // the same bearing is harmless, see commitManualTarget()'s own doc
    // comment). This override exists only so Qt doesn't fall back to its
    // default double-click handling (which would otherwise also
    // re-dispatch as a second ordinary press) inside the dial ring.
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    const double deg = bearingAt(event->position());
    if (deg < 0.0) {
        return;
    }
    commitManualTarget(deg);
}

void RotorWidget::commitManualTarget(double bearingDeg)
{
    // Show the bearing as the new ZIEL right away, whether or not a
    // rotor is actually connected -- operator, 2026-09-14: "sollte
    // offline auch funktionieren". -1.0 is the "no known distance"
    // sentinel (see drawReadout()'s/computeCaptionLine()'s own comments)
    // -- a manually aimed bearing (clicked OR typed into m_targetInput)
    // has no station to measure distance to, unlike a candidate accepted
    // from the map/suggestion panel. emit rotateRequested() below still
    // asks MainWindow to actually command the hardware, which itself
    // does nothing further when nothing is connected (RotctldClient::
    // setAzimuth()'s own guard) -- this local call is only the visual
    // half.
    setTargetBearing(bearingDeg, -1.0);
    emit rotateRequested(bearingDeg);
    if (!m_connected) {
        startSimulatedTurn();
    }
}

void RotorWidget::startSimulatedTurn()
{
    m_simulated = true;
    if (!m_simTimer) {
        m_simTimer = new QTimer(this);
        m_simTimer->setInterval(100);
        connect(m_simTimer, &QTimer::timeout, this, &RotorWidget::stepSimulatedTurn);
    }
    if (!m_simTimer->isActive()) {
        m_simTimer->start();
    }
}

void RotorWidget::stepSimulatedTurn()
{
    // 2.5 deg/tick @ 100ms = 25 deg/s -- fast enough that a click reads
    // as "yes, this works" within a couple of seconds, not a literal
    // claim about any particular rotor's real turning speed.
    constexpr double kSimStepDeg = 2.5;
    const double diff = std::fmod(m_targetBearingDeg - m_azimuthDeg + 540.0, 360.0) - 180.0;
    if (std::abs(diff) <= kSimStepDeg) {
        setAzimuthDeg(m_targetBearingDeg);
        m_simTimer->stop();
        return;
    }
    setAzimuthDeg(m_azimuthDeg + (diff > 0.0 ? kSimStepDeg : -kSimStepDeg));
}

QPointF RotorWidget::pointOnCircle(const QPointF& center, double radius, double angleDeg)
{
    // Compass convention: 0 degrees is north (straight up), increasing
    // clockwise -- matches the bearing values calculateBearingInDegrees()
    // and BeamHeading already use everywhere else in this project.
    const double rad = qDegreesToRadians(angleDeg);
    return QPointF(center.x() + radius * std::sin(rad), center.y() - radius * std::cos(rad));
}

double RotorWidget::bearingAt(const QPointF& pos) const
{
    if (m_dialStyle == RotorDialStyle::LinearScale) {
        // No circular dial to aim at in this style -- see this method's
        // own header comment.
        return -1.0;
    }
    const int dialAreaTop = kHeaderHeight;
    const int dialAreaHeight = height() - kHeaderHeight - textAreaHeight();
    const QPointF center(width() / 2.0, dialAreaTop + dialAreaHeight / 2.0);
    const double radius = (std::min(width(), dialAreaHeight) - kDialMargin * 2) / 2.0;
    if (radius <= 0.0) {
        return -1.0;
    }

    const QPointF p = pos - center;
    // The dead zone scales with the dial, same floor/fraction Longpath's
    // own RotorDialWidget::bearingAt() uses -- a fixed few pixels would
    // swallow most of a small dragged-small panel's whole face, and a
    // pure fraction would let a tiny dead zone on a big panel register a
    // near-centre click as a wild, barely-intentional bearing.
    const double dead = std::max(5.0, radius * 0.16);
    const double distance = std::hypot(p.x(), p.y());
    if (distance < dead) {
        return -1.0;
    }
    // Upper bound added 2026-09-14 -- operator, after m_targetInput
    // started living in the readout area below the dial: "dreht sich
    // sofort ohne eingabe!" A click meant to focus that field (or land
    // anywhere else in the AKTUELL/ZIEL/ENTFERNUNG text below the ring)
    // could still be well outside `dead` but was never checked against
    // the ring's own outer edge either, so it silently counted as "aim
    // here" and rotated immediately -- before a single digit was typed.
    // A small margin (not exactly `radius`) still lets a click right at
    // the ring's own drawn edge register, matching how it visually looks
    // clickable.
    if (distance > radius * 1.1) {
        return -1.0;
    }
    return BeamHeading::wrap360(qRadiansToDegrees(std::atan2(p.x(), -p.y())));
}

void RotorWidget::drawPanelHeader(QPainter& painter) const
{
    const QRect headerRect(0, 0, width(), kHeaderHeight);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);

    QLinearGradient bg(0, 0, 0, kHeaderHeight);
    bg.setColorAt(0.0, QColor(Style::kPanelHeadTop()));
    bg.setColorAt(0.5, QColor(Style::kPanelHeadMid()));
    bg.setColorAt(1.0, QColor(Style::kPanelHeadBot()));
    painter.fillRect(headerRect, bg);

    // The accent bar -- see PanelHeaderBar.h's class comment for why
    // there is no "⠿" glyph beside it (mockup invention, not real
    // Longpath chrome; see ContainerWidget::buildTitleBar there).
    painter.fillRect(QRect(0, 0, kAccentBarWidth, kHeaderHeight), QColor(Style::kAmberText()));

    painter.setPen(QColor(Style::kTitleBorder()));
    painter.drawLine(0, kHeaderHeight - 1, width(), kHeaderHeight - 1);

    // Band-tag pill ("2m +23cm" / "70cm") removed -- operator, 2026-09-14,
    // pointing at the Rotor panel: "die markierung 2m und 23 und 70 cm
    // weglassen, kostet platz." The header now just carries the accent
    // bar and the turn-status badge below (still positioned from `x`,
    // kept at its old starting offset so that badge's own left-boundary
    // guard is unchanged).
    const int reservedRight = kOptionsButtonW + kOptionsButtonRightMargin + 4;
    const int x = kAccentBarWidth + 9;

    // Turn-status badge, right-aligned before the ⚙ button -- "DREHT"
    // while travelling towards a set target, "AUSGERICHTET" once within
    // arrival tolerance, nothing with no target set at all. Mirrors
    // Longpath's own Idle/Turning/OnTarget states (same file cited
    // above) without needing a full state machine here: derived
    // straight from the values this widget already has. Requires
    // m_connected OR m_simulated -- "DREHT" is a claim about the antenna
    // actually moving, which is only honest either with a live rotor
    // link or while startSimulatedTurn()'s own timer is genuinely
    // driving m_azimuthDeg toward the target (see its own comment) --
    // same "unbekannt zeigt nichts falsches an" rule this codebase
    // applies everywhere else (HAUSSTIL rule 7).
    if (m_hasTarget && (m_connected || m_simulated)) {
        const double rawDiff = std::fmod(std::abs(m_azimuthDeg - m_targetBearingDeg), 360.0);
        const double angleDiff = std::min(rawDiff, 360.0 - rawDiff);
        const bool onTarget = angleDiff <= kArrivalToleranceDeg;

        const QString badgeText = onTarget ? QStringLiteral("AUSGERICHTET") : QStringLiteral("DREHT");
        const QColor badgeBg{onTarget ? Style::kGreenBg() : Style::kBadgeWarnBg()};
        const QColor badgeBorder{onTarget ? Style::kGreenBorder() : Style::kAmberWarn()};
        const QColor badgeText_{onTarget ? Style::kGreenText() : Style::kAmberWarn()};

        const QFont badgeFont = Style::capsFont(painter.font());
        painter.setFont(badgeFont);
        const QFontMetrics bfm(badgeFont);
        constexpr int kDotSize = 5;
        constexpr int kBadgeH = 16;
        constexpr int kBadgePadX = 8;
        constexpr int kBadgeGap = 5;
        const int textW = bfm.horizontalAdvance(badgeText);
        const int badgeW = kBadgePadX * 2 + kDotSize + kBadgeGap + textW;
        const int badgeLeft = width() - reservedRight - badgeW;
        if (badgeLeft > x + 4) {
            const QRect badgeRect(badgeLeft, (kHeaderHeight - kBadgeH) / 2, badgeW, kBadgeH);
            painter.setPen(QPen(badgeBorder, 1.0));
            painter.setBrush(badgeBg);
            painter.drawRoundedRect(badgeRect, 4.0, 4.0);

            painter.setPen(Qt::NoPen);
            painter.setBrush(badgeText_);
            const QPointF dotCenter(badgeRect.left() + kBadgePadX + kDotSize / 2.0, badgeRect.center().y());
            painter.drawEllipse(dotCenter, kDotSize / 2.0, kDotSize / 2.0);

            painter.setPen(badgeText_);
            const QRect textRect(badgeRect.left() + kBadgePadX + kDotSize + kBadgeGap, badgeRect.top(), textW,
                                  badgeRect.height());
            painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, badgeText);
        }
    }

    painter.restore();
}

void RotorWidget::updateOptionsButtonGeometry()
{
    if (!m_optionsButton) {
        return;
    }
    const int x = std::max(0, width() - kOptionsButtonW - kOptionsButtonRightMargin);
    const int y = (kHeaderHeight - kOptionsButtonH) / 2;
    m_optionsButton->move(x, y);
}

void RotorWidget::updateTargetInputGeometry()
{
    if (!m_targetInput) {
        return;
    }
    if (m_dialStyle == RotorDialStyle::Digital) {
        // drawDigitalReadout() has its own separate ZIEL glass panel,
        // with no equivalent input slot wired up yet -- hidden rather
        // than mispositioned over unrelated content.
        m_targetInput->hide();
        return;
    }
    if (textAreaHeight() == 0) {
        // Too low for the readout: no cell to sit on (see
        // textAreaHeight()).
        m_targetInput->hide();
        return;
    }
    // The ZIEL cell's own value row -- the same readoutValuesRow()/
    // columnRect(..., 1) drawReadout() paints, so the two can never
    // drift apart.
    const QRect col1 = columnRect(readoutValuesRow(), 1);
    if (m_secondAntennaEnabled) {
        // Top half only -- the bottom half is drawReadout()'s own
        // read-only "2  <target+offset>" line (Antenna 2's target is
        // never independently settable, both antennas share one mast/
        // rotor -- see that code's own comment). Smaller font to match
        // that painted line's own kFontBody, since kFontDisplay's 38px
        // no longer fits half the row.
        m_targetInput->setGeometry(col1.left(), col1.top(), col1.width(), col1.height() / 2);
        m_targetInput->setFont(Style::monoFont(font(), Style::kFontBody, QFont::Bold));
    } else {
        m_targetInput->setGeometry(col1);
        m_targetInput->setFont(Style::monoFont(font(), readoutValueFontPx(), QFont::Bold));
    }
    m_targetInput->show();
}

void RotorWidget::syncTargetInputText()
{
    if (!m_targetInput || m_targetInput->hasFocus()) {
        // Never stomp on a keystroke in progress -- see this method's
        // own doc comment in the header.
        return;
    }
    m_targetInput->setText(m_hasTarget ? QString::number(qRound(BeamHeading::wrap360(m_targetBearingDeg)))
                                        : QString());
}

void RotorWidget::showOptionsPopup()
{
    if (!m_optionsButton) {
        return;
    }

    // Heap-allocated + WA_DeleteOnClose, shown via popup() rather than
    // the blocking exec() -- matches the real Longpath TxApplet::
    // showFinePopup() precedent, whose own gear-triggered popup is a
    // QWidget(Qt::Popup) opened with show() (non-blocking), not a
    // modal exec() loop. The menu deletes itself once it closes,
    // whichever way that happens (an entry picked, or the operator
    // clicking elsewhere).
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // Four entries offering RotorWidget's selectable dial styles (core/
    // RotorDialStyle.h) directly from this panel's own header. Labels
    // copied verbatim from SettingsDialog's own "Rotor-Anzeige" combo
    // (ui/SettingsDialog.cpp) so the same style reads identically in
    // both places -- that combo is left in place for discoverability
    // rather than removed, see the class comment.
    const auto addStyleAction = [this, menu](RotorDialStyle style, const QString& label) {
        QAction* action = menu->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_dialStyle == style);
        connect(action, &QAction::triggered, this, [this, style]() {
            // Applies to THIS widget immediately -- same "act on
            // yourself, then tell the owner" order PanelHeaderBar's own
            // lock button already uses -- so the popup works correctly
            // even before MainWindow's round trip (see this signal's
            // own comment) comes back around.
            setDialStyle(style);
            emit dialStyleRequested(style);
        });
    };
    addStyleAction(RotorDialStyle::FullCompass, QStringLiteral("Kompass (360°)"));
    addStyleAction(RotorDialStyle::LinearScale, QStringLiteral("Skala (linear)"));
    addStyleAction(RotorDialStyle::PartialArc, QStringLiteral("Rotor-Box (Bogen)"));
    addStyleAction(RotorDialStyle::Digital, QStringLiteral("Digital (Zahlen)"));

    // Below the button, right-aligned with it -- the same positioning
    // rule the real Longpath TxApplet::showFinePopup() uses for its own
    // gear-triggered popup ("Unter dem Zahnrad, aber rechtsbündig mit
    // ihm"), so a popup near this panel's right edge never hangs off
    // the screen.
    const QSize menuSize = menu->sizeHint();
    const QPoint below = m_optionsButton->mapToGlobal(
        QPoint(m_optionsButton->width() - menuSize.width(), m_optionsButton->height() + 4));
    menu->popup(below);
}

void RotorWidget::drawGlow(QPainter& painter, const QPointF& center, double radius) const
{
    // Ported 1:1 from Longpath's own rotor dial (gui/widgets/
    // RotorDialWidget.cpp::paintFace()) -- operator, 2026-09-14: "bitte
    // 1:1 die gleiche grafik und layout des rotors von longpath
    // übernehmen." Replaces the borrowed S-Meter/SWR "Bogeninstrumente
    // glimmen" olive glow (HAUSSTIL's general instrument rule) with
    // Longpath's own two-gradient rotor treatment, which HAUSSTIL's
    // general rule was itself superseded by there: a faint accent wash
    // rising from below the whole panel face, plus a warm amber "lamp"
    // behind the rose itself -- "von hinten leicht beleuchten", lighting
    // the whole disc rather than just the needle.
    const double w = width();
    const double h = height();
    QRadialGradient wash(QPointF(w * 0.5, h * 1.10), w * 0.85);
    QColor washInner{Style::kBlueBg()};
    washInner.setAlpha(26);
    wash.setColorAt(0.0, washInner);
    wash.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(wash);
    painter.drawRect(rect());

    QRadialGradient lamp(center, radius * 1.05);
    QColor warm{Style::kAmberText()};
    warm.setAlphaF(0.12);
    lamp.setColorAt(0.0, warm);
    warm.setAlphaF(0.0);
    lamp.setColorAt(1.0, warm);
    painter.setBrush(lamp);
    painter.drawEllipse(center, radius * 1.05, radius * 1.05);
    painter.setBrush(Qt::NoBrush);
}

void RotorWidget::drawTicks(QPainter& painter, const QPointF& center, double radius) const
{
    // Ported 1:1 from Longpath's own rotor dial (gui/widgets/
    // RotorDialWidget.cpp::paintFace()) -- operator, 2026-09-14: "bitte
    // 1:1 die gleiche grafik und layout des rotors von longpath
    // übernehmen." Three graduation tiers -- short every 10 deg, longer
    // every 30, longest+brightest on the four cardinals -- unchanged in
    // spirit from this file's earlier port, but the tick length is now a
    // FRACTION of the dial radius (0.86/0.90/0.94, as Longpath computes
    // it) rather than a flat pixel inset, so the graduation scales
    // proportionally at any panel size instead of a fixed 6-14px bite
    // out of the rim. Major and minor also now share Longpath's single
    // kTextScale colour (only the cardinal tier gets its own, darker
    // kTextSecondary/kMuted) -- the previous alpha-stepped three-colour
    // scheme was this file's own invention, not what Longpath draws.
    for (int deg = 0; deg < 360; deg += 10) {
        const bool cardinal = (deg % 90 == 0);
        const bool major = (deg % 30 == 0);
        const double innerRadius = radius * (cardinal ? 0.86 : major ? 0.90 : 0.94);
        const QPointF outer = pointOnCircle(center, radius, deg);
        const QPointF inner = pointOnCircle(center, innerRadius, deg);
        QPen pen(cardinal ? QColor(Style::kTextSecondary()) : QColor(Style::kTextScale()));
        pen.setWidthF(cardinal ? 1.5 : major ? 1.1 : 0.7);
        painter.setPen(pen);
        painter.drawLine(inner, outer);
    }

    // ── Himmelsrichtungen und Gradzahlen INNEN, im selben Ring ────────
    //
    // Longpath moved both from outside the rim (radius+11/+14) to a
    // shared radius*0.755 INSIDE the ring -- one labelling ring instead
    // of two, and the rose gains the outer 11-14px back for the ticks
    // themselves. See RotorDialWidget.cpp's own comment on this move
    // ("die Rose gewinnt aussen 11 px").
    drawCompassLabel(painter, center, radius * 0.755, 0.0, QStringLiteral("N"));
    drawCompassLabel(painter, center, radius * 0.755, 90.0, QStringLiteral("E"));
    drawCompassLabel(painter, center, radius * 0.755, 180.0, QStringLiteral("S"));
    drawCompassLabel(painter, center, radius * 0.755, 270.0, QStringLiteral("W"));

    // Intercardinal degree numbers -- every 30 deg except the four
    // cardinals, which already carry a letter. Longpath only draws these
    // once the dial is big enough that they would not collide (r > 84);
    // below that it skips them entirely rather than crowding the face.
    if (radius > 84.0) {
        for (int deg = 30; deg < 360; deg += 30) {
            if (deg % 90 == 0) {
                continue;
            }
            drawDegreeLabel(painter, center, radius * 0.755, deg);
        }
    }
}

void RotorWidget::drawCompassLabel(QPainter& painter, const QPointF& center, double radius, double angleDeg, const QString& text) const
{
    const QPointF p = pointOnCircle(center, radius, angleDeg);
    // Normal weight, not bold -- Longpath's own cardinal letters
    // (paintFace()'s `cf`) are the same monoFont as the tick labels
    // around them, undistinguished by weight.
    const QFont f = Style::monoFont(painter.font(), Style::kFontSmall);
    painter.setFont(f);
    const QFontMetrics fm(f);
    const QRectF box(p.x() - fm.horizontalAdvance(text), p.y() - fm.height(), fm.horizontalAdvance(text) * 2.0, fm.height() * 2.0);
    painter.setPen(QColor(Style::kTextScale()));
    painter.drawText(box, Qt::AlignCenter, text);
}

void RotorWidget::drawDegreeLabel(QPainter& painter, const QPointF& center, double radius, double angleDeg) const
{
    const QString text = QString::number(static_cast<int>(angleDeg));
    const QPointF p = pointOnCircle(center, radius, angleDeg);
    const QFont f = Style::monoFont(painter.font(), Style::kFontCaption);
    painter.setFont(f);
    const QFontMetrics fm(f);
    const QRectF box(p.x() - fm.horizontalAdvance(text), p.y() - fm.height() * 0.5, fm.horizontalAdvance(text) * 2.0, fm.height());
    // alpha 150, matching Longpath's degInk -- a hair quieter than the
    // cardinal letters so N/E/S/W still read as the primary labels.
    QColor ink{Style::kTextScale()};
    ink.setAlpha(150);
    painter.setPen(ink);
    painter.drawText(box, Qt::AlignCenter, text);
}

bool RotorWidget::isInMechanicalStopZone() const
{
    if (m_dialStyle != RotorDialStyle::PartialArc) {
        return false;
    }
    const double deg = std::fmod(std::fmod(m_azimuthDeg, 360.0) + 360.0, 360.0);
    return deg > kArcGapStartDeg && deg < kArcGapEndDeg;
}

void RotorWidget::setTerrainSectors(const QVector<LineOfSightClass>& sectorsByDegree)
{
    m_terrainSectors = sectorsByDegree;
    update();
}

void RotorWidget::drawNeedle(QPainter& painter, const QPointF& center, double radius, double angleDeg,
                              Qt::PenStyle style, int alpha, bool blocked) const
{
    // Amber, not cream -- HAUSSTIL.md carries an older "Zeiger und
    // Teilung cremeweiss, nie farbig" line, but that line is itself
    // superseded a few paragraphs later by the document's current rule:
    // "Blau = anfassbar. Warm = gemessen." A rotor heading is a MEASURED
    // value, the same category as an S-meter or SWR needle, so it takes
    // the same warm/amber ink those do. Confirmed against Longpath's own
    // real rotor widget (gui/widgets/RotorDialWidget.cpp), which made
    // exactly this correction on 2026-08-20 after first shipping with a
    // grey needle: "Die Richtung, in die die Antenne zeigt, IST eine
    // Messung. Sie bekommt dieselbe Farbe wie jede andere." `alpha` dims
    // a stale/unknown reading (rotor not connected) rather than hiding
    // the needle outright.
    //
    // `blocked`: the one exception. A heading actually sitting inside
    // the mechanical no-go zone (see isInMechanicalStopZone()) is not a
    // normal measurement any more -- it is a fault/manual-override the
    // operator needs to notice immediately, so it takes HAUSSTIL's
    // reserved warning red instead (Rotor-C.dc.html's "SPERRZONE"
    // state), same red family as CONTESTPROGRAMM's other genuine
    // warnings (Style::kRedText()), not the desaturated everyday amber.
    QColor color{blocked ? Style::kRedText() : Style::kAmberText()};
    color.setAlpha(alpha);

    const QPointF dir = pointOnCircle(center, 1.0, angleDeg) - center;
    const QPointF nrm = pointOnCircle(center, 1.0, angleDeg + 90.0) - center;
    const QPointF tip = center + dir * (radius * 0.90);

    if (style == Qt::DashLine) {
        if (m_dialStyle == RotorDialStyle::PartialArc) {
            // Bogen-Präzision: the second antenna's needle is a dotted
            // instrument-face line ending in a small hollow ring-edge
            // circle, replacing the dashed/amber treatment the other
            // styles use below -- per Design3.dc.html ("Option 3"),
            // whose own dash pattern (1,3.4, vs. the dashed style's own
            // 2.2,1.6) and cream stroke (var(--inst-face), not
            // var(--amber-text)) are a deliberate quieter, secondary-
            // hierarchy mark: this style's whole point is that the
            // mechanical gap/stop is the loud element, so the auxiliary
            // second antenna recedes rather than competing with it. No
            // halo underneath (Design3.dc.html's own SVG has none for
            // this line, unlike the dashed style below). `blocked`
            // still overrides to warning red exactly like the primary
            // needle, when THIS needle's own bearing sits in the gap
            // (see drawNeedlesAndTarget()'s per-needle inGap() check).
            // The hollow circle sits at the ring intersection (the same
            // `tip` point the numbered ring-edge mark's own angle is
            // computed from), IN ADDITION TO drawNumberedMark()'s own
            // mark further out at radius+22 -- both coexist, matching
            // Design3.dc.html's own <circle ... fill="none"
            // stroke="var(--inst-face)"/> alongside its numbered mark.
            QColor dotColor{blocked ? Style::kRedText() : Style::kInstrumentFace()};
            dotColor.setAlpha(alpha);
            QPen dotPen(dotColor, 2.0, Qt::DashLine, Qt::RoundCap);
            dotPen.setDashPattern({1.0, 3.4});
            painter.setPen(dotPen);
            painter.drawLine(center, tip);

            painter.setPen(QPen(dotColor, 1.6));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(tip, 4.0, 4.0);
            return;
        }

        // Second antenna (FullCompass/Digital): a halo'd dashed line
        // rather than a tapered body -- same distinction as before
        // (solid vs. dashed marks which needle is which), ported
        // alongside the primary needle's new shape from Longpath's own
        // drawNeedle() (same file cited above).
        QColor halo = color;
        halo.setAlpha(std::min(alpha, 70));
        QPen haloPen(halo, 5.1, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(haloPen);
        painter.drawLine(center, tip);

        QPen pen(color, 2.5, Qt::DashLine, Qt::RoundCap);
        pen.setDashPattern({2.2, 1.6});
        painter.setPen(pen);
        painter.drawLine(center, tip);
        return;
    }

    // Tapered instrument-needle body with a short tail/counterweight
    // behind the pivot -- ported from Longpath's real drawNeedle(): the
    // taper marks which end is the reading, the tail makes the pivot
    // read as an axis rather than a line's loose end. A soft halo
    // underneath (same idea as drawGlow(), just needle-shaped) keeps it
    // legible against the ring's own ticks.
    const double halfW = radius * kNeedleHalfWidthFraction;
    const double tailLen = radius * 0.16;
    const QPointF tail = center - dir * tailLen;

    QPolygonF body;
    body << tip
         << center + nrm * halfW
         << tail + nrm * (halfW * 0.7)
         << tail - nrm * (halfW * 0.7)
         << center - nrm * halfW;

    QColor halo = color;
    halo.setAlpha(std::min(alpha, 60));
    QPen haloPen(halo, 3.0, Qt::SolidLine, Qt::RoundCap);
    haloPen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(haloPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPolygon(body);

    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPolygon(body);
    painter.setBrush(Qt::NoBrush);
}

void RotorWidget::drawTargetMarker(QPainter& painter, const QPointF& center, double radius, double angleDeg) const
{
    // Blue = anfassbar/commanded -- HAUSSTIL.md's colour-meaning table
    // names a target-bearing marker as its own example of this rule.
    const QPointF tip = pointOnCircle(center, radius, angleDeg);
    const QPointF back1 = pointOnCircle(center, radius + 10.0, angleDeg - 6.0);
    const QPointF back2 = pointOnCircle(center, radius + 10.0, angleDeg + 6.0);
    QPolygonF marker;
    marker << tip << back1 << back2;
    painter.setBrush(QColor(Style::kBlueBg()));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(marker);
}

void RotorWidget::drawBeamCone(QPainter& painter, const QPointF& center, double radius, double angleDeg,
                               const QColor& color, int alpha) const
{
    // The half-power beamwidth as a wedge under the needle -- the
    // colour at the hub, nothing at the ring, so the needle stays the
    // reading and the wedge only says how wide the beam is (design
    // sheet "Rotoren: Kegel"). QPainterPath::arcTo() counts degrees
    // counter-clockwise from three o'clock; a bearing runs clockwise
    // from twelve.
    const double half = m_beamwidthDeg / 2.0;
    QPainterPath wedge;
    wedge.moveTo(center);
    wedge.arcTo(QRectF(center.x() - radius, center.y() - radius, 2.0 * radius, 2.0 * radius),
                90.0 - (angleDeg - half), -m_beamwidthDeg);
    wedge.closeSubpath();

    QRadialGradient fade(center, radius);
    QColor hub = color;
    hub.setAlpha(120 * alpha / 255);
    QColor rim = color;
    rim.setAlpha(14 * alpha / 255);
    fade.setColorAt(0.0, hub);
    fade.setColorAt(1.0, rim);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fade);
    painter.drawPath(wedge);

    // Faint edges, so the width still reads where the fill has faded.
    QColor edge = color;
    edge.setAlpha(90 * alpha / 255);
    painter.setPen(QPen(edge, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(center, pointOnCircle(center, radius, angleDeg - half));
    painter.drawLine(center, pointOnCircle(center, radius, angleDeg + half));
}

void RotorWidget::drawLinearBeamBand(QPainter& painter, double trackLeft, double trackWidth, double trackY,
                                     double angleDeg, const QColor& color, int alpha) const
{
    // The polar cone's linear-track equivalent: a translucent band a
    // beamwidth wide around the handle, in two pieces when it wraps
    // past the track's 0/360 ends.
    QColor fill = color;
    fill.setAlpha(55 * alpha / 255);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    const double half = m_beamwidthDeg / 2.0;
    const double bandHeight = kLinearTrackHeightPx + 8.0;
    const auto band = [&](double fromDeg, double toDeg) {
        const double left = trackLeft + fromDeg / 360.0 * trackWidth;
        const double right = trackLeft + toDeg / 360.0 * trackWidth;
        painter.drawRoundedRect(QRectF(left, trackY - bandHeight / 2.0, right - left, bandHeight), 3.0, 3.0);
    };
    const double from = BeamHeading::wrap360(angleDeg - half);
    const double to = BeamHeading::wrap360(angleDeg + half);
    if (from <= to) {
        band(from, to);
    } else {
        band(from, 360.0);
        band(0.0, to);
    }
    painter.setBrush(Qt::NoBrush);
}

QRect RotorWidget::readoutBlockRect() const
{
    const int textHeight = textAreaHeight();
    const QRect textArea(0, height() - textHeight, width(), textHeight);
    return QRect(textArea.left() + 6, textArea.top() + kReadoutTopPad, textArea.width() - 12,
                 kReadoutLabelHeight + kReadoutValueHeight);
}

QRect RotorWidget::readoutValuesRow() const
{
    // Below the cell's label, ending a little above the glass's bottom
    // edge.
    const QRect block = readoutBlockRect();
    return QRect(block.left(), block.top() + 3 + kReadoutLabelHeight, block.width(), kReadoutValueHeight - 6);
}

int RotorWidget::readoutValueFontPx() const
{
    const int cellWidth = columnRect(readoutBlockRect(), 0).width() - 3;
    const QFontMetrics big(Style::monoFont(font(), Style::kFontDisplay));
    return big.horizontalAdvance(QStringLiteral("000°")) + 10 <= cellWidth ? Style::kFontDisplay
                                                                             : kDigitalValueFontPx;
}

void RotorWidget::drawReadoutInset(QPainter& painter, const QRect& box) const
{
    painter.setPen(QPen(QColor(Style::kBorderSubtle()), 1.0));
    painter.setBrush(QColor(Style::kInsetBg()));
    painter.drawRoundedRect(box, 6.0, 6.0);
    painter.save();
    painter.setClipRect(box);
    QLinearGradient insetShadow(0, box.top(), 0, box.top() + 10.0);
    insetShadow.setColorAt(0.0, QColor(0, 0, 0, 140));
    insetShadow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(box, insetShadow);
    painter.restore();
    painter.setBrush(Qt::NoBrush);
}

void RotorWidget::drawNumberedMark(QPainter& painter, const QPointF& center, double radius, double angleDeg,
                                    const QString& number) const
{
    // Positioned outside the tick ring, computed from the needle's own
    // angle so it rotates with it -- per the plan's "Randmarkierung je
    // Nadel" note. Both antennas' marks share the same instrument-face
    // colour (no terrain classification exists yet to justify a
    // per-needle hue -- see the class comment's deliberate scope cut);
    // only the digit tells them apart.
    const QPointF p = pointOnCircle(center, radius + 22.0, angleDeg);
    drawNumberedMarkAt(painter, p, number);
}

void RotorWidget::drawNumberedMarkAt(QPainter& painter, const QPointF& pos, const QString& number) const
{
    // The actual ring+digit drawing, factored out of the polar
    // drawNumberedMark() above so LinearScale's own (non-polar) marks
    // can share it too -- see drawLinearHandle().
    constexpr double kMarkRadius = 8.0;
    const QColor color{Style::kInstrumentFace()};
    painter.setBrush(QColor(Style::kPanelBg()));
    QPen pen(color);
    pen.setWidthF(1.5);
    painter.setPen(pen);
    painter.drawEllipse(pos, kMarkRadius, kMarkRadius);
    painter.setPen(color);
    painter.setFont(Style::monoFont(painter.font(), Style::kFontCaption, QFont::Bold));
    painter.drawText(QRectF(pos.x() - kMarkRadius, pos.y() - kMarkRadius, kMarkRadius * 2.0, kMarkRadius * 2.0),
                      Qt::AlignCenter, number);
}

void RotorWidget::drawReadout(QPainter& painter, const QRect& area) const
{
    // Three-column AKTUELL/ZIEL/ENTFERNUNG block, per the design mockup
    // (Rotor-A.dc.html/Rotor-Dual.dc.html) that prompted this pass --
    // replaces the old single "AZ <value>" line. Column 0 (current
    // heading) reflects m_connected the same way the old AZ line did;
    // columns 1/2 (target bearing, distance) reflect m_hasTarget --
    // HAUSSTIL rule 7 ("Unbekannt ist ein Strich, keine Null"): a dash,
    // never a fabricated 0/000.
    const QFont capLabelFont = Style::capsFont(painter.font());
    const QFont valueFont = Style::monoFont(painter.font(), readoutValueFontPx());
    const QFont unitFont = Style::monoFont(painter.font(), Style::kFontSmall);

    const QColor scaleColor{Style::kTextScale()};
    const QColor inactiveColor{Style::kTextInactive()};
    // Current-heading number: the same amber drawNeedle() now paints the
    // actual needle in -- a rotor heading is a MEASURED value, and
    // HAUSSTIL.md's current rule is "Blau = anfassbar. Warm = gemessen."
    // (an older "Zeiger cremeweiss" line in the same document is
    // superseded by this one; confirmed against Longpath's own real
    // rotor widget, gui/widgets/RotorDialWidget.cpp, which made this
    // exact correction on 2026-08-20). Needle's own colour, not a
    // re-invented one.
    const QColor instrumentColor{Style::kAmberText()};
    const QColor primaryColor{Style::kTextPrimary()};
    const QColor tertiaryColor{Style::kTextTertiary()};

    int y = area.top() + kReadoutTopPad;

    // The three cells first, as sunken glass with the label inside at
    // the top and the number below it (design sheet "Rotoren: Kegel",
    // 2026-09-21) -- replaces the earlier bare columns with divider
    // lines. Geometry from readoutBlockRect()/readoutValuesRow(), which
    // updateTargetInputGeometry() shares so the editable ZIEL field
    // sits exactly on its cell.
    const QRect block = readoutBlockRect();
    for (int column = 0; column < 3; ++column) {
        drawReadoutInset(painter,
                         columnRect(block, column).adjusted(column == 0 ? 0 : 3, 0, column == 2 ? 0 : -3, 0));
    }

    // Row 1: caps labels, one per column -- same "small caps label
    // above a value" convention UnifiedLogWidget::buildFieldCell() and
    // MapWidget::drawLegend() both already use.
    const QRect labelsRow(block.left(), block.top() + 3, block.width(), kReadoutLabelHeight);
    painter.setFont(capLabelFont);
    painter.setPen(scaleColor);
    painter.drawText(columnRect(labelsRow, 0), Qt::AlignCenter, QStringLiteral("Aktuell"));
    painter.drawText(columnRect(labelsRow, 1), Qt::AlignCenter, QStringLiteral("Ziel"));
    painter.drawText(columnRect(labelsRow, 2), Qt::AlignCenter, QStringLiteral("Entfernung"));
    y += kReadoutLabelHeight;

    // Row 2: the big numbers themselves.
    const QRect valuesRow = readoutValuesRow();

    // Zero-padded to 3 digits (mockup: "072°", "060°") -- the old AZ
    // line never zero-padded, but a fixed-width column reads much
    // better without the digit count jumping around as the rotor turns.
    const auto formatDeg = [](double deg) {
        return QStringLiteral("%1°").arg(deg, 3, 'f', 0, QLatin1Char('0'));
    };

    {
        // Same warning-red override as the needle/caption when the
        // heading sits inside the mechanical no-go zone -- terrain data
        // is deliberately NOT part of this: it is a pure informational
        // marker on the ring itself (drawTerrainSectorWash()), never a
        // restriction, and must never make the rotor's own live
        // readout look like it is locked out of a direction (operator,
        // 2026-09-12: "rotor soll nicht blockiert werden, nur markiert
        // werden, welche richtung nicht gut ist, keine sperre!!!").
        // m_simulated shows the same number as a real reading would, but
        // in the disconnected ring's own kAmberWarn instead of the
        // normal measured colour -- readable, but never mistakeable for
        // a real position (see m_simulated's own doc comment).
        const bool showAktuell = m_connected || m_simulated;
        const QColor aktuellColor = isInMechanicalStopZone() ? QColor(Style::kRedText())
                                   : m_connected              ? instrumentColor
                                                               : QColor(Style::kAmberWarn());
        const QRect col0 = columnRect(valuesRow, 0);
        if (m_secondAntennaEnabled) {
            // Two stacked rows, "1"/"2" prefixed -- operator, 2026-09-14:
            // "aktuell und ziel sollten 2 zeilen sein, antenne 1 und
            // antenne 2". Antenna 2's own actual heading
            // (secondAntennaBearing(), the same fixed-offset formula the
            // dial's own second needle/numbered mark already use) had no
            // numeric readout of its own before this -- only the needle
            // itself showed it. A smaller font than the single-antenna
            // case's kFontDisplay -- two lines share the same
            // kReadoutValueHeight the one line used to have alone.
            const QRect aRow1(col0.left(), col0.top(), col0.width(), col0.height() / 2);
            const QRect aRow2(col0.left(), col0.top() + col0.height() / 2, col0.width(), col0.height() - col0.height() / 2);
            painter.setFont(Style::monoFont(painter.font(), Style::kFontBody, QFont::Bold));
            painter.setPen(showAktuell ? aktuellColor : inactiveColor);
            painter.drawText(aRow1, Qt::AlignCenter,
                              QStringLiteral("1  %1").arg(showAktuell ? formatDeg(m_azimuthDeg) : Style::unknownDash()));
            painter.drawText(aRow2, Qt::AlignCenter,
                              QStringLiteral("2  %1").arg(showAktuell
                                  ? formatDeg(secondAntennaBearing(m_azimuthDeg, m_secondAntennaOffsetDeg))
                                  : Style::unknownDash()));
        } else {
            QVector<TextSegment> segs;
            segs << TextSegment{showAktuell ? formatDeg(m_azimuthDeg) : Style::unknownDash(), valueFont,
                                 showAktuell ? aktuellColor : inactiveColor};
            drawCenteredSegments(painter, col0, segs);
        }
    }
    // ZIEL's own ANTENNA-1 value is no longer painted here -- m_targetInput
    // (a real, editable QLineEdit, see updateTargetInputGeometry()) sits
    // over it instead, showing/editing the same m_targetBearingDeg
    // directly (operator, 2026-09-14: "wenn ich dort im ziel meine
    // zielrichtung eingebe ... sollte automatisch der zeiger drehen").
    // Antenna 2's own ZIEL (target + the fixed offset, never independently
    // settable -- both antennas share one mast/rotor) still IS painted
    // here, in the input's own lower half, when the second antenna is on
    // (see updateTargetInputGeometry()'s own comment for why the input
    // itself shrinks to the top half in that case).
    if (m_secondAntennaEnabled) {
        const QRect col1 = columnRect(valuesRow, 1);
        const QRect row2(col1.left(), col1.top() + col1.height() / 2, col1.width(), col1.height() - col1.height() / 2);
        const QString secondZiel = m_hasTarget
            ? formatDeg(BeamHeading::wrap360(m_targetBearingDeg + m_secondAntennaOffsetDeg))
            : Style::unknownDash();
        painter.setFont(Style::monoFont(painter.font(), Style::kFontBody, QFont::Bold));
        painter.setPen(m_hasTarget ? QColor(Style::kBlueBg()) : inactiveColor);
        painter.drawText(row2, Qt::AlignCenter, QStringLiteral("2  %1").arg(secondZiel));
    }
    {
        QVector<TextSegment> segs;
        // Negative distanceKm is the "no known distance" sentinel a
        // manual dial click sets (see mousePressEvent()/setTargetBearing()'s
        // own comment) -- a clicked bearing has no station to measure
        // distance to, so this column reads as unknown too rather than a
        // fabricated "0 km" or "-1 km".
        if (m_hasTarget && m_targetDistanceKm >= 0.0) {
            // Smaller "km" suffix beside the big number -- mirrors the
            // mockup's own two-size treatment of this one value
            // (Rotor-A.dc.html: "471<span style=...font-size:12px>
            // km</span>"), mapped onto this ladder's kFontSmall rather
            // than the mockup's literal 12px.
            segs << TextSegment{QStringLiteral("%1").arg(m_targetDistanceKm, 0, 'f', 0), valueFont, primaryColor};
            segs << TextSegment{QStringLiteral(" km"), unitFont, tertiaryColor};
        } else {
            segs << TextSegment{Style::unknownDash(), valueFont, inactiveColor};
        }
        drawCenteredSegments(painter, columnRect(valuesRow, 2), segs);
    }

    y += kReadoutValueHeight + kReadoutRowGap;

    // A reduced block (see textAreaHeight()) ends here, or after the
    // caption.
    const bool hasCaptionRow = area.height() >= kTextAreaHeight - (kReadoutRowGap + kReadoutLineHeight);
    const bool hasStatusRow = area.height() >= kTextAreaHeight;
    if (!hasCaptionRow) {
        return;
    }

    // Row 3: target-station caption ("SP9XYZ · JO90 · 471 km") -- only
    // drawn with real content when a target is actually set AND its
    // station identity is known (never a fabricated placeholder); when
    // there is genuinely no target at all, this row shows the same
    // unknown-dash the ZIEL/ENTFERNUNG columns above just showed, rather
    // than silently vanishing and leaving the block looking unfinished.
    // Logic lives in computeCaptionLine() so drawDigitalReadout()'s own
    // caption row shows exactly the same message for the exact same
    // state.
    {
        QString captionText;
        QColor captionColor;
        bool showCaption = false;
        computeCaptionLine(captionText, captionColor, showCaption);
        if (showCaption) {
            painter.setFont(capLabelFont);
            painter.setPen(captionColor);
            painter.drawText(QRect(area.left(), y, area.width(), kReadoutLineHeight), Qt::AlignCenter, captionText);
        }
    }
    y += kReadoutLineHeight + kReadoutRowGap;
    if (!hasStatusRow) {
        return;
    }

    // Row 4: connection status as a small badge dot + caps text --
    // unchanged from before this pass, matching the confirmed/off badge
    // colour pair used in the status bar (kGreenText / kTextInactive).
    // Factored into drawConnectionStatusRow() so drawDigitalReadout()'s
    // own status row matches exactly.
    drawConnectionStatusRow(painter, QRect(area.left(), y, area.width(), kReadoutLineHeight));
}

void RotorWidget::computeCaptionLine(QString& text, QColor& color, bool& show) const
{
    const QColor tertiaryColor{Style::kTextTertiary()};
    text.clear();
    color = tertiaryColor;
    show = false;

    if (isInMechanicalStopZone()) {
        // Warning row replaces the normal caption while the heading is
        // actually inside the no-go zone -- Rotor-C.dc.html's own
        // "SPERRZONE" line. Deliberately still NOT claiming anything
        // about terrain here: a confirmed-Blocked terrain sector is a
        // pure informational marker on the ring itself
        // (drawTerrainSectorWash()), never a restriction, and must
        // never make the rotor's own live caption read as if the
        // operator cannot or should not point there (operator,
        // 2026-09-12: "rotor soll nicht blockiert werden, nur markiert
        // werden, welche richtung nicht gut ist, keine sperre!!!" --
        // this reverses an earlier same-day version of this comment
        // that DID fold terrain into this warning row). The mockup's
        // "Δ <travel>°" prefix is folded onto the same line rather than
        // added as a genuinely new row, to stay inside this block's
        // existing fixed height.
        QString warning = QStringLiteral("⚠ SPERRZONE — WEITERDREHEN");
        if (m_hasTarget) {
            const double signedDiff = std::fmod(m_targetBearingDeg - m_azimuthDeg + 540.0, 360.0) - 180.0;
            warning += QStringLiteral(" · Δ %1°").arg(qRound(std::abs(signedDiff)));
        }
        text = warning;
        color = QColor(Style::kRedText());
        show = true;
    } else if (m_hasTarget) {
        if (!m_targetCallsign.isEmpty() || !m_targetGrid.isEmpty()) {
            QStringList parts;
            if (!m_targetCallsign.isEmpty()) {
                parts << m_targetCallsign;
            }
            if (!m_targetGrid.isEmpty()) {
                parts << m_targetGrid;
            }
            // See drawReadout()'s own comment -- negative is "no known
            // distance" (a manual dial click), so this trailing segment
            // is simply left off rather than printing "-1 km".
            if (m_targetDistanceKm >= 0.0) {
                parts << QStringLiteral("%1 km").arg(m_targetDistanceKm, 0, 'f', 0);
            }
            text = parts.join(QStringLiteral(" · "));
            show = true;
        }
        // else: a target bearing is set but no station identity came
        // with it -- leave this row blank rather than drawing a dash
        // that would misleadingly suggest the whole target is unknown
        // when the bearing/distance above are not.
    } else {
        text = Style::unknownDash();
        show = true;
    }
}

void RotorWidget::drawConnectionStatusRow(QPainter& painter, const QRect& row) const
{
    const QColor greenColor{Style::kGreenText()};
    const QColor inactiveColor{Style::kTextInactive()};
    const QFont capLabelFont = Style::capsFont(painter.font());

    const QColor dotColor = m_connected ? greenColor : inactiveColor;
    const QString statusText = m_connected ? QStringLiteral(" verbunden") : QStringLiteral(" getrennt");
    constexpr int kDotSize = 6;
    const QFontMetrics fm(capLabelFont);
    const int textWidth = fm.horizontalAdvance(statusText);
    const int totalWidth = kDotSize + textWidth;
    const int x = row.left() + (row.width() - totalWidth) / 2;
    const int rowCenterY = row.top() + row.height() / 2;

    painter.setPen(Qt::NoPen);
    painter.setBrush(dotColor);
    painter.drawEllipse(QPointF(x + kDotSize / 2.0, rowCenterY), kDotSize / 2.0, kDotSize / 2.0);

    painter.setFont(capLabelFont);
    painter.setPen(dotColor);
    painter.drawText(QRect(x + kDotSize, row.top(), textWidth, row.height()), Qt::AlignVCenter | Qt::AlignLeft, statusText);
}

void RotorWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Panel chrome: rounded background + border, clipped so everything
    // drawn afterwards (header, dial, readout) respects the rounded
    // corners -- HAUSSTIL: "Nie Radius 3", panel radius kPanelRadius.
    QPainterPath panelPath;
    panelPath.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Style::kPanelRadius, Style::kPanelRadius);
    painter.fillPath(panelPath, QColor(Style::kPanelBg()));
    painter.setPen(QPen(QColor(Style::kBorderSubtle()), 1.0));
    painter.drawPath(panelPath);
    painter.setClipPath(panelPath);

    drawPanelHeader(painter);

    const int dialAreaTop = kHeaderHeight;
    const int textHeight = textAreaHeight();
    const int dialAreaHeight = height() - kHeaderHeight - textHeight;

    // All four paint paths (RotorDialStyle, core/RotorDialStyle.h) share
    // this one fixed dial-area/text-area split -- Digital's ring is the
    // same size as the other three's (operator, 2026-09-11: "rotor
    // gleich groß wie die anderen"), only its readout block differs
    // (drawDigitalReadout() vs. the shared drawReadout(), picked below).
    switch (m_dialStyle) {
    case RotorDialStyle::LinearScale:
        paintLinearScaleDial(painter, QRect(0, dialAreaTop, width(), dialAreaHeight));
        break;
    case RotorDialStyle::PartialArc: {
        const QPointF center(width() / 2.0, dialAreaTop + dialAreaHeight / 2.0);
        // Extra headroom below the ring for the "SPERRZONE (ENDANSCHLAG)"
        // caption (drawArcGapMarkers() places it at radius+30, one
        // kFontCaption line tall). Without this the caption's bottom
        // edge lands ~20px past the dial area's own bottom edge -- right
        // where drawReadout()'s AKTUELL/ZIEL/ENTFERNUNG row paints next,
        // over it, so the one visually distinctive marker of this style
        // was silently invisible. FullCompass has no such caption and
        // keeps the plain radius. Flagged but left unfixed by an earlier
        // task's own report ("bei sehr kleiner Fenstergröße überlappt
        // sich ... die Beschriftung mit der neuen Anzeige-Zeile") --
        // this is that fix, now applied at every size, not just small
        // ones (the collision is a fixed pixel amount, not size-relative,
        // so it existed at every panel size, just more visible at small
        // ones where nothing else fills the gap).
        constexpr double kGapCaptionAllowancePx = 24.0;
        const double radius = (std::min(width(), dialAreaHeight) - kDialMargin * 2) / 2.0 - kGapCaptionAllowancePx;
        paintPartialArcDial(painter, center, radius);
        break;
    }
    case RotorDialStyle::Digital: {
        const QPointF center(width() / 2.0, dialAreaTop + dialAreaHeight / 2.0);
        const double radius = (std::min(width(), dialAreaHeight) - kDialMargin * 2) / 2.0;
        paintDigitalDial(painter, center, radius);
        break;
    }
    case RotorDialStyle::FullCompass:
    default: {
        const QPointF center(width() / 2.0, dialAreaTop + dialAreaHeight / 2.0);
        const double radius = (std::min(width(), dialAreaHeight) - kDialMargin * 2) / 2.0;
        paintFullCompassDial(painter, center, radius);
        break;
    }
    }

    if (textHeight == 0) {
        return; // too low for any readout: the dial alone
    }
    const QRect textArea(0, height() - textHeight, width(), textHeight);
    if (m_dialStyle == RotorDialStyle::Digital) {
        drawDigitalReadout(painter, textArea);
    } else {
        drawReadout(painter, textArea);
    }
}

void RotorWidget::paintFullCompassDial(QPainter& painter, const QPointF& center, double radius) const
{
    // Unchanged from RotorWidget's only rendering before this pass --
    // moved into its own method so paintEvent() can pick it via
    // m_dialStyle, but every drawing call and its order is identical.
    drawGlow(painter, center, radius);

    // Ported 1:1 from Longpath's own rotor dial -- two rings, not one:
    // an outer ring in the scale colour (kTextScale, matching the
    // graduation it frames, not the panel's own faint kBorderSubtle
    // edge) and an inner ring one step down (kBorder) at 0.83x radius.
    // When disconnected, the outer ring goes dashed amber-warn instead
    // -- the same "this reading is not real" marker Longpath's own dial
    // draws for a simulated/unconnected rotator, mapped onto this
    // widget's own m_connected flag.
    if (!m_connected) {
        QPen warnPen{QColor(Style::kAmberWarn())};
        warnPen.setWidthF(1.2);
        warnPen.setStyle(Qt::DashLine);
        painter.setPen(warnPen);
    } else {
        painter.setPen(QPen(QColor(Style::kTextScale()), 1.0));
    }
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius, radius);
    painter.setPen(QPen(QColor(Style::kBorder()), 1.0));
    painter.drawEllipse(center, radius * 0.83, radius * 0.83);

    drawTicks(painter, center, radius);

    drawNeedlesAndTarget(painter, center, radius);
}

void RotorWidget::drawTerrainSectorWash(QPainter& painter, const QPointF& center, double radius) const
{
    // Operator, 2026-09-14: "bitte den roten ring generell entfernen" --
    // removed from both paint paths below (paintFullCompassDial() and
    // paintPartialArcDial()). The ring is now 1:1 Longpath's own plain
    // scale-colour ring with nothing painted over it, regardless of what
    // TerrainDataManager::sectorSweep() reports. Left the method and
    // setTerrainSectors()/m_terrainSectors themselves in place rather
    // than deleting the whole terrain-wash mechanism -- MapWidget's own
    // equivalent wash is unaffected, and the sector data may still be
    // wanted elsewhere later; this call is just no longer wired into
    // either dial's paint path.
    // No-op until MainWindow actually has a sweep to give this widget
    // (see setTerrainSectors()'s own comment) -- not an error state,
    // just "Phase 2 terrain data not wired up (yet) for this instance".
    if (m_terrainSectors.size() != 360) {
        return;
    }

    // Same "loop a degree range, pointOnCircle(), build a QPainterPath,
    // stroke once" technique drawArcGapMarkers() already uses for its
    // own (fixed, mechanical-only) wash -- generalized here to however
    // many contiguous Blocked/Marginal runs m_terrainSectors actually
    // has, painted directly on the ring itself rather than just either
    // side of one fixed gap. A run that wraps past 359->0 degrees draws
    // as two adjacent arcs instead of one continuous one -- visually
    // identical, not worth the extra bookkeeping for a purely cosmetic
    // difference.
    // PartialArc has no ring inside the fixed mechanical gap
    // (kArcGapStartDeg..kArcGapEndDeg, see drawArcGapMarkers()'s own
    // scope note) -- painting a wash there would float a coloured arc
    // over nothing, so those degrees are skipped entirely for that
    // style, same as the gap itself already is for ticks/needles.
    const bool isArcStyle = (m_dialStyle == RotorDialStyle::PartialArc);
    const auto insideMechanicalGap = [](int deg) { return deg > kArcGapStartDeg && deg < kArcGapEndDeg; };

    int i = 0;
    while (i < 360) {
        const LineOfSightClass cls = m_terrainSectors.at(i);
        if ((cls != LineOfSightClass::Blocked && cls != LineOfSightClass::Marginal)
            || (isArcStyle && insideMechanicalGap(i))) {
            ++i;
            continue;
        }
        int j = i;
        while (j < 360 && m_terrainSectors.at(j) == cls && !(isArcStyle && insideMechanicalGap(j))) {
            ++j;
        }

        // kRedText is tuned for TEXT on the dark panel background (see
        // computeCaptionLine()'s own use) -- a pale near-white
        // (#f0dcdc) that reads fine as a warning word, but all but
        // disappears as a thin stroke ON TOP OF the ring itself, which
        // is drawn in a similarly light colour (operator, 2026-09-12,
        // from a live screenshot: the warning text showed correctly
        // but the ring wash itself was invisible). kRedBorder is the
        // named colour StyleKit.h already reserves for exactly this
        // "needs to read as a distinct coloured LINE, not text" role --
        // Marginal keeps kAmberWarn unchanged, the same amber
        // drawArcGapMarkers()'s own flanking ticks already use at a
        // similar stroke width and are visibly fine there.
        const QColor color{
            cls == LineOfSightClass::Blocked ? Style::kRedBorder() : Style::kAmberWarn()};
        QPen washPen(color);
        washPen.setWidthF(5.0);
        painter.setPen(washPen);
        painter.setBrush(Qt::NoBrush);

        QPainterPath path;
        bool first = true;
        for (int deg = i; deg < j; ++deg) {
            const QPointF p = pointOnCircle(center, radius, static_cast<double>(deg));
            if (first) {
                path.moveTo(p);
                first = false;
            } else {
                path.lineTo(p);
            }
        }
        painter.drawPath(path);
        i = j;
    }
}

void RotorWidget::drawNeedlesAndTarget(QPainter& painter, const QPointF& center, double radius) const
{
    // Shared by FullCompass and PartialArc -- identical to what used to
    // be paintEvent()'s tail end. Both styles draw from the same
    // center/radius; only the ring/ticks behind this differ.
    //
    // The beamwidth first, under everything else: a translucent wedge
    // around each needle, the antenna's half-power beamwidth wide
    // (design sheet "Rotoren: Kegel"); the second antenna's in the
    // map's own kAmberWarn so a stacked pair never reads as one beam.
    // Dimmed with the needles when the reading is stale.
    const int coneAlpha = m_connected ? 255 : 110;
    if (m_secondAntennaEnabled) {
        drawBeamCone(painter, center, radius, secondAntennaBearing(m_azimuthDeg, m_secondAntennaOffsetDeg),
                     QColor(Style::kAmberWarn()), coneAlpha);
    }
    drawBeamCone(painter, center, radius, m_azimuthDeg, QColor(Style::kAmberText()), coneAlpha);

    if (m_hasTarget) {
        drawTargetMarker(painter, center, radius, m_targetBearingDeg);
    }

    // Disconnected means the current heading below is a stale/unknown
    // reading (see drawReadout()) -- both needles fade together rather
    // than either vanishing or reading as a confident zero.
    const int needleAlpha = m_connected ? 255 : 110;
    const bool arcStyle = (m_dialStyle == RotorDialStyle::PartialArc);
    const auto inGap = [](double deg) {
        const double wrapped = std::fmod(std::fmod(deg, 360.0) + 360.0, 360.0);
        return wrapped > kArcGapStartDeg && wrapped < kArcGapEndDeg;
    };

    if (m_secondAntennaEnabled) {
        const double secondDeg = secondAntennaBearing(m_azimuthDeg, m_secondAntennaOffsetDeg);
        // Each needle judged on ITS OWN bearing, not the main needle's --
        // per the plan's own second-antenna note ("die zweite Nadel
        // bekommt ihre EIGENE Sperrzonen-Farbe ... nicht die der ersten
        // Nadel übernommen"). Mechanical gap only -- terrain data is
        // deliberately NOT folded in here, see drawNeedle()'s call just
        // below for the full reasoning.
        drawNeedle(painter, center, radius, secondDeg, Qt::DashLine, needleAlpha, arcStyle && inGap(secondDeg));
        drawNumberedMark(painter, center, radius, secondDeg, QStringLiteral("2"));
    }

    // Mechanical gap only, deliberately NOT terrain: a confirmed-
    // Blocked terrain sector is a pure informational marker on the ring
    // itself (drawTerrainSectorWash()), never a restriction, and must
    // never make the needle itself look like the rotor cannot or should
    // not point there (operator, 2026-09-12: "rotor soll nicht
    // blockiert werden, nur markiert werden, welche richtung nicht gut
    // ist, keine sperre!!!" -- this reverses an earlier same-day
    // version that DID fold terrain into this needle colour).
    drawNeedle(painter, center, radius, m_azimuthDeg, Qt::SolidLine, needleAlpha, isInMechanicalStopZone());
    if (m_secondAntennaEnabled) {
        drawNumberedMark(painter, center, radius, m_azimuthDeg, QStringLiteral("1"));
    }

    // Hub: soft amber radial glow + a small solid dot, matching
    // Longpath's own rotor hub -- replaces the previous flat cream dot,
    // same colour reasoning as drawNeedle() above.
    const QColor hubColor{Style::kAmberText()};
    QRadialGradient hub(center, 7.0);
    hub.setColorAt(0.0, hubColor);
    QColor hubEdge = hubColor;
    hubEdge.setAlpha(0);
    hub.setColorAt(1.0, hubEdge);
    painter.setPen(Qt::NoPen);
    painter.setBrush(hub);
    painter.drawEllipse(center, 7.0, 7.0);
    painter.setBrush(hubColor);
    painter.drawEllipse(center, 2.8, 2.8);
    painter.setBrush(Qt::NoBrush);
}

void RotorWidget::paintPartialArcDial(QPainter& painter, const QPointF& center, double radius) const
{
    // Rotor-C.dc.html: a ~300-degree arc with a fixed gap at south
    // (mechanical stop / cable-wrap zone -- see drawArcGapMarkers()'s
    // own scope note) instead of a full ring, otherwise the same
    // needle/target/second-antenna drawing FullCompass uses.
    drawGlow(painter, center, radius);
    drawPartialArcRing(painter, center, radius);
    drawPartialArcTicks(painter, center, radius);
    drawArcGapMarkers(painter, center, radius);

    drawNeedlesAndTarget(painter, center, radius);
}

void RotorWidget::drawPartialArcRing(QPainter& painter, const QPointF& center, double radius) const
{
    // Traced as a sequence of short segments via the same
    // compass-convention pointOnCircle() the rest of this file uses,
    // rather than converting to Qt's own (x-axis, counter-clockwise)
    // drawArc() angle system -- one fewer angle convention to keep
    // straight. Runs from the gap's end round through north back to the
    // gap's start -- the long way, 300 degrees of the 360.
    QPainterPath arcPath;
    bool first = true;
    for (double deg = kArcGapEndDeg; deg <= kArcGapStartDeg + 360.0; deg += 2.0) {
        const QPointF p = pointOnCircle(center, radius, deg);
        if (first) {
            arcPath.moveTo(p);
            first = false;
        } else {
            arcPath.lineTo(p);
        }
    }
    QPen ringPen{QColor(Style::kBorder())};
    ringPen.setWidthF(1.5);
    painter.setPen(ringPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(arcPath);
}

void RotorWidget::drawPartialArcTicks(QPainter& painter, const QPointF& center, double radius) const
{
    // Same three-tier scheme as FullCompass's drawTicks() (see its own
    // comment), just skipping degrees strictly inside the gap (the
    // gap's own boundary ticks at kArcGapStartDeg/kArcGapEndDeg still
    // draw normally, matching Rotor-C.dc.html's continuous tick run
    // right up to the gap edge).
    for (int deg = 0; deg < 360; deg += 10) {
        if (deg > kArcGapStartDeg && deg < kArcGapEndDeg) {
            continue;
        }
        const bool cardinal = (deg % 90 == 0);
        const bool major = (deg % 30 == 0);
        const double innerRadius = radius - (cardinal ? 14.0 : major ? 10.0 : 6.0);
        const QPointF outer = pointOnCircle(center, radius, deg);
        const QPointF inner = pointOnCircle(center, innerRadius, deg);
        QColor color{cardinal ? Style::kTextSecondary() : Style::kTextScale()};
        color.setAlpha(cardinal ? 255 : major ? 190 : 120);
        QPen pen(color);
        pen.setWidthF(cardinal ? 1.6 : major ? 1.1 : 0.7);
        painter.setPen(pen);
        painter.drawLine(inner, outer);
    }

    // N/E/W only -- south sits inside the gap, so it carries no
    // cardinal letter in this style (Rotor-C.dc.html has none either).
    drawCompassLabel(painter, center, radius + 14.0, 0.0, QStringLiteral("N"));
    drawCompassLabel(painter, center, radius + 14.0, 90.0, QStringLiteral("E"));
    drawCompassLabel(painter, center, radius + 14.0, 270.0, QStringLiteral("W"));

    // Sparser degree numbers than FullCompass (every 30 degrees there):
    // just the four odd 45-multiples between the cardinals, matching
    // Rotor-C.dc.html's own four labels (225/315/45/135) exactly.
    drawDegreeLabel(painter, center, radius + 11.0, 45.0);
    drawDegreeLabel(painter, center, radius + 11.0, 135.0);
    drawDegreeLabel(painter, center, radius + 11.0, 225.0);
    drawDegreeLabel(painter, center, radius + 11.0, 315.0);
}

void RotorWidget::drawArcGapMarkers(QPainter& painter, const QPointF& center, double radius) const
{
    // Amber flanking marks + caption at the gap boundary -- a fixed
    // visual convention of the PartialArc style only, drawn at a
    // static, always-south position (matching Rotor-C.dc.html).
    // Deliberately NOT wired to core/BeamHeading.h's Stop enum or any
    // new per-rotor ContestSettings field: BeamHeading.h's own comment
    // already says the operator's actual stop positions are not
    // configured anywhere yet, and adding that configuration is out of
    // scope for this (a visual-style) task -- see the task notes this
    // pass shipped with. If/when a real configured stop lands, this is
    // the one place that would need to start reading it instead of
    // kArcGapCenterDeg.
    const QColor amber{Style::kAmberWarn()};
    QPen pen(amber);
    pen.setWidthF(1.6);
    painter.setPen(pen);
    for (double deg : {kArcGapStartDeg, kArcGapEndDeg}) {
        const QPointF inner = pointOnCircle(center, radius - 12.0, deg);
        const QPointF outer = pointOnCircle(center, radius + 6.0, deg);
        painter.drawLine(inner, outer);
    }

    painter.setFont(Style::monoFont(painter.font(), Style::kFontCaption));
    const QPointF labelPoint = pointOnCircle(center, radius + 30.0, kArcGapCenterDeg);
    const QRectF box(labelPoint.x() - 80.0, labelPoint.y() - 8.0, 160.0, 16.0);
    painter.drawText(box, Qt::AlignCenter, QStringLiteral("SPERRZONE (ENDANSCHLAG)"));

    // Warning wash along the ring immediately either side of the gap --
    // ONLY drawn once the heading has actually arrived inside the gap
    // (isInMechanicalStopZone()); the static amber ticks/caption above
    // mark the boundary at all times, this additional red band is the
    // "something is actually wrong right now" escalation (Rotor-C.dc.html's
    // rose SPERRZONE wash). A solid warning colour, not the mockup's
    // amber-to-rose gradient -- QPainterPath stroking has no clean way to
    // gradient along a curve, and a flat, unambiguous red communicates
    // the same "stop" meaning without inventing a blend that has no
    // other meaning in this palette.
    if (isInMechanicalStopZone()) {
        const QColor warn{Style::kRedText()};
        QPen washPen(warn);
        washPen.setWidthF(3.0);
        painter.setPen(washPen);
        painter.setBrush(Qt::NoBrush);
        constexpr double kWashSpanDeg = 50.0;
        const auto drawWash = [&](double from, double to) {
            QPainterPath path;
            bool first = true;
            for (double deg = from; deg <= to; deg += 2.0) {
                const QPointF p = pointOnCircle(center, radius, deg);
                if (first) {
                    path.moveTo(p);
                    first = false;
                } else {
                    path.lineTo(p);
                }
            }
            painter.drawPath(path);
        };
        drawWash(kArcGapStartDeg - kWashSpanDeg, kArcGapStartDeg);
        drawWash(kArcGapEndDeg, kArcGapEndDeg + kWashSpanDeg);
    }
}

void RotorWidget::paintLinearScaleDial(QPainter& painter, const QRect& area) const
{
    // Rotor-B.dc.html: no circular dial at all -- a horizontal
    // azimuth track (0-360 degrees left to right) with a filled portion
    // up to the current heading, a "handle" for the current heading,
    // and a downward marker for a commanded target.
    const double trackY = area.center().y();
    const double trackLeft = area.left() + kLinearMarginPx;
    const double trackRight = area.right() - kLinearMarginPx;
    const double trackWidth = trackRight - trackLeft;
    const auto xForDeg = [&](double deg) {
        return trackLeft + (BeamHeading::wrap360(deg) / 360.0) * trackWidth;
    };

    // Disconnected fades the fill + handles together rather than either
    // vanishing or reading as a confident zero -- same HAUSSTIL rule 7
    // reasoning as drawNeedle()'s `alpha` parameter on the other styles.
    const int handleAlpha = m_connected ? 255 : 110;

    // Base groove, Style::kGroove() -- the same sunken-track colour family
    // StyleKit.h already names for other sliders in this codebase.
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(Style::kGroove()));
    painter.drawRoundedRect(QRectF(trackLeft, trackY - kLinearTrackHeightPx / 2.0, trackWidth, kLinearTrackHeightPx),
                             kLinearTrackHeightPx / 2.0, kLinearTrackHeightPx / 2.0);

    // Filled portion up to the current heading -- "how far turned", per
    // Rotor-B.dc.html.
    {
        QColor fillColor{Style::kAmberDim()};
        fillColor.setAlpha(handleAlpha);
        const double fillRight = xForDeg(m_azimuthDeg);
        painter.setBrush(fillColor);
        painter.drawRoundedRect(
            QRectF(trackLeft, trackY - kLinearTrackHeightPx / 2.0, std::max(0.0, fillRight - trackLeft), kLinearTrackHeightPx),
            kLinearTrackHeightPx / 2.0, kLinearTrackHeightPx / 2.0);
    }

    // The beamwidth around each handle (the polar styles' cones), the
    // second antenna's in kAmberWarn like theirs.
    if (m_secondAntennaEnabled) {
        drawLinearBeamBand(painter, trackLeft, trackWidth, trackY,
                           secondAntennaBearing(m_azimuthDeg, m_secondAntennaOffsetDeg), QColor(Style::kAmberWarn()),
                           handleAlpha);
    }
    drawLinearBeamBand(painter, trackLeft, trackWidth, trackY, m_azimuthDeg, QColor(Style::kAmberText()), handleAlpha);

    drawLinearTicks(painter, trackLeft, trackRight, trackY);

    if (m_hasTarget) {
        drawLinearTargetMarker(painter, xForDeg(m_targetBearingDeg), trackY);
    }

    // Second-antenna adaptation for this style: a second, dashed/shorter
    // handle on the same track plus its own numbered mark above it --
    // the closest linear-track equivalent of FullCompass's second
    // needle + ring-edge digit (judgment call: no ring exists here to
    // put the digit "outside" of, so it sits above the handle instead).
    if (m_secondAntennaEnabled) {
        const double secondDeg = secondAntennaBearing(m_azimuthDeg, m_secondAntennaOffsetDeg);
        const double secondX = xForDeg(secondDeg);
        drawLinearHandle(painter, secondX, trackY, handleAlpha, true);
        drawNumberedMarkAt(painter, QPointF(secondX, trackY - 34.0), QStringLiteral("2"));
    }

    drawLinearHandle(painter, xForDeg(m_azimuthDeg), trackY, handleAlpha, false);
    if (m_secondAntennaEnabled) {
        drawNumberedMarkAt(painter, QPointF(xForDeg(m_azimuthDeg), trackY - 40.0), QStringLiteral("1"));
    }
}

void RotorWidget::drawLinearTicks(QPainter& painter, double trackLeft, double trackRight, double trackY) const
{
    const double trackWidth = trackRight - trackLeft;
    const auto xForDeg = [&](double deg) { return trackLeft + (deg / 360.0) * trackWidth; };

    // Coarser than the other two styles (every 45 degrees, not 15) --
    // matches Rotor-B.dc.html's own tick spacing, which reads fine on a
    // compact horizontal strip without the fine graduation a full ring
    // has room for.
    drawLinearCardinalTick(painter, xForDeg(0.0), trackY, QStringLiteral("N"));
    drawLinearIntercardinalTick(painter, xForDeg(45.0), trackY, 45.0);
    drawLinearCardinalTick(painter, xForDeg(90.0), trackY, QStringLiteral("E"));
    drawLinearIntercardinalTick(painter, xForDeg(135.0), trackY, 135.0);
    drawLinearCardinalTick(painter, xForDeg(180.0), trackY, QStringLiteral("S"));
    drawLinearIntercardinalTick(painter, xForDeg(225.0), trackY, 225.0);
    drawLinearCardinalTick(painter, xForDeg(270.0), trackY, QStringLiteral("W"));
    drawLinearIntercardinalTick(painter, xForDeg(315.0), trackY, 315.0);
    drawLinearCardinalTick(painter, xForDeg(360.0), trackY, QStringLiteral("N"));
}

void RotorWidget::drawLinearCardinalTick(QPainter& painter, double x, double trackY, const QString& label) const
{
    QPen pen{QColor(Style::kInstrumentFace())};
    pen.setWidthF(2.0);
    painter.setPen(pen);
    painter.drawLine(QPointF(x, trackY - 16.0), QPointF(x, trackY - kLinearTrackHeightPx / 2.0));

    painter.setFont(Style::monoFont(painter.font(), Style::kFontSmall, QFont::Bold));
    painter.drawText(QRectF(x - 12.0, trackY - 34.0, 24.0, 14.0), Qt::AlignCenter, label);
}

void RotorWidget::drawLinearIntercardinalTick(QPainter& painter, double x, double trackY, double labelDeg) const
{
    QColor color{Style::kInstrumentFace()};
    color.setAlpha(140);
    QPen pen(color);
    pen.setWidthF(1.0);
    painter.setPen(pen);
    painter.drawLine(QPointF(x, trackY - 10.0), QPointF(x, trackY - kLinearTrackHeightPx / 2.0));

    painter.setFont(Style::monoFont(painter.font(), Style::kFontCaption));
    painter.setPen(QColor(Style::kTextScale()));
    painter.drawText(QRectF(x - 14.0, trackY - 28.0, 28.0, 12.0), Qt::AlignCenter, QString::number(static_cast<int>(labelDeg)));
}

void RotorWidget::drawLinearHandle(QPainter& painter, double x, double trackY, int alpha, bool isSecondAntenna) const
{
    // Amber -- same measured-value colour as the polar styles' needle,
    // see drawNeedle()'s own comment for the house-style reasoning.
    QColor color{Style::kAmberText()};
    color.setAlpha(alpha);
    const double tipY = trackY - kLinearTrackHeightPx / 2.0;
    const double headY = tipY - (isSecondAntenna ? 16.0 : 22.0);
    QPolygonF triangle;
    triangle << QPointF(x, tipY) << QPointF(x - 7.0, headY) << QPointF(x + 7.0, headY);

    if (isSecondAntenna) {
        // Outline/dashed rather than filled -- the same solid-vs-dashed
        // distinction FullCompass/PartialArc draw their two needles
        // with, adapted to a filled-vs-outline shape here since a
        // dashed *stroke* on a closed triangle would look like a
        // rendering glitch rather than a second, deliberately distinct
        // handle.
        QPen pen(color);
        pen.setWidthF(1.5);
        pen.setStyle(Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
    }
    painter.drawPolygon(triangle);
}

void RotorWidget::drawLinearTargetMarker(QPainter& painter, double x, double trackY) const
{
    const double tipY = trackY + kLinearTrackHeightPx / 2.0;
    QPolygonF triangle;
    triangle << QPointF(x, tipY) << QPointF(x - 6.0, tipY + 8.0) << QPointF(x + 6.0, tipY + 8.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(Style::kBlueBg()));
    painter.drawPolygon(triangle);
}

void RotorWidget::paintDigitalDial(QPainter& painter, const QPointF& center, double radius) const
{
    // Design2.dc.html ("Option 2 -- Digital") originally gave this style
    // its own small, secondary ring -- corrected by the operator,
    // 2026-09-11 ("rotor gleich groß wie die anderen"): same center/
    // radius paintFullCompassDial() gets, same size as every other
    // style. Only drawDigitalReadout() below still differs.
    drawGlow(painter, center, radius);

    // Ported 1:1 from Longpath's own rotor dial -- two rings, not one:
    // an outer ring in the scale colour (kTextScale, matching the
    // graduation it frames, not the panel's own faint kBorderSubtle
    // edge) and an inner ring one step down (kBorder) at 0.83x radius.
    // When disconnected, the outer ring goes dashed amber-warn instead
    // -- the same "this reading is not real" marker Longpath's own dial
    // draws for a simulated/unconnected rotator, mapped onto this
    // widget's own m_connected flag.
    if (!m_connected) {
        QPen warnPen{QColor(Style::kAmberWarn())};
        warnPen.setWidthF(1.2);
        warnPen.setStyle(Qt::DashLine);
        painter.setPen(warnPen);
    } else {
        painter.setPen(QPen(QColor(Style::kTextScale()), 1.0));
    }
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius, radius);
    painter.setPen(QPen(QColor(Style::kBorder()), 1.0));
    painter.drawEllipse(center, radius * 0.83, radius * 0.83);

    drawTicks(painter, center, radius);

    drawNeedlesAndTarget(painter, center, radius);
}

void RotorWidget::drawDigitalReadout(QPainter& painter, const QRect& area) const
{
    // Replaces drawReadout()'s plain text block for the Digital style
    // only -- large glowing monospace digits inside a "black glass"
    // panel, per HAUSSTIL's already-documented "Grosse Zahlen liegen in
    // schwarzem Glas" rule (~/Longpath/NereusSDR/docs/design/
    // HAUSSTIL.md), applied to the rotor for the first time. Geometry
    // ground truth: Design2.dc.html / build2.py's own glass() helper --
    // same three values drawReadout() shows (Aktuell/Ziel/Entfernung),
    // same colour-meaning rules (amber = measured, blue = commanded,
    // HAUSSTIL rule 7's unknown-dash for anything not (yet) known), just
    // a different layout and a lot bigger.
    const QColor instrumentColor{Style::kAmberText()};
    const QColor blueColor{Style::kBlueBg()};
    const QColor inactiveColor{Style::kTextInactive()};

    const auto formatDeg = [](double deg) {
        return QStringLiteral("%1°").arg(deg, 3, 'f', 0, QLatin1Char('0'));
    };

    int y = area.top() + kDigitalTopPad;
    const int usableWidth = area.width() - kDigitalSidePad * 2;
    const int leftX = area.left() + kDigitalSidePad;

    // Aktuell + Ziel, side by side (build2.py: `display:flex;
    // gap:10px;`). Entfernung is gone -- operator, 2026-09-11:
    // "entfernung muss weg" -- this style shows heading only, the same
    // information the compass ring above it already carries; the
    // distance figure stays available in every other dial style's
    // three-column readout, just not duplicated here.
    {
        const int colWidth = (usableWidth - kDigitalColumnGap) / 2;
        const int boxHeight = kDigitalLabelHeight + kDigitalLabelGap + kDigitalGlassHeight;
        const QRect aktuellBox(leftX, y, colWidth, boxHeight);
        const QRect zielBox(leftX + colWidth + kDigitalColumnGap, y, colWidth, boxHeight);

        // Same warning-red override as the needle/caption when the
        // heading sits inside the mechanical no-go zone (always false
        // for Digital today -- isInMechanicalStopZone() only ever
        // returns true for PartialArc, see its own comment) -- terrain
        // data deliberately NOT folded in, see drawNeedlesAndTarget()'s
        // own primary-needle comment for the full reasoning (pure
        // informational ring marker, never a restriction).
        // See drawReadout()'s own comment on m_simulated.
        const bool showAktuell = m_connected || m_simulated;
        const QColor aktuellColor = m_connected ? (isInMechanicalStopZone() ? QColor(Style::kRedText()) : instrumentColor)
                                   : m_simulated ? QColor(Style::kAmberWarn())
                                                 : inactiveColor;
        drawGlassPanel(painter, aktuellBox, QStringLiteral("Aktuell"),
                        showAktuell ? formatDeg(m_azimuthDeg) : Style::unknownDash(), aktuellColor,
                        kDigitalValueFontPx);

        const QColor zielColor = m_hasTarget ? blueColor : inactiveColor;
        drawGlassPanel(painter, zielBox, QStringLiteral("Ziel"),
                        m_hasTarget ? formatDeg(m_targetBearingDeg) : Style::unknownDash(), zielColor,
                        kDigitalValueFontPx);

        y += boxHeight + kDigitalRowGap;
    }

    // A reduced block (see textAreaHeight()) ends here, or after the
    // caption.
    if (area.height() < kDigitalTextAreaHeight - (kDigitalRowGap + kDigitalStatusHeight)) {
        return;
    }

    // Row 3: caption (target-station identity / SPERRZONE / unknown-
    // dash) -- same computeCaptionLine() logic drawReadout() uses, so
    // both readout styles show the identical message for the identical
    // state.
    {
        QString captionText;
        QColor captionColor;
        bool showCaption = false;
        computeCaptionLine(captionText, captionColor, showCaption);
        if (showCaption) {
            painter.setFont(Style::capsFont(painter.font()));
            painter.setPen(captionColor);
            painter.drawText(QRect(area.left(), y, area.width(), kDigitalCaptionHeight), Qt::AlignCenter, captionText);
        }
        y += kDigitalCaptionHeight + kDigitalRowGap;
    }
    if (area.height() < kDigitalTextAreaHeight) {
        return;
    }

    // Row 4: connection status -- same drawConnectionStatusRow() helper
    // drawReadout() uses.
    drawConnectionStatusRow(painter, QRect(area.left(), y, area.width(), kDigitalStatusHeight));
}

void RotorWidget::drawGlassPanel(QPainter& painter, const QRect& box, const QString& label, const QString& value,
                                  const QColor& valueColor, int valueFontPx) const
{
    // Caps label above the box -- same convention drawReadout()'s own
    // row 1 labels use.
    painter.setFont(Style::capsFont(painter.font()));
    painter.setPen(QColor(Style::kTextScale()));
    painter.drawText(QRect(box.left(), box.top(), box.width(), kDigitalLabelHeight), Qt::AlignCenter, label);

    const QRect glassRect(box.left(), box.top() + kDigitalLabelHeight + kDigitalLabelGap, box.width(),
                           box.height() - kDigitalLabelHeight - kDigitalLabelGap);

    // Near-black glass fill + subtle border -- Style::kInsetBg() rather
    // than build2.py's own literal #020203: same "versenkt" role
    // (StyleKit.h's own comment: "deliberately darker than the panel"),
    // and this codebase's Style:: tokens are what every other colour in
    // this file goes through, never a re-typed hex literal.
    painter.setPen(QPen(QColor(Style::kBorderSubtle()), 1.0));
    painter.setBrush(QColor(Style::kInsetBg()));
    painter.drawRoundedRect(glassRect, 6.0, 6.0);

    // Inset shadow along the top edge -- approximates build2.py's own
    // `box-shadow: inset 0 2px 6px rgba(0,0,0,0.6)`; QPainter has no
    // inset-shadow primitive, so this fakes it with a short dark-to-
    // transparent gradient hugging the top, clipped to the glass rect.
    painter.save();
    painter.setClipRect(glassRect);
    QLinearGradient insetShadow(0, glassRect.top(), 0, glassRect.top() + 10.0);
    insetShadow.setColorAt(0.0, QColor(0, 0, 0, 140));
    insetShadow.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.fillRect(glassRect, insetShadow);
    painter.restore();

    drawGlowingValueText(painter, glassRect, value, valueColor, valueFontPx);
}

void RotorWidget::drawGlowingValueText(QPainter& painter, const QRect& box, const QString& text, const QColor& color,
                                        int fontPx) const
{
    const QFont font = Style::monoFont(painter.font(), fontPx, QFont::Light);
    painter.setFont(font);

    // Approximates build2.py's own CSS `text-shadow: 0 0 18px <color>55`
    // glow -- QPainter has no text-shadow primitive, so this fakes a
    // soft halo the same way drawNeedle()'s own halo pen already does
    // elsewhere in this file (a duller, offset pass underneath the
    // crisp one): a ring of low-alpha copies of the same text, offset a
    // couple of pixels in every direction, drawn before the sharp final
    // pass on top.
    QColor glow = color;
    glow.setAlpha(70);
    painter.setPen(glow);
    constexpr int kGlowOffsetPx = 2;
    for (int dx = -kGlowOffsetPx; dx <= kGlowOffsetPx; dx += kGlowOffsetPx) {
        for (int dy = -kGlowOffsetPx; dy <= kGlowOffsetPx; dy += kGlowOffsetPx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            painter.drawText(box.translated(dx, dy), Qt::AlignCenter, text);
        }
    }

    painter.setPen(color);
    painter.drawText(box, Qt::AlignCenter, text);
}

} // namespace Contestprogramm
