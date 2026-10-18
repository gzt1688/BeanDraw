#include "scene.h"

#include "items/cross_stitch_item.h"
#include "items/piece_item.h"
#include "items/text_preview_item.h"
#include "tools/tool_base.h"

#include <QGraphicsView>

CraftScene::CraftScene(QObject* parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-1000, -1000, 3000, 3000);
}

TextPreviewItem* CraftScene::ensureTextPreview()
{
    if (!m_textPreview) {
        m_textPreview = new TextPreviewItem();
        addItem(m_textPreview);
    }
    return m_textPreview;
}

void CraftScene::clearTextPreview()
{
    if (m_textPreview) {
        removeItem(m_textPreview);
        delete m_textPreview; // QGraphicsItem 析构自动摘离场景
        m_textPreview = nullptr;
    }
}

void CraftScene::setTextPreviewVisible(bool on)
{
    if (m_textPreview) {
        m_textPreview->setVisible(on);
    }
}

void CraftScene::setPlates(const QVector<ChartSnapshot>& plates)
{
    clearAll();
    if (plates.isEmpty()) {
        return;
    }
    m_crossStitchItem = new CrossStitchItem(plates);
    // 面料计数取首盘参数（仅当有效时；空盘保持默认 14ct）
    if (plates.first().valid && plates.first().params.fabric_count > 0) {
        m_crossStitchItem->setFabricCount(plates.first().params.fabric_count);
    }
    addItem(m_crossStitchItem);
    // 视图初始适配由 MainWindow 完成
}

bool CraftScene::updatePlate(int plate, const ChartSnapshot& chart)
{
    if (!chart.valid || chart.gridSize.isEmpty()) {
        return false;
    }
    if (!m_crossStitchItem || plate < 0 || plate >= m_crossStitchItem->plateCount()) {
        return false;
    }
    m_crossStitchItem->setFabricCount(chart.params.fabric_count);
    m_crossStitchItem->updatePlate(plate, chart);
    return true;
}

void CraftScene::setPiece(const PieceSnapshot& piece)
{
    if (!m_pieceItem) {
        m_pieceItem = new PieceItem();
        addItem(m_pieceItem);
    }
    m_piece = piece;
    m_pieceItem->setPaths(piece.outline, piece.seam);
}

void CraftScene::clearAll()
{
    // 场景重建前先清掉文字预览等覆盖层（内部有顺带的状态复位）
    clearTextPreview();
    if (m_crossStitchItem) {
        removeItem(m_crossStitchItem);
        delete m_crossStitchItem;
        m_crossStitchItem = nullptr;
    }
    if (m_pieceItem) {
        removeItem(m_pieceItem);
        delete m_pieceItem;
        m_pieceItem = nullptr;
    }
    m_piece = PieceSnapshot();
}

void CraftScene::setCurrentTool(ToolBase* tool)
{
    m_currentTool = tool;
    Q_EMIT currentToolChanged();
}

QPoint CraftScene::cellAt(const QPointF& scenePos) const
{
    if (!m_crossStitchItem) {
        return QPoint(-1, -1);
    }
    return m_crossStitchItem->cellAt(scenePos);
}

int CraftScene::plateAt(const QPointF& scenePos) const
{
    if (!m_crossStitchItem) {
        return -1;
    }
    return m_crossStitchItem->plateAt(scenePos);
}

void CraftScene::reportPlateClick(const QPointF& scenePos)
{
    if (!m_crossStitchItem) {
        return;
    }
    const int plate = m_crossStitchItem->plateAt(scenePos);
    if (plate >= 0 && plate != m_crossStitchItem->activePlate()) {
        emit plateActivationRequested(plate);
    }
}

bool CraftScene::handleItemPress(const QPointF& scenePos)
{
    if (!m_crossStitchItem) {
        return false;
    }
    return m_crossStitchItem->handlePress(scenePos);
}

QPointF CraftScene::cellCenter(int x, int y) const
{
    if (!m_crossStitchItem) {
        return QPointF();
    }
    return m_crossStitchItem->cellCenter(x, y);
}

void CraftScene::reportHover(const QPointF& scenePos)
{
    const QPoint cell = cellAt(scenePos);
    emit cellHovered(cell.x(), cell.y());
}
