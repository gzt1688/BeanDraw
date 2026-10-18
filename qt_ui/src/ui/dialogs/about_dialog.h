#pragma once

#include <QDialog>

class QLabel;
class QDialogButtonBox;

/// 关于对话框：版本信息（应用 / Rust 内核 / Qt）。
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
};
