// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/ui/RotorRowWidget.h  (Contestprogramm)
//
// Die Zeile mit den Rotor-Bedienfeldern -- nebeneinander, wenn die
// Breite reicht, sonst untereinander.
//
// Betreiber 2026-11-04, mit dem Panel "Rotoren" als eigenem Fenster in
// Longpath: "das fenster rotoren muss sehr gross gezogen werden, um grad
// eigens zu sehen. bitte um loesungen, um fenster klein ziehen zu
// koennen." Bis dahin war die Zeile ein QHBoxLayout: zwei Rotoren
// nebeneinander, und die Mindestbreite der Zeile war die Summe ihrer
// Mindestbreiten (2 x 186 + 10). Kleiner ging das Fenster nicht, und bei
// der Mindestbreite war jedes Zifferblatt gerade so gross, dass die
// Gradzahlen wegfielen.
//
// Jetzt entscheidet die Breite: passen alle Rotoren nebeneinander,
// stehen sie nebeneinander; sonst untereinander. Die Mindestgroesse der
// Zeile ist die EINES Rotors -- ein schmales, hohes Fenster geht also
// genauso wie ein breites, flaches. Kein Layout-Objekt, weil Qt-Layouts
// ihre Mindestgroesse aus der festen Anordnung ableiten und die Zeile
// damit nie unter die Breite "alle nebeneinander" kaeme.
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-11-04 -- Original von Martin Fischer, KI-gestuetzt ueber
//                 Anthropic Claude.
// =================================================================
#pragma once

#include <QList>
#include <QWidget>

namespace Contestprogramm {

class RotorRowWidget : public QWidget {
    Q_OBJECT

public:
    explicit RotorRowWidget(QWidget* parent = nullptr);

    // Ein Bedienfeld an der Stelle `index` einhaengen (0 = links/oben).
    // Entfernt wird durch Loeschen des Widgets; die Zeile merkt es.
    void einfuegen(int index, QWidget* w);
    // Aus der Zeile nehmen (das Widget bleibt bestehen; wer es loescht,
    // entscheidet der Aufrufer). Ein spaeteres Loeschen ist harmlos.
    void entfernen(QWidget* w);
    int anzahl() const { return m_felder.size(); }
    // Stehen die Felder gerade nebeneinander (true) oder untereinander?
    bool nebeneinander() const { return m_nebeneinander; }

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void childEvent(QChildEvent* event) override;

private:
    void anordnen();
    QSize einzelMindestgroesse() const;

    QList<QWidget*> m_felder;
    bool m_nebeneinander = true;
    static constexpr int kAbstand = 10;
};

} // namespace Contestprogramm
