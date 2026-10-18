#include "about_dialog.h"

#include "ffi_bind/rust_api.h"

#include <QLabel>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QApplication>
#include <QVersionNumber>
#include <QPixmap>
#include <QScrollArea>
#include <QFrame>
#include <QFile>

namespace {

// 把一段富文本包进 QLabel，自动换行、可选浏览器交互（链接）
QLabel* makeRichLabel(const QString& html, bool wordWrap = true,
                      bool openLinks = false, QWidget* parent = nullptr)
{
    auto* lab = new QLabel(parent);
    lab->setTextFormat(Qt::RichText);
    lab->setWordWrap(wordWrap);
    if (openLinks) {
        lab->setTextInteractionFlags(Qt::TextBrowserInteraction);
    }
    lab->setText(html);
    return lab;
}

// 在 QWidget 里垂直堆叠若干 widget，返回容器
QWidget* vBoxPage(std::initializer_list<QWidget*> widgets, int spacing = 12)
{
    auto* page = new QWidget;
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(spacing);
    for (QWidget* w : widgets) {
        lay->addWidget(w);
    }
    lay->addStretch(1);
    return page;
}

}  // namespace

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About BeanDraw"));
    setMinimumSize(640, 560);
    resize(680, 600);

    auto* root = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    root->addWidget(tabs, 1);

    // ============ 标签 1：关于 ============
    auto* title = new QLabel(
        QStringLiteral("<h2 style='margin:0;'>%1</h2>"
                       "<div style='color:#888;'>%2</div>"
                       "<div style='color:#888;'>%3</div>")
            .arg(tr("BeanDraw"),
                 tr("DIY craft · Free & open source · No ads"),
                 tr("Initial release: 2026-10-17 · v0.1.0")),
        this);
    title->setTextFormat(Qt::RichText);

    const QString info = tr(
        "<p>"
        "<b>Version:</b> %1<br>"
        "<b>Core (Rust):</b> %2<br>"
        "<b>Qt:</b> %3"
        "</p>"
        "<p>An <b>all-in-one design platform</b> for craft DIY enthusiasts, "
        "fabric studios and handmade designers.<br>"
        "Includes cross-stitch design, a pattern engine and the DMC color card."
        "</p>"
        "<p style='color:#888;'>This software is completely free and open "
        "source; no user data is collected.</p>")
                             .arg(QApplication::applicationVersion())
                             .arg(rust_api::coreVersion())
                             .arg(QString::fromUtf8(qVersion()));
    auto* infoLabel = makeRichLabel(info, true, false, this);

    tabs->addTab(vBoxPage({title, infoLabel}, 12), tr("About"));

    // ============ 标签 2：开源许可 ============
    auto* ossIntro = makeRichLabel(
        tr("<b>This project is released under the MIT license</b>; anyone may "
           "freely use, study, modify, merge, publish, distribute, sublicense "
           "and resell it for any purpose including commercial use, provided "
           "the copyright notice and full license text are kept in "
           "distributions.<br><i>The software is provided \"as is\"; the "
           "authors accept no direct or indirect liability.</i>"),
        true, false, this);

    const QString lic = tr(
        "<hr>"
        "<p><b>1) BeanDraw (Rust core + Qt UI) — MIT License</b><br>"
        "Copyright (c) 2026 The BeanDraw authors. "
        "Permission is hereby granted, free of charge, to any person obtaining a "
        "copy of this software and associated documentation files (the \"Software\"), "
        "to deal in the Software without restriction, including the rights to use, "
        "copy, modify, merge, publish, distribute, sublicense, and/or sell copies of "
        "the Software, subject to the above copyright notice and this permission "
        "notice in all copies or substantial portions of the Software.<br>"
        "<i>THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, "
        "EXPRESS OR IMPLIED.</i></p>"
        "<p><b>2) Qt Advanced Docking System — GNU LGPL v2.1</b><br>"
        "This library is free software; you can redistribute it and/or modify it "
        "under the terms of the GNU Lesser General Public License, version 2.1, as "
        "published by the Free Software Foundation. The library is distributed in "
        "the hope that it will be useful, but WITHOUT ANY WARRANTY; the full text "
        "ships with the source at "
        "<code>deps/Qt-Advanced-Docking-System-5.1.1/gnu-lgpl-v2.1.md</code>.</p>");
    auto* licLabel = makeRichLabel(lic, true, true, this);
    licLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    // 许可证正文较长，放进滚动区
    auto* licScroll = new QScrollArea(this);
    licScroll->setWidgetResizable(true);
    licScroll->setFrameShape(QFrame::NoFrame);
    licScroll->setWidget(licLabel);

    tabs->addTab(vBoxPage({ossIntro, licScroll}, 12), tr("License"));

    // ============ 标签 4：支持作者 ============
    auto* donateIntro = makeRichLabel(
        tr("<p>BeanDraw is completely <b>free and open source</b>; "
           "maintenance, DMC color card completion, pattern template growth and "
           "cross-platform support all take time and effort.</p>"
           "<p>If it has helped you, feel free to scan the code and buy the "
           "author a coffee ☕ — your support goes straight into further "
           "development.</p>"),
        true, false, this);

    auto* imageWrap = new QWidget(this);
    auto* imageLay = new QHBoxLayout(imageWrap);
    imageLay->setContentsMargins(0, 0, 0, 0);
    imageLay->addStretch(1);

    auto* qrLabel = new QLabel(this);
    qrLabel->setAlignment(Qt::AlignCenter);
    QPixmap qrPix(QStringLiteral(":/icons/donate.png"));
    if (!qrPix.isNull()) {
        // 固定显示宽度 280px，按比例缩放
        qrLabel->setPixmap(qrPix.scaledToWidth(280, Qt::SmoothTransformation));
    } else {
        qrLabel->setText(tr("Donation QR image not found"));
    }
    qrLabel->setFrameShape(QFrame::Box);
    imageLay->addWidget(qrLabel);
    imageLay->addStretch(1);

    auto* thanks = makeRichLabel(
        tr("<p align='center' style='color:#888;'>Thank you again for your "
           "open-source spirit 💛</p>"),
        false, false, this);

    tabs->addTab(vBoxPage({donateIntro, imageWrap, thanks}, 12), tr("Support the author"));

    // ============ 底部按钮 ============
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(buttons);
}
