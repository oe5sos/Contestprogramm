#pragma once

#include <QColor>
#include <QPixmap>
#include <QString>

namespace Contestprogramm {

// Die Symbole für die Leiste des Seitenbereichs. Martin, 2026-09-28:
// "schön wäre, wenn wir vielleicht icons dazu hätten" -- aus drei
// Blättern hat er C gewählt: Symbol und Name nebeneinander.
//
// Gezeichnet statt geladen: neun kleine Strichzeichnungen, die in der
// Farbe des Zustands entstehen (still oder Akzent). Eine Bilddatei je
// Symbol und Zustand wären achtzehn Dateien, die bei jeder Palette
// wieder nicht passen.
//
// `panelId` ist die Panel-Kennung, wie sie PanelLayoutManager führt
// ("unifiedlog", "map", "chat", ...). Eine unbekannte Kennung ergibt
// ein leeres Bild -- dann steht in der Leiste nur der Name, und das
// ist immer noch brauchbar.
QPixmap sideAreaIconPixmap(const QString& panelId, int kantenlaengePx, const QColor& farbe,
                            qreal geraeteFaktor = 1.0);

} // namespace Contestprogramm
