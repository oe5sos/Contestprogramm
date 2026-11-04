// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/ui/RotorRowWidget.cpp  (Contestprogramm) -- siehe RotorRowWidget.h
//
// =================================================================
// Modification history (Contestprogramm):
//   2026-11-04 -- Original von Martin Fischer, KI-gestuetzt ueber
//                 Anthropic Claude.
// =================================================================
#include "ui/RotorRowWidget.h"

#include <QChildEvent>
#include <algorithm>
#include <QResizeEvent>

namespace Contestprogramm {

RotorRowWidget::RotorRowWidget(QWidget* parent)
    : QWidget(parent)
{
}

void RotorRowWidget::einfuegen(int index, QWidget* w)
{
    if (!w) { return; }
    m_felder.removeAll(w);
    if (index < 0 || index > m_felder.size()) { index = m_felder.size(); }
    m_felder.insert(index, w);
    w->setParent(this);
    w->show();
    anordnen();
    updateGeometry();
}

void RotorRowWidget::entfernen(QWidget* w)
{
    if (m_felder.removeAll(w) > 0) {
        anordnen();
        updateGeometry();
    }
}

QSize RotorRowWidget::einzelMindestgroesse() const
{
    QSize s(0, 0);
    for (QWidget* w : m_felder) {
        if (w) { s = s.expandedTo(w->minimumSizeHint()); }
    }
    return s;
}

QSize RotorRowWidget::minimumSizeHint() const
{
    // EIN Rotor -- ob die anderen daneben oder darunter stehen,
    // entscheidet anordnen() nach der Groesse, die man der Zeile gibt.
    return einzelMindestgroesse();
}

QSize RotorRowWidget::sizeHint() const
{
    // Nebeneinander, mit dem Wunschmass jedes Feldes.
    int w = 0;
    int h = 0;
    for (QWidget* f : m_felder) {
        if (!f) { continue; }
        const QSize s = f->sizeHint();
        w += s.width();
        h = std::max(h, s.height());
    }
    if (m_felder.size() > 1) { w += kAbstand * (m_felder.size() - 1); }
    return QSize(w, h);
}

void RotorRowWidget::anordnen()
{
    const int n = m_felder.size();
    if (n == 0) { return; }
    const QSize einzel = einzelMindestgroesse();
    const int breite = width();
    const int hoehe  = height();

    // Nebeneinander, wenn alle in ihrer Mindestbreite nebeneinander
    // passen; sonst untereinander -- es sei denn, auch das passt nicht,
    // dann bleibt es nebeneinander (irgendwo muss es hin).
    const bool passtNebeneinander = breite >= n * einzel.width() + kAbstand * (n - 1);
    const bool passtUntereinander = hoehe >= n * einzel.height() + kAbstand * (n - 1);
    m_nebeneinander = passtNebeneinander || !passtUntereinander;

    if (m_nebeneinander) {
        const int feldBreite = std::max(1, (breite - kAbstand * (n - 1)) / n);
        int x = 0;
        for (int i = 0; i < n; ++i) {
            const int w = (i == n - 1) ? std::max(1, breite - x) : feldBreite;
            m_felder[i]->setGeometry(x, 0, w, std::max(1, hoehe));
            x += feldBreite + kAbstand;
        }
    } else {
        const int feldHoehe = std::max(1, (hoehe - kAbstand * (n - 1)) / n);
        int y = 0;
        for (int i = 0; i < n; ++i) {
            const int h = (i == n - 1) ? std::max(1, hoehe - y) : feldHoehe;
            m_felder[i]->setGeometry(0, y, std::max(1, breite), h);
            y += feldHoehe + kAbstand;
        }
    }
}

void RotorRowWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    anordnen();
}

void RotorRowWidget::childEvent(QChildEvent* event)
{
    QWidget::childEvent(event);
    if (event->removed()) {
        // Ein geloeschtes Bedienfeld (Rotor-Steckplatz abgeschaltet)
        // faellt aus der Liste; der Rest rueckt nach.
        auto* w = static_cast<QWidget*>(event->child());
        if (m_felder.removeAll(w) > 0) {
            anordnen();
            updateGeometry();
        }
    }
}

} // namespace Contestprogramm
