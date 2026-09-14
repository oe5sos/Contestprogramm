#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QHBoxLayout;
class QPushButton;

namespace Contestprogramm {

// Simple CW F-key macro panel, per the plan's "CW-Unterstützung" nachziehen
// item: "F-Tasten-Makros mit Platzhaltern (Call/Exchange einsetzen)". A
// simple macro panel, not a full keyer -- 4-6 buttons, one per template in
// ContestSettings::cwMacros, each substituting {call}/{exchange} at click
// time and emitting the finished text.
//
// Capability separation, same shape as UnifiedLogWidget: this widget has
// no RigctldClient reference and cannot send anything by itself.
// MainWindow reads UnifiedLogWidget's *current* contents when
// macroActivated fires, substitutes the placeholders, and calls
// RigctldClient::sendMorse() itself.
class CwMacroPanel : public QWidget {
    Q_OBJECT

public:
    explicit CwMacroPanel(QWidget* parent = nullptr);

    // One button per template, in order. Truncated to 6, per the plan's
    // "4-6 buttons" scope; a caller with fewer templates gets fewer
    // buttons.
    void setMacroTemplates(const QStringList& templates);
    QStringList macroTemplates() const { return m_templates; }

    // Replaces "{call}" with `callsign` and "{exchange}" with `exchange`
    // wherever they occur in `templateText`. Case-sensitive, literal
    // substring replacement -- no escaping needed since CW macro text
    // has no other use for curly braces.
    static QString substitute(const QString& templateText, const QString& callsign, const QString& exchange);

signals:
    // Emitted with the button's raw template text (placeholders not yet
    // substituted) -- see the class comment for why substitution happens
    // in MainWindow, not here.
    void macroActivated(const QString& templateText);

private:
    void rebuildButtons();

    QStringList m_templates;
    QVector<QPushButton*> m_buttons;
    QHBoxLayout* m_layout = nullptr;
};

} // namespace Contestprogramm
