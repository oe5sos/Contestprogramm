#pragma once

#include "core/SpotCandidate.h"

#include <QString>
#include <QWidget>

#include <optional>

class QLabel;
class QPushButton;

namespace Contestprogramm {

// "Betriebsassistent" (plan section of the same name): shows the
// single best next-target suggestion (core/assistant/
// NextTargetSuggester.h) plus a drafted ON4KST chat message (core/
// assistant/MessageDrafter.h), with two explicit-confirmation actions
// -- never anything automatic. Deliberately a thin, dumb display+signal
// widget, the same "caller computes, widget only renders + emits" split
// RateMeterWidget's own computeRateBreakdown()/refresh() already
// established for this codebase: this class holds no ChatFeedModel/
// AppController/On4kstClient reference at all, so it structurally
// cannot send anything on its own -- the plan's own "harte Regel,
// strukturell erzwungen, nicht nur Konvention" capability-separation
// requirement ("Nur der UI-Bestätigen-Klick hält die Berechtigung,
// tatsächlich zu senden"), enforced by simply never giving this class
// the means, not by a runtime guard. MainWindow computes the
// suggestion+draft (from both ChatFeedModel instances) and performs the
// actual On4kstClient send/entry-row-fill on the signals below.
class SuggestionPanel : public QWidget {
    Q_OBJECT

public:
    explicit SuggestionPanel(QWidget* parent = nullptr);

    // `suggestion` is nullopt when nothing currently qualifies (no
    // reachable, not-yet-worked, currently-visible candidate on either
    // feed) -- shown as an explicit "kein Ziel" state (both buttons
    // disabled), not a blank panel; `distanceKm`/`bearingDeg` follow the
    // same std::nullopt-means-unknown convention UnifiedLogWidget::
    // setEntryDistanceBearing() already uses. `draftedMessage` is
    // whatever MessageDrafter produced for `suggestion`; ignored when
    // `suggestion` is nullopt.
    void setSuggestion(const std::optional<SpotCandidate>& suggestion,
                        const std::optional<double>& distanceKm,
                        const std::optional<double>& bearingDeg,
                        const QString& draftedMessage);

signals:
    // "Ziel übernehmen" -- fills the entry row. Same signature/meaning
    // as UnifiedLogWidget::candidateActivated so MainWindow can connect
    // this straight to its existing handleCandidateActivated() slot
    // rather than a near-duplicate handler.
    void targetAccepted(const QString& callsign, const QString& grid, qint64 freqHz);
    // "Ruf senden" -- MainWindow performs the actual
    // On4kstClient::sendChatMessage() call; this class never touches
    // On4kstClient itself (see the class comment).
    void sendRequested(const QString& text);

private:
    QLabel* m_targetLabel;
    QLabel* m_messageLabel;
    QPushButton* m_acceptButton;
    QPushButton* m_sendButton;
    std::optional<SpotCandidate> m_currentSuggestion;
    QString m_currentDraft;
};

} // namespace Contestprogramm
