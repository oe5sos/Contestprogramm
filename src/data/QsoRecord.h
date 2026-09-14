#pragma once

#include <QString>

#include <optional>

namespace Contestprogramm {

// One row of the `qsos` table. See the SQLite schema in the project plan
// (§ "SQLite-Schema (Phase 1)") -- field names and types mirror the
// table columns directly so ContestDatabase can bind/read them without a
// separate mapping layer.
struct QsoRecord {
    int id = -1;                 // -1 == not yet inserted
    QString callsign;
    QString band;                 // "144" | "432"
    QString mode;
    QString timestampUtc;         // ISO-8601
    std::optional<qint64> freqHz; // from TCI, when available
    QString gridSquare;           // worked station's grid
    std::optional<double> distanceKm;
    std::optional<double> bearingDeg;
    std::optional<int> serialSent;
    std::optional<int> serialRcvd;
    // The "rst"-typed exchange field's own value, extracted the same way
    // gridSquare/serialRcvd already are (see MainWindow::findFieldByType/
    // findAutoIncrementField) -- a real DB column of its own, not just
    // baked into exchangeSent/exchangeRcvd's composed text, so a
    // logged-row hand-correction (see UnifiedLogWidget's history rows)
    // can recompose the exchange text losslessly after editing only the
    // grid or serial part. Empty when the active contest has no
    // "rst"-typed field.
    QString rstSent;
    QString rstRcvd;
    QString exchangeSent;         // composed from ContestDefinition.exchangeFields, in order
    QString exchangeRcvd;
    QString contestId;
    bool isDupe = false;
    // DXLog.net deliberately has no delete function for a logged QSO --
    // "in the spirit of honest contest logging... you don't" (dxlog.net/
    // docs/index.php/Menu_Edit, verified for this task) -- a QSO is
    // marked invalid instead, never removed. See DupeChecker/
    // MultiplierTracker/CabrilloExporter/AdifExporter for where this is
    // then excluded.
    bool isInvalid = false;
    QString source = QStringLiteral("manual"); // 'manual' | 'on4kst' | 'tci'
    QString notes;
};

} // namespace Contestprogramm
