#include "cross_stitch_settings.h"

#include <QSettings>
#include <QtGlobal>
#include <cmath>

namespace {
constexpr int kMinFabricCount = 6;
constexpr int kMaxFabricCount = 32;

int clampInt(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

double clampDouble(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}
} // namespace

CrossStitchSettings CrossStitchSettings::defaults()
{
    return CrossStitchSettings{};
}

void CrossStitchSettings::load()
{
    QSettings s;
    s.beginGroup(QStringLiteral("crossStitch"));

    stitchLengthMm = s.value(QStringLiteral("stitchLengthMm"), stitchLengthMm).toDouble();
    strands = s.value(QStringLiteral("strands"), strands).toInt();
    skeinMeters = s.value(QStringLiteral("skeinMeters"), skeinMeters).toDouble();
    flossMarginPct = s.value(QStringLiteral("flossMarginPct"), flossMarginPct).toInt();

    beadMarginPct = s.value(QStringLiteral("beadMarginPct"), beadMarginPct).toInt();
    beadSizeMm = s.value(QStringLiteral("beadSizeMm"), beadSizeMm).toDouble();
    countBeads = s.value(QStringLiteral("countBeads"), countBeads).toBool();

    fabricCount = s.value(QStringLiteral("fabricCount"), fabricCount).toInt();
    borderMarginMm = s.value(QStringLiteral("borderMarginMm"), borderMarginMm).toInt();
    extraStitchesPerColor =
        s.value(QStringLiteral("extraStitchesPerColor"), extraStitchesPerColor).toInt();
    coveragePct = s.value(QStringLiteral("coveragePct"), coveragePct).toInt();

    maxColors = s.value(QStringLiteral("maxColors"), maxColors).toInt();
    symbolSet = s.value(QStringLiteral("symbolSet"), symbolSet).toInt();
    cleanup = s.value(QStringLiteral("cleanup"), cleanup).toInt();
    rareColorPpt = s.value(QStringLiteral("rareColorPpt"), rareColorPpt).toInt();
    dither = s.value(QStringLiteral("dither"), dither).toBool();

    showSymbols = s.value(QStringLiteral("showSymbols"), showSymbols).toBool();
    showGrid = s.value(QStringLiteral("showGrid"), showGrid).toBool();
    majorGridEvery = s.value(QStringLiteral("majorGridEvery"), majorGridEvery).toInt();
    plateCount = s.value(QStringLiteral("plateCount"), plateCount).toInt();
    plateMarginCells = s.value(QStringLiteral("plateMarginCells"), plateMarginCells).toInt();
    plateNames = s.value(QStringLiteral("plateNames"), plateNames).toStringList();

    showColorNameColumn =
        s.value(QStringLiteral("showColorNameColumn"), showColorNameColumn).toBool();
    showStitchesColumn = s.value(QStringLiteral("showStitchesColumn"), showStitchesColumn).toBool();
    showBeadsColumn = s.value(QStringLiteral("showBeadsColumn"), showBeadsColumn).toBool();
    showThreadColumn = s.value(QStringLiteral("showThreadColumn"), showThreadColumn).toBool();

    keyPicker = s.value(QStringLiteral("keyPicker"), keyPicker).toString();
    keyViewMax = s.value(QStringLiteral("keyViewMax"), keyViewMax).toString();

    printPaperKind = s.value(QStringLiteral("printPaperKind"), printPaperKind).toInt();
    printLandscape = s.value(QStringLiteral("printLandscape"), printLandscape).toBool();
    printCellMm = s.value(QStringLiteral("printCellMm"), printCellMm).toDouble();
    printMarginMm = s.value(QStringLiteral("printMarginMm"), printMarginMm).toDouble();
    // 新键 V2：旧键 printLegendScalePct 是 12pt 基准（新基准为其 ×2.8），不复用旧值
    printLegendScalePct =
        s.value(QStringLiteral("printLegendScalePctV2"), printLegendScalePct).toInt();
    const QStringList lockedStr = s.value(QStringLiteral("plateLocked")).toStringList();
    plateLocked.clear();
    plateLocked.reserve(lockedStr.size());
    for (const QString& token : lockedStr) {
        plateLocked.append(token == QLatin1String("1"));
    }

    s.endGroup();
    sanitize();
}

void CrossStitchSettings::save() const
{
    QSettings s;
    s.beginGroup(QStringLiteral("crossStitch"));

    s.setValue(QStringLiteral("stitchLengthMm"), stitchLengthMm);
    s.setValue(QStringLiteral("strands"), strands);
    s.setValue(QStringLiteral("skeinMeters"), skeinMeters);
    s.setValue(QStringLiteral("flossMarginPct"), flossMarginPct);

    s.setValue(QStringLiteral("beadMarginPct"), beadMarginPct);
    s.setValue(QStringLiteral("beadSizeMm"), beadSizeMm);
    s.setValue(QStringLiteral("countBeads"), countBeads);

    s.setValue(QStringLiteral("fabricCount"), fabricCount);
    s.setValue(QStringLiteral("borderMarginMm"), borderMarginMm);
    s.setValue(QStringLiteral("extraStitchesPerColor"), extraStitchesPerColor);
    s.setValue(QStringLiteral("coveragePct"), coveragePct);

    s.setValue(QStringLiteral("maxColors"), maxColors);
    s.setValue(QStringLiteral("symbolSet"), symbolSet);
    s.setValue(QStringLiteral("cleanup"), cleanup);
    s.setValue(QStringLiteral("rareColorPpt"), rareColorPpt);
    s.setValue(QStringLiteral("dither"), dither);

    s.setValue(QStringLiteral("showSymbols"), showSymbols);
    s.setValue(QStringLiteral("showGrid"), showGrid);
    s.setValue(QStringLiteral("majorGridEvery"), majorGridEvery);
    s.setValue(QStringLiteral("plateCount"), plateCount);
    s.setValue(QStringLiteral("plateMarginCells"), plateMarginCells);
    s.setValue(QStringLiteral("plateNames"), plateNames);

    s.setValue(QStringLiteral("showColorNameColumn"), showColorNameColumn);
    s.setValue(QStringLiteral("showStitchesColumn"), showStitchesColumn);
    s.setValue(QStringLiteral("showBeadsColumn"), showBeadsColumn);
    s.setValue(QStringLiteral("showThreadColumn"), showThreadColumn);

    s.setValue(QStringLiteral("keyPicker"), keyPicker);
    s.setValue(QStringLiteral("keyViewMax"), keyViewMax);

    s.setValue(QStringLiteral("printPaperKind"), printPaperKind);
    s.setValue(QStringLiteral("printLandscape"), printLandscape);
    s.setValue(QStringLiteral("printCellMm"), printCellMm);
    s.setValue(QStringLiteral("printMarginMm"), printMarginMm);
    s.setValue(QStringLiteral("printLegendScalePctV2"), printLegendScalePct);
    QStringList lockedStr;
    lockedStr.reserve(plateLocked.size());
    for (bool locked : plateLocked) {
        lockedStr.append(locked ? QStringLiteral("1") : QStringLiteral("0"));
    }
    s.setValue(QStringLiteral("plateLocked"), lockedStr);

    s.endGroup();
}

void CrossStitchSettings::sanitize()
{
    stitchLengthMm = clampDouble(stitchLengthMm, 1.0, 100.0);
    strands = clampInt(strands, 1, 6);
    skeinMeters = clampDouble(skeinMeters, 0.5, 100.0);
    flossMarginPct = clampInt(flossMarginPct, 0, 200);

    beadMarginPct = clampInt(beadMarginPct, 0, 200);
    beadSizeMm = clampDouble(beadSizeMm, 0.5, 20.0);

    fabricCount = clampInt(fabricCount, kMinFabricCount, kMaxFabricCount);
    borderMarginMm = clampInt(borderMarginMm, 0, 500);
    extraStitchesPerColor = clampInt(extraStitchesPerColor, 0, 100);
    coveragePct = clampInt(coveragePct, 1, 100);

    maxColors = clampInt(maxColors, 1, 64);
    symbolSet = clampInt(symbolSet, 0, 3);
    cleanup = clampInt(cleanup, 0, 2);
    rareColorPpt = clampInt(rareColorPpt, 0, 100);
    majorGridEvery = clampInt(majorGridEvery, 1, 100);
    plateCount = clampInt(plateCount, 1, kMaxPlateCount);
    plateMarginCells = clampInt(plateMarginCells, 0, 50);

    // 快捷键空值回落默认（QKeySequenceEdit 清空后保存）
    if (keyPicker.isEmpty()) {
        keyPicker = QStringLiteral("Q");
    }
    if (keyViewMax.isEmpty()) {
        keyViewMax = QStringLiteral("Space");
    }

    printPaperKind = clampInt(printPaperKind, 0, 4);
    printCellMm = clampDouble(printCellMm, 2.0, 30.0);
    printMarginMm = clampDouble(printMarginMm, 0.0, 30.0);
    printLegendScalePct = clampInt(printLegendScalePct, 50, 300);
    // 锁定标记只补齐（与 plateNames 一致：不截断，避免盘数波动时丢状态）
    while (plateLocked.size() < plateCount) {
        plateLocked.append(false);
    }
}

double CrossStitchSettings::effectiveStitchLengthMm() const
{
    // 面料计数越高，单格越小，两根对角线随之变短（14ct 为标定基准）
    const int count = qBound(kMinFabricCount, fabricCount, kMaxFabricCount);
    return stitchLengthMm * (14.0 / count);
}

int CrossStitchSettings::skeinsFor(quint32 stitches) const
{
    if (stitches == 0) {
        return 0;
    }
    const double perStitchMm = effectiveStitchLengthMm();
    if (perStitchMm <= 0.0) {
        return 0;
    }
    // 一束 6 股线按所用股数分成若干捆，每捆长度不变 → 可用总长 = 束长 × (6 / 股数)
    const double usableMm = skeinMeters * 1000.0 * (6.0 / strands);
    const double stitchesPerSkein = usableMm / perStitchMm;
    if (stitchesPerSkein <= 0.0) {
        return 0;
    }
    const double marginFactor = 1.0 + flossMarginPct / 100.0;
    const double needed = stitches / marginFactor + extraStitchesPerColor;
    return std::max(1, static_cast<int>(std::ceil(needed / stitchesPerSkein)));
}

int CrossStitchSettings::beadsFor(quint32 stitches) const
{
    if (!countBeads || stitches == 0) {
        return 0;
    }
    const double marginFactor = 1.0 + beadMarginPct / 100.0;
    return static_cast<int>(std::ceil(stitches * marginFactor));
}

int CrossStitchSettings::requiredFabricSideMm(int cells) const
{
    const int count = qBound(kMinFabricCount, fabricCount, kMaxFabricCount);
    const double cellMm = 25.4 / count;
    return qRound(cells * cellMm) + borderMarginMm * 2;
}
