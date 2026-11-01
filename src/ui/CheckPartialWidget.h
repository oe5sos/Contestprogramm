#pragma once

#include "core/CheckPartialIndex.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;

namespace Contestprogramm {

// The "Check" panel: what CheckPartialIndex found for the callsign
// fragment being typed, as a wrapped row of clickable calls -- the
// N1MM+ Check window / DXLog.net Check Partial window, in this app's
// panel form. A click hands the call (and its locator, when known)
// back to the entry row via callsignChosen(); the widget itself
// neither queries the index nor knows the entry row, MainWindow wires
// the two (same capability separation as SuggestionPanel/CwMacroPanel).
//
// Reading the row: a call worked on the current band is struck
// through and dimmed (a dupe -- exactly what the check is for), a call
// this station knows from its log, call history or the ON4KST/cluster
// feeds is bright, an SCP-only call is secondary, and a near miss
// (N1MM's "N+1", one character off) is prefixed with "≈".
class CheckPartialWidget : public QWidget {
    Q_OBJECT

public:
    explicit CheckPartialWidget(QWidget* parent = nullptr);

    void setMatches(const QString& fragment, const QVector<CheckPartialMatch>& matches);

    // The status line under the matches: which lists are loaded.
    // `scpFileName` empty means no SCP list is loaded, and the line
    // then offers the load action itself (scpLoadRequested()).
    void setSources(int scpCount, const QString& scpFileName, int historyCount, int seenCount);

    // Eine Zeile über den Treffern: bringt diese Station auf diesem
    // Band einen neuen Multiplikator, und auf welchen Bändern steht er
    // schon? Das ist DXLogs "Check Multipliers" -- die Frage, die man
    // beim Tippen wirklich hat. Leer blendet die Zeile aus (kein
    // Multiplikator in den Regeln, oder nichts bekannt).
    void setMultiplierStatus(const QString& text);

signals:
    void callsignChosen(const QString& callsign, const QString& grid);
    void scpLoadRequested();

private:
    void rebuildStatus();

    QLabel* m_multiplierLabel;
    QLabel* m_matchesLabel;
    QLabel* m_statusLabel;
    int m_scpCount = 0;
    QString m_scpFileName;
    int m_historyCount = 0;
    int m_seenCount = 0;
    QVector<CheckPartialMatch> m_matches;
};

} // namespace Contestprogramm
