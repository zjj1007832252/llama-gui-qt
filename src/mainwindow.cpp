#include "mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextOption>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QUrl>

#include <algorithm>

namespace {
const QString kTaskHint = QStringLiteral("暂无统计数据，启动服务并处理请求后此处显示各 slot 的实时速度");
}

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("llama.cpp本地启动器（多参数）、启动参数管理工具、最优启动参数测试、"
                                 "多尺寸上下文批量测速工具、CPU多线程批量测速工具  @%1")
                      .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy.MM.dd hh:mm"))));
    resize(1230, 780);

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *left = new QVBoxLayout;
    left->setContentsMargins(6, 8, 6, 4);
    left->setSpacing(3);
    left->addWidget(createModelPanel());
    left->addWidget(createParamPanel(), 1);
    left->addWidget(createCmdPanel(), 1);

    auto *leftWrap = new QWidget;
    leftWrap->setFixedWidth(470);
    leftWrap->setLayout(left);

    auto *right = new QVBoxLayout;
    right->setContentsMargins(6, 8, 6, 4);
    right->setSpacing(4);

    auto *serverTabs = new QTabWidget;
    serverTabs->addTab(createServerPage(), QStringLiteral("1、服务器启动参数"));
    serverTabs->addTab(new QWidget, QStringLiteral("2、上下文批量测试"));
    serverTabs->addTab(new QWidget, QStringLiteral("3、多线程测试"));
    serverTabs->setFixedHeight(212);

    m_taskInfo = new QPlainTextEdit;
    m_taskInfo->setReadOnly(true);
    m_taskInfo->setFixedHeight(60);
    m_taskInfo->setFrameShape(QFrame::Box);
    m_taskInfo->setStyleSheet(QStringLiteral(
        "QPlainTextEdit{background:#ffffff;border:1px solid #c9ccd4;color:#898989;}"
        "QScrollBar:vertical{width:14px;}"));
    m_taskInfo->setPlainText(kTaskHint);

    auto *logTabs = new QTabWidget;
    m_runLog = new QPlainTextEdit;
    m_testLog = new QPlainTextEdit;
    m_serverLog = new QPlainTextEdit;
    for (QPlainTextEdit *edit : {m_runLog, m_testLog, m_serverLog}) {
        edit->setReadOnly(true);
        edit->setWordWrapMode(QTextOption::NoWrap);
        edit->setFont(QFont(QStringLiteral("SimSun"), 10));
        edit->setMaximumBlockCount(5000);
        edit->setStyleSheet(QStringLiteral(
            "QPlainTextEdit{background:#ffffff;border:1px solid #d4d4d4;font-size:10pt;}"));
    }
    m_runLog->clear();
    logTabs->addTab(m_runLog, QStringLiteral("运行日志"));
    logTabs->addTab(m_testLog, QStringLiteral("测试结果"));
    logTabs->addTab(m_serverLog, QStringLiteral("服务器日志"));

    right->addWidget(serverTabs);
    right->addWidget(m_taskInfo);
    right->addWidget(logTabs, 1);

    root->addWidget(leftWrap);

    auto *vsep = new QFrame;
    vsep->setFrameShape(QFrame::VLine);
    vsep->setFixedWidth(1);
    vsep->setStyleSheet(QStringLiteral("color:#d0d0d0;background:#d0d0d0;border:none;"));
    root->addWidget(vsep);

    root->addLayout(right, 1);

    setStyleSheet(QStringLiteral(
        "MainWindow{background:#f0f0f0;}"
        "QLabel{background:transparent;}"
        "QCheckBox{background:transparent;}"
        "QCheckBox::indicator{width:15px;height:15px;background:#ffffff;"
        "border:1px solid #909090;border-radius:2px;}"
        "QCheckBox::indicator:checked{background:#1a7abf;border-color:#1a7abf;"
        "image:url(:/res/check.png);}"
        "QLineEdit,QComboBox,QSpinBox{background:#ffffff;border:1px solid #a0a0a0;"
        "border-radius:2px;padding:1px 4px;min-height:20px;}"
        "QPlainTextEdit{font-size:9pt;}"
        "QPushButton{background:#fdfdfd;border:1px solid #b8b8b8;border-radius:4px;"
        "padding:3px 10px;}"
        "QPushButton:hover{background:#f2f2f2;}"
        "QPushButton:pressed{background:#e4e4e4;}"));

    m_server = new QProcess(this);
    m_server->setProcessChannelMode(QProcess::MergedChannels);
    // console 子进程默认会弹出黑窗口，这里强制隐藏
    m_server->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
    connect(m_server, &QProcess::readyRead, this, [this] {
        const QString text = QString::fromLocal8Bit(m_server->readAll());
        const QStringList lines = text.split(QLatin1Char('\n'));
        for (QString line : lines) {
            line.remove(QLatin1Char('\r'));
            if (!line.trimmed().isEmpty())
                handleServerLine(line);
        }
    });
    connect(m_server, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (m_stopping)
            return; // 主动停止时的 Crashed 属预期，不弹窗
        if (err == QProcess::FailedToStart) {
            QMessageBox::warning(this, QStringLiteral("启动失败"),
                                 QStringLiteral("无法启动 llama-server，请检查工具地址是否正确。\n") +
                                     m_server->errorString());
        } else if (err == QProcess::Crashed) {
            QMessageBox::warning(this, QStringLiteral("服务异常退出"),
                                 QStringLiteral("llama-server 进程异常崩溃，详情请查看「服务器日志」。"));
        }
        setRunningUi(false);
    });
    connect(m_server, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        if (m_running) {
            m_serverLog->appendPlainText(QStringLiteral("[GUI] 服务进程已退出，code = %1").arg(code));
            setRunningUi(false);
        }
    });

    autoDetectPaths();
    wireLogic();
    scanModels();
    refreshAll();
}

// ---------------------------------------------------------------- model panel

QWidget *MainWindow::createModelPanel()
{
    auto *panel = new QWidget;
    auto *grid = new QGridLayout(panel);
    grid->setContentsMargins(2, 0, 0, 6);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(5);

    auto title = new QLabel(QStringLiteral("模型选择"));
    title->setStyleSheet(QStringLiteral("font-size:10pt;background:transparent;"));

    auto addBrowse = [grid](int row, int col) {
        auto *btn = new QPushButton(QStringLiteral("浏览..."));
        btn->setFixedWidth(80);
        grid->addWidget(btn, row, col);
        return btn;
    };

    grid->addWidget(title, 0, 0, 1, 2);
    grid->addWidget(new QLabel(QStringLiteral("Lamma.cpp工具地址:")), 1, 0, 1, 2);

    m_toolPath = new QLineEdit;
    m_toolPath->setStyleSheet(QStringLiteral(
        "QLineEdit{background:#91e98c;border:1px solid #8f8f8f;}"));
    grid->addWidget(m_toolPath, 2, 0);
    auto *browseTool = addBrowse(2, 1);

    grid->addWidget(new QLabel(QStringLiteral("GGUF模型文件夹(支持多个模型、系统自动查找):")), 3, 0, 1, 2);

    m_modelDir = new QLineEdit;
    grid->addWidget(m_modelDir, 4, 0);
    auto *browseDir = addBrowse(4, 1);

    m_modelCombo = new QComboBox;
    grid->addWidget(m_modelCombo, 5, 0);
    auto *refreshBtn = new QPushButton(QStringLiteral("刷 新"));
    refreshBtn->setFixedWidth(80);
    grid->addWidget(refreshBtn, 5, 1);

    grid->addWidget(new QLabel(QStringLiteral("多模态投影模型文件:")), 6, 0, 1, 2);

    m_mmprojPath = new QLineEdit;
    grid->addWidget(m_mmprojPath, 7, 0);
    auto *browseMm = addBrowse(7, 1);

    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 0);

    connect(browseTool, &QPushButton::clicked, this, &MainWindow::browseToolPath);
    connect(browseDir, &QPushButton::clicked, this, &MainWindow::browseModelDir);
    connect(browseMm, &QPushButton::clicked, this, &MainWindow::browseMmproj);
    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::scanModels);

    auto *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color:#d8d8d8;"));

    auto *outer = new QVBoxLayout;
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);
    outer->addWidget(panel);
    outer->addWidget(sep);

    auto *wrap = new QWidget;
    wrap->setLayout(outer);
    return wrap;
}

// ---------------------------------------------------------------- param panel

QWidget *MainWindow::createParamPanel()
{
    auto *outer = new QVBoxLayout;
    outer->setContentsMargins(2, 0, 0, 2);
    outer->setSpacing(4);

    outer->addWidget(new QLabel(QStringLiteral("模型参数设置")));

    auto makeHeader = [](const QString &title) {
        auto *header = new QFrame;
        header->setStyleSheet(QStringLiteral(
            "QFrame{background:#fafafa;border:1px solid #cfcfcf;border-radius:2px;}"));
        auto *hl = new QHBoxLayout(header);
        hl->setContentsMargins(8, 3, 8, 3);
        auto *icon = new QLabel(QStringLiteral("🔧"));
        icon->setStyleSheet(QStringLiteral("border:none;background:transparent;font-size:12pt;"));
        auto *cap = new QLabel(title);
        cap->setStyleSheet(QStringLiteral(
            "border:none;background:transparent;font-size:10.5pt;font-weight:600;"));
        auto *arrow = new QToolButton;
        arrow->setText(QStringLiteral("▼"));
        arrow->setAutoRaise(true);
        arrow->setStyleSheet(QStringLiteral("border:none;background:transparent;color:#555;"));
        hl->addWidget(icon);
        hl->addSpacing(4);
        hl->addWidget(cap);
        hl->addStretch();
        hl->addWidget(arrow);
        return header;
    };

    auto *body = new QWidget;
    auto *grid = new QGridLayout(body);
    grid->setContentsMargins(2, 4, 2, 0);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(7);

    auto addRow = [&](QGridLayout *g, int row, QCheckBox *cb, QWidget *editor) {
        g->addWidget(cb, row, 0);
        if (editor)
            g->addWidget(editor, row, 1);
    };

    m_ctxCheck = new QCheckBox(QStringLiteral("上下文窗口大小"));
    m_ctxSpin = new QSpinBox;
    m_ctxSpin->setRange(256, 99999999);
    m_ctxSpin->setValue(8192);
    m_ctxSpin->setFixedWidth(110);
    m_ctxInfo = new QLineEdit(QStringLiteral("128K (131072)"));
    m_ctxInfo->setFixedWidth(100);
    m_ctxInfo->setReadOnly(true);
    m_ctxInfo->setAlignment(Qt::AlignLeft);
    auto *ctxRow = new QWidget;
    auto *ctxL = new QHBoxLayout(ctxRow);
    ctxL->setContentsMargins(0, 0, 0, 0);
    ctxL->setSpacing(6);
    ctxL->addWidget(m_ctxSpin);
    ctxL->addWidget(m_ctxInfo);
    addRow(grid, 0, m_ctxCheck, ctxRow);

    m_threadsCheck = new QCheckBox(QStringLiteral("CPU线程数"));
    m_threadsSpin = new QSpinBox;
    m_threadsSpin->setRange(1, 1024);
    m_threadsSpin->setValue(12);
    m_threadsSpin->setFixedWidth(110);
    addRow(grid, 1, m_threadsCheck, m_threadsSpin);

    m_flashCheck = new QCheckBox(QStringLiteral("Flash Attn"));
    m_flashCombo = new QComboBox;
    m_flashCombo->addItems({QStringLiteral("on"), QStringLiteral("off"),
                            QStringLiteral("auto")});
    m_flashCombo->setFixedWidth(200);
    addRow(grid, 2, m_flashCheck, m_flashCombo);

    m_nglCheck = new QCheckBox(QStringLiteral("GPU卸载层数"));
    m_nglCheck->setChecked(true);
    m_nglSpin = new QSpinBox;
    m_nglSpin->setRange(0, 999);
    m_nglSpin->setValue(99);
    m_nglSpin->setFixedWidth(110);
    addRow(grid, 3, m_nglCheck, m_nglSpin);

    m_noMmapCheck = new QCheckBox(QStringLiteral("禁用内存映射"));
    m_mmapLoadCheck = new QCheckBox(QStringLiteral("内存映射加载"));
    // 两个内存映射选项合并到同一行
    auto *mmapRow = new QWidget;
    auto *mmapL = new QHBoxLayout(mmapRow);
    mmapL->setContentsMargins(0, 0, 0, 0);
    mmapL->setSpacing(12);
    mmapL->addWidget(m_mmapLoadCheck);
    mmapL->addStretch();
    addRow(grid, 4, m_noMmapCheck, mmapRow);

    m_cpuMoeCheck = new QCheckBox(QStringLiteral("CPU MoE"));
    // 投机解码：--spec-type draft-mtp + --spec-draft-n-max（默认不启用）
    m_specCheck = new QCheckBox(QStringLiteral("MTP投机解码"));
    m_specSpin = new QSpinBox;
    m_specSpin->setRange(1, 8);
    m_specSpin->setValue(2);
    m_specSpin->setFixedWidth(110);
    auto *specRow = new QWidget;
    auto *specL = new QHBoxLayout(specRow);
    specL->setContentsMargins(0, 0, 0, 0);
    specL->setSpacing(6);
    specL->addWidget(m_specCheck);
    specL->addWidget(m_specSpin);
    specL->addStretch();
    addRow(grid, 5, m_cpuMoeCheck, specRow);

    m_reasoningCheck = new QCheckBox(QStringLiteral("Reasoning"));
    m_reasoningCombo = new QComboBox;
    m_reasoningCombo->addItems({QStringLiteral("on"), QStringLiteral("off"),
                                QStringLiteral("auto")});
    m_reasoningCombo->setFixedWidth(200);
    addRow(grid, 6, m_reasoningCheck, m_reasoningCombo);

    m_splitCheck = new QCheckBox(QStringLiteral("多卡拆分模式"));
    m_splitCombo = new QComboBox;
    m_splitCombo->addItems({QStringLiteral("layer"), QStringLiteral("row"),
                            QStringLiteral("tensor"), QStringLiteral("none")});
    m_splitCombo->setFixedWidth(200);
    addRow(grid, 7, m_splitCheck, m_splitCombo);

    grid->setColumnStretch(1, 1);

    outer->addWidget(makeHeader(QStringLiteral("一、核心基础参数")));
    outer->addWidget(body);

    // 二、KV 缓存量化类型（默认不勾选 = 使用 server 默认 f16，不输出参数）
    auto *body2 = new QWidget;
    auto *grid2 = new QGridLayout(body2);
    grid2->setContentsMargins(2, 4, 2, 0);
    grid2->setHorizontalSpacing(8);
    grid2->setVerticalSpacing(7);

    const QStringList cacheTypes = {QStringLiteral("f16"), QStringLiteral("q8_0"),
                                    QStringLiteral("q5_0"), QStringLiteral("q5_1"),
                                    QStringLiteral("q4_0"), QStringLiteral("q4_1"),
                                    QStringLiteral("iq4_nl"), QStringLiteral("bf16"),
                                    QStringLiteral("f32")};
    m_cacheKCheck = new QCheckBox(QStringLiteral("K缓存量化"));
    m_cacheKCombo = new QComboBox;
    m_cacheKCombo->addItems(cacheTypes);
    m_cacheKCombo->setFixedWidth(200);
    addRow(grid2, 0, m_cacheKCheck, m_cacheKCombo);

    m_cacheVCheck = new QCheckBox(QStringLiteral("V缓存量化"));
    m_cacheVCombo = new QComboBox;
    m_cacheVCombo->addItems(cacheTypes);
    m_cacheVCombo->setFixedWidth(200);
    addRow(grid2, 1, m_cacheVCheck, m_cacheVCombo);

    grid2->setColumnStretch(1, 1);

    outer->addWidget(makeHeader(QStringLiteral("二、KV 缓存量化类型")));
    outer->addWidget(body2);

    auto *wrap = new QWidget;
    wrap->setLayout(outer);
    return wrap;
}

// ---------------------------------------------------------------- cmd panel

QWidget *MainWindow::createCmdPanel()
{
    auto *outer = new QVBoxLayout;
    outer->setContentsMargins(2, 4, 0, 0);
    outer->setSpacing(3);
    outer->addWidget(new QLabel(QStringLiteral("启动参数配置信息")));

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    m_cmdInfo = new QPlainTextEdit;
    m_cmdInfo->setReadOnly(true);
    m_cmdInfo->setStyleSheet(QStringLiteral(
        "QPlainTextEdit{background:#ffffff;border:1px solid #b0b0b0;font-size:10pt;}"));
    m_cmdInfo->setFont(QFont(QStringLiteral("SimSun"), 10));

    auto *btnCol = new QVBoxLayout;
    btnCol->setContentsMargins(0, 0, 0, 0);
    btnCol->setSpacing(7);
    const QList<QPair<QString, void (MainWindow::*)()>> names = {
        {QStringLiteral("获取最优启动参数"), &MainWindow::bestParams},
        {QStringLiteral("cmd窗口对话测试"), &MainWindow::chatTest},
        {QStringLiteral("生成server启动脚"), &MainWindow::generateScript},
        {QStringLiteral("清空日志信息"), nullptr},
        {QStringLiteral("引入启动参数"), &MainWindow::loadParams},
        {QStringLiteral("保存启动参数"), &MainWindow::saveParams},
        {QStringLiteral("动态解析启动参数"), &MainWindow::parseArgsText},
    };
    QPushButton *clearBtn = nullptr;
    for (const auto &n : names) {
        auto *btn = new QPushButton(n.first);
        btn->setFixedWidth(130);
        btnCol->addWidget(btn, 0, Qt::AlignHCenter);
        if (n.second)
            connect(btn, &QPushButton::clicked, this, n.second);
        else
            clearBtn = btn;
    }
    connect(clearBtn, &QPushButton::clicked, this, [this] {
        m_runLog->clear();
        m_testLog->clear();
        m_serverLog->clear();
        m_taskInfo->setStyleSheet(QStringLiteral(
            "QPlainTextEdit{background:#ffffff;border:1px solid #c9ccd4;color:#898989;}"
            "QScrollBar:vertical{width:14px;}"));
        m_taskInfo->setPlainText(kTaskHint);
        m_taskStats.clear();
    });

    row->addWidget(m_cmdInfo, 1);
    row->addLayout(btnCol);

    outer->addLayout(row, 1);

    auto *wrap = new QWidget;
    wrap->setLayout(outer);
    return wrap;
}

// ---------------------------------------------------------------- server page

QWidget *MainWindow::createServerPage()
{
    auto *page = new QWidget;
    page->setStyleSheet(QStringLiteral("background:#ffffff;"));

    auto *grid = new QGridLayout(page);
    grid->setContentsMargins(12, 12, 10, 8);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(10);

    auto fieldLabel = [](const QString &t) {
        auto *lb = new QLabel(t);
        lb->setStyleSheet(QStringLiteral("background:transparent;font-size:10pt;"));
        return lb;
    };

    grid->addWidget(fieldLabel(QStringLiteral("监听地址:")), 0, 0, Qt::AlignRight);
    m_listenEdit = new QLineEdit(QStringLiteral("127.0.0.1"));
    m_listenEdit->setFixedWidth(96);
    grid->addWidget(m_listenEdit, 0, 1, Qt::AlignLeft);
    m_localOnlyCheck = new QCheckBox(QStringLiteral("本地访问"));
    m_localOnlyCheck->setChecked(true);
    m_localOnlyCheck->setStyleSheet(QStringLiteral("background:transparent;"));
    grid->addWidget(m_localOnlyCheck, 0, 2, Qt::AlignLeft);
    grid->addWidget(fieldLabel(QStringLiteral("API Port:")), 0, 3, Qt::AlignRight);
    m_portEdit = new QLineEdit(QStringLiteral("8090"));
    m_portEdit->setFixedWidth(62);
    grid->addWidget(m_portEdit, 0, 4, Qt::AlignLeft);
    grid->addWidget(fieldLabel(QStringLiteral("Cors支持:")), 0, 5, Qt::AlignRight);
    m_corsCheck = new QCheckBox(QStringLiteral("跨域"));
    m_corsCheck->setStyleSheet(QStringLiteral("background:transparent;"));
    grid->addWidget(m_corsCheck, 0, 6, Qt::AlignLeft);

    grid->addWidget(fieldLabel(QStringLiteral("API URL:")), 1, 0, Qt::AlignRight);
    m_urlEdit = new QLineEdit(QStringLiteral("http://localhost:8090/v1"));
    m_urlEdit->setReadOnly(true);
    grid->addWidget(m_urlEdit, 1, 1, 1, 5);
    auto *copyUrl = new QPushButton(QStringLiteral("复制"));
    copyUrl->setFixedWidth(58);
    grid->addWidget(copyUrl, 1, 6);

    grid->addWidget(fieldLabel(QStringLiteral("API Key:")), 2, 0, Qt::AlignRight);
    m_keyEdit = new QLineEdit(QStringLiteral("sk-1234567890"));
    grid->addWidget(m_keyEdit, 2, 1, 1, 5);
    auto *copyKey = new QPushButton(QStringLiteral("复制"));
    copyKey->setFixedWidth(58);
    grid->addWidget(copyKey, 2, 6);

    grid->addWidget(fieldLabel(QStringLiteral("Model ID:")), 3, 0, Qt::AlignRight);
    m_modelIdEdit = new QLineEdit;
    grid->addWidget(m_modelIdEdit, 3, 1, 1, 5);
    auto *copyId = new QPushButton(QStringLiteral("复制"));
    copyId->setFixedWidth(58);
    grid->addWidget(copyId, 3, 6);

    // 深色面板 = 启动/停止按钮；下方依次是 Web 跳转按钮和关闭 web 访问按钮
    auto *rightCol = new QWidget;
    auto *rl = new QVBoxLayout(rightCol);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(6);

    m_startPanel = new QPushButton;
    m_startPanel->setCursor(Qt::PointingHandCursor);
    m_startPanel->setFixedHeight(92);
    m_startPanel->setToolTip(QStringLiteral("点击启动 / 停止 llama-server"));
    rl->addWidget(m_startPanel);

    m_openWebBtn = new QPushButton;
    m_openWebBtn->setCursor(Qt::PointingHandCursor);
    m_openWebBtn->setEnabled(false);
    m_openWebBtn->setToolTip(QStringLiteral("启动服务后可点击打开 llama-server 网页界面"));
    m_openWebBtn->setStyleSheet(QStringLiteral(
        "QPushButton{background:#ffffff;border:1px solid #7aa7c7;border-radius:4px;"
        "color:#1a6fb5;font-size:10pt;padding:3px 4px;text-decoration:underline;}"
        "QPushButton:hover{background:#eef5fb;}"
        "QPushButton:disabled{color:#a8a8a8;border-color:#c0c0c0;text-decoration:none;}"));
    connect(m_openWebBtn, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl(webUiUrl()));
    });
    rl->addWidget(m_openWebBtn);

    m_closeWebBtn = new QPushButton(QStringLiteral("关闭 llama.cpp 的web访问"));
    m_closeWebBtn->setCursor(Qt::PointingHandCursor);
    m_closeWebBtn->setStyleSheet(QStringLiteral(
        "QPushButton{background:#ffffff;border:2px solid #34616e;border-radius:4px;"
        "color:#1f3d47;font-size:10.5pt;padding:4px 6px;}"
        "QPushButton:hover{background:#f0f6f8;}"));
    rl->addWidget(m_closeWebBtn);
    rl->addStretch();

    grid->addWidget(rightCol, 0, 7, 4, 1);
    grid->setColumnMinimumWidth(7, 240);

    grid->setColumnStretch(1, 1);
    grid->setColumnMinimumWidth(7, 240);

    connect(copyUrl, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(m_urlEdit->text()); });
    connect(copyKey, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(m_keyEdit->text()); });
    connect(copyId, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(m_modelIdEdit->text()); });
    connect(m_startPanel, &QPushButton::clicked, this, [this] {
        m_running ? stopServer() : startServer();
    });
    connect(m_closeWebBtn, &QPushButton::clicked, this, [this] {
        m_webuiDisabled = !m_webuiDisabled;
        m_closeWebBtn->setText(m_webuiDisabled ? QStringLiteral("开启 llama.cpp 的web访问")
                                               : QStringLiteral("关闭 llama.cpp 的web访问"));
        m_openWebBtn->setEnabled(m_running && !m_webuiDisabled);
        refreshCmdInfo();
        if (m_running) {
            m_serverLog->appendPlainText(
                QStringLiteral("[GUI] web访问设置已变更，正在重启服务使其生效..."));
            restartServer();
        }
    });

    setRunningUi(false);
    return page;
}

// ---------------------------------------------------------------- wiring

void MainWindow::wireLogic()
{
    auto regen = [this] { refreshAll(); };
    for (QCheckBox *cb : {m_ctxCheck, m_threadsCheck, m_flashCheck, m_nglCheck,
                          m_noMmapCheck, m_cpuMoeCheck, m_specCheck, m_reasoningCheck,
                          m_splitCheck, m_mmapLoadCheck, m_cacheKCheck, m_cacheVCheck})
        connect(cb, &QCheckBox::toggled, this, regen);
    connect(m_ctxSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
        refreshCtxInfo();
        refreshCmdInfo();
    });
    connect(m_threadsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, regen);
    connect(m_specSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, regen);
    connect(m_nglSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, regen);
    for (QComboBox *cb : {m_flashCombo, m_reasoningCombo, m_splitCombo,
                          m_cacheKCombo, m_cacheVCombo})
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, regen);

    connect(m_toolPath, &QLineEdit::editingFinished, this, [this] { refreshCmdInfo(); });
    connect(m_modelDir, &QLineEdit::editingFinished, this, [this] { scanModels(); });
    connect(m_mmprojPath, &QLineEdit::textChanged, this, regen);
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        refreshModelId();
        refreshCmdInfo();
    });

    connect(m_localOnlyCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            m_listenEdit->setText(QStringLiteral("127.0.0.1"));
        refreshApiUrl();
        refreshCmdInfo();
    });
    connect(m_listenEdit, &QLineEdit::textChanged, this, [this] {
        refreshApiUrl();
        refreshCmdInfo();
    });
    connect(m_portEdit, &QLineEdit::textChanged, this, [this] {
        refreshApiUrl();
        refreshCmdInfo();
    });
    connect(m_keyEdit, &QLineEdit::textChanged, this, regen);
    connect(m_modelIdEdit, &QLineEdit::textChanged, this, regen);
    connect(m_corsCheck, &QCheckBox::toggled, this, regen);
}

void MainWindow::autoDetectPaths()
{
    if (m_toolPath->text().trimmed().isEmpty()) {
        const QString exe = QStandardPaths::findExecutable(QStringLiteral("llama-server"));
        if (!exe.isEmpty())
            m_toolPath->setText(QDir::toNativeSeparators(QFileInfo(exe).absolutePath()));
    }
}

void MainWindow::scanModels()
{
    const QString dir = QDir::fromNativeSeparators(m_modelDir->text().trimmed());
    QStringList models;
    // 空路径时 QDir 会指向当前工作目录，导致误扫描，这里直接视为未指定
    if (!dir.isEmpty()) {
        QDirIterator it(dir, {QStringLiteral("*.gguf")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString rel = QDir(dir).relativeFilePath(it.next());
            if (!QFileInfo(rel).fileName().startsWith(QStringLiteral("mmproj")))
                models << QDir::toNativeSeparators(rel);
        }
    }
    models.sort(Qt::CaseInsensitive);
    m_modelCombo->blockSignals(true);
    m_modelCombo->clear();
    m_modelCombo->addItems(models);
    m_modelCombo->blockSignals(false);
    if (models.isEmpty()) {
        m_modelCombo->setEditText(dir.isEmpty() ? QString() : QStringLiteral("未找到模型文件"));
    } else {
        m_modelCombo->setCurrentIndex(0);
        refreshModelId();
    }
    refreshCmdInfo();
}

void MainWindow::selectModelFile(const QString &path)
{
    const QString dir = QDir::fromNativeSeparators(m_modelDir->text());
    const QString abs = QDir::fromNativeSeparators(path);
    QString rel = QDir(dir).relativeFilePath(abs);
    if (rel.startsWith(QStringLiteral("..")))
        rel = QFileInfo(abs).fileName();
    rel = QDir::toNativeSeparators(rel);
    int idx = m_modelCombo->findText(rel);
    if (idx < 0) {
        m_modelCombo->addItem(rel);
        idx = m_modelCombo->count() - 1;
    }
    m_modelCombo->setCurrentIndex(idx);
}

// ---------------------------------------------------------------- command gen

QString MainWindow::modelFilePath() const
{
    const QString rel = m_modelCombo->currentText();
    if (rel.isEmpty() || rel.endsWith(QStringLiteral("未找到模型")))
        return QString();
    return QDir::toNativeSeparators(
        QDir(m_modelDir->text()).filePath(QDir::fromNativeSeparators(rel)));
}

QStringList MainWindow::buildServerArgs() const
{
    QStringList args;
    const QString model = modelFilePath();
    if (!model.isEmpty())
        args << QStringLiteral("-m") << model;
    if (!m_mmprojPath->text().trimmed().isEmpty())
        args << QStringLiteral("-mm") << QDir::toNativeSeparators(m_mmprojPath->text().trimmed());

    // 二、KV 缓存量化类型（默认 f16 不输出；量化 V 缓存需要 flash attention，
    // 显式关闭 -fa 时丢弃量化 V，不传 -fa 时 server 默认 auto 会自动开启）
    const bool flashExplicitOff = m_flashCheck->isChecked() && m_flashCombo->currentText() == QStringLiteral("off");
    const bool vQuant = m_cacheVCheck->isChecked() && m_cacheVCombo->currentText() != QLatin1String("f16") &&
                        m_cacheVCombo->currentText() != QLatin1String("f32");
    if (m_cacheKCheck->isChecked())
        args << QStringLiteral("-ctk") << m_cacheKCombo->currentText();
    if (m_cacheVCheck->isChecked() && !(vQuant && flashExplicitOff))
        args << QStringLiteral("-ctv") << m_cacheVCombo->currentText();

    if (m_nglCheck->isChecked())
        args << QStringLiteral("-ngl") << QString::number(m_nglSpin->value());
    if (m_threadsCheck->isChecked())
        args << QStringLiteral("-t") << QString::number(m_threadsSpin->value());
    if (m_ctxCheck->isChecked())
        args << QStringLiteral("-c") << QString::number(m_ctxSpin->value());
    if (m_flashCheck->isChecked())
        args << QStringLiteral("-fa") << m_flashCombo->currentText();
    if (m_splitCheck->isChecked())
        args << QStringLiteral("-sm") << m_splitCombo->currentText();
    if (m_noMmapCheck->isChecked())
        args << QStringLiteral("--no-mmap");
    if (m_mmapLoadCheck->isChecked())
        args << QStringLiteral("--mmap");
    if (m_cpuMoeCheck->isChecked())
        args << QStringLiteral("--cpu-moe");
    if (m_specCheck->isChecked()) {
        args << QStringLiteral("--spec-type") << QStringLiteral("draft-mtp");
        args << QStringLiteral("--spec-draft-n-max") << QString::number(m_specSpin->value());
    }
    if (m_reasoningCheck->isChecked())
        args << QStringLiteral("--reasoning") << m_reasoningCombo->currentText();

    args << QStringLiteral("--host") << (m_localOnlyCheck->isChecked()
                                             ? QStringLiteral("127.0.0.1")
                                             : m_listenEdit->text().trimmed().isEmpty()
                                                 ? QStringLiteral("127.0.0.1")
                                                 : m_listenEdit->text().trimmed());
    bool portOk = false;
    const int port = m_portEdit->text().toInt(&portOk);
    args << QStringLiteral("--port") << QString::number(portOk ? port : 8090);
    if (!m_keyEdit->text().trimmed().isEmpty())
        args << QStringLiteral("--api-key") << m_keyEdit->text().trimmed();
    if (!m_modelIdEdit->text().trimmed().isEmpty())
        args << QStringLiteral("-a") << m_modelIdEdit->text().trimmed();
    if (m_webuiDisabled)
        args << QStringLiteral("--no-webui");

    // CORS：勾选=任意来源(默认*)，未勾选=仅本机页面可跨域
    args << QStringLiteral("--cors-origins")
         << (m_corsCheck->isChecked() ? QStringLiteral("*") : QStringLiteral("localhost"));
    return args;
}

void MainWindow::refreshCmdInfo()
{
    QString text;
    // 二、KV 缓存量化类型（与实际启动参数保持一致：显式关闭 flash attention 时丢弃量化 V 缓存）
    const bool flashOffNow = m_flashCheck->isChecked() && m_flashCombo->currentText() == QStringLiteral("off");
    const bool vQuantNow = m_cacheVCheck->isChecked() && m_cacheVCombo->currentText() != QLatin1String("f16") &&
                           m_cacheVCombo->currentText() != QLatin1String("f32");
    text += QStringLiteral("# 二、KV 缓存量化类型\n");
    bool anyCache = false;
    if (m_cacheKCheck->isChecked()) {
        text += QStringLiteral("--cache-type-k %1\n").arg(m_cacheKCombo->currentText());
        anyCache = true;
    }
    if (m_cacheVCheck->isChecked() && !(vQuantNow && flashOffNow)) {
        text += QStringLiteral("--cache-type-v %1\n").arg(m_cacheVCombo->currentText());
        anyCache = true;
    }
    if (!anyCache)
        text += QStringLiteral("(未启用，使用 server 默认 f16)\n");
    text += QLatin1Char('\n');
    text += QStringLiteral("# 一、核心基础参数\n");
    if (m_nglCheck->isChecked())
        text += QStringLiteral("--n-gpu-layers %1\n").arg(m_nglSpin->value());
    if (m_threadsCheck->isChecked())
        text += QStringLiteral("--threads %1\n").arg(m_threadsSpin->value());
    if (m_ctxCheck->isChecked())
        text += QStringLiteral("--ctx-size %1\n").arg(m_ctxSpin->value());
    if (m_flashCheck->isChecked())
        text += QStringLiteral("--flash-attn %1\n").arg(m_flashCombo->currentText());
    if (m_splitCheck->isChecked())
        text += QStringLiteral("--split-mode %1\n").arg(m_splitCombo->currentText());
    if (m_noMmapCheck->isChecked())
        text += QStringLiteral("--no-mmap\n");
    if (m_mmapLoadCheck->isChecked())
        text += QStringLiteral("--mmap\n");
    if (m_cpuMoeCheck->isChecked())
        text += QStringLiteral("--cpu-moe\n");
    if (m_specCheck->isChecked()) {
        text += QStringLiteral("--spec-type draft-mtp\n");
        text += QStringLiteral("--spec-draft-n-max %1\n").arg(m_specSpin->value());
    }
    if (m_reasoningCheck->isChecked())
        text += QStringLiteral("--reasoning %1\n").arg(m_reasoningCombo->currentText());
    if (!m_mmprojPath->text().trimmed().isEmpty())
        text += QStringLiteral("--mmproj %1\n").arg(QDir::toNativeSeparators(m_mmprojPath->text().trimmed()));
    text += QLatin1Char('\n');
    text += QStringLiteral("# 三、网络与 API 参数\n");
    text += QStringLiteral("--host %1\n").arg(m_localOnlyCheck->isChecked()
                                                  ? QStringLiteral("127.0.0.1")
                                                  : m_listenEdit->text());
    text += QStringLiteral("--port %1\n").arg(m_portEdit->text());
    if (!m_keyEdit->text().trimmed().isEmpty())
        text += QStringLiteral("--api-key %1\n").arg(m_keyEdit->text().trimmed());
    if (!m_modelIdEdit->text().trimmed().isEmpty())
        text += QStringLiteral("--alias %1\n").arg(m_modelIdEdit->text().trimmed());
    if (m_webuiDisabled)
        text += QStringLiteral("--no-webui\n");
    text += QStringLiteral("--cors-origins %1\n")
                .arg(m_corsCheck->isChecked() ? QStringLiteral("*") : QStringLiteral("localhost"));

    const int pos = m_cmdInfo->verticalScrollBar()->value();
    m_cmdInfo->setPlainText(text);
    m_cmdInfo->verticalScrollBar()->setValue(pos);
}

void MainWindow::refreshApiUrl()
{
    QString host = m_listenEdit->text().trimmed();
    if (host.isEmpty() || host == QLatin1String("127.0.0.1") || host == QLatin1String("0.0.0.0"))
        host = QStringLiteral("localhost");
    m_urlEdit->setText(QStringLiteral("http://%1:%2/v1").arg(host, m_portEdit->text()));
}

void MainWindow::refreshModelId()
{
    QString name = QFileInfo(m_modelCombo->currentText()).completeBaseName();
    if (name.isEmpty() || name == QLatin1String("*"))
        return;
    m_modelIdEdit->setText(name);
}

void MainWindow::refreshCtxInfo()
{
    const int ctx = m_ctxSpin->value();
    m_ctxInfo->setText(QStringLiteral("%1K (%2)").arg(ctx / 1024).arg(ctx));
    // 只读框光标停在末尾时会滚动显示文本尾部（如只露出 "8K (131072)"），拨回开头
    m_ctxInfo->setCursorPosition(0);
}

void MainWindow::refreshAll()
{
    refreshCtxInfo();
    refreshApiUrl();
    refreshCmdInfo();
}

// ---------------------------------------------------------------- browse

void MainWindow::browseToolPath()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择 llama.cpp 工具目录"), m_toolPath->text());
    if (dir.isEmpty())
        return;
    m_toolPath->setText(QDir::toNativeSeparators(dir));
    refreshCmdInfo();
}

void MainWindow::browseModelDir()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择 GGUF 模型文件夹"), m_modelDir->text());
    if (dir.isEmpty())
        return;
    m_modelDir->setText(QDir::toNativeSeparators(dir));
    scanModels();
}

void MainWindow::browseMmproj()
{
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择多模态投影模型文件"), m_modelDir->text(),
        QStringLiteral("GGUF 模型 (*.gguf);;所有文件 (*.*)"));
    if (file.isEmpty())
        return;
    m_mmprojPath->setText(QDir::toNativeSeparators(file));
}

// ---------------------------------------------------------------- best params

void MainWindow::bestParams()
{
    // 显存探测
    double totalGB = 0, usedGB = 0;
    QProcess nvidia;
    nvidia.start(QStringLiteral("nvidia-smi"),
                 {QStringLiteral("--query-gpu=memory.total,memory.used"),
                  QStringLiteral("--format=csv,noheader,nounits")});
    if (nvidia.waitForFinished(4000)) {
        const QString out = QString::fromLocal8Bit(nvidia.readAllStandardOutput());
        const QRegularExpression rx(QStringLiteral("(\\d+)\\s*,\\s*(\\d+)"));
        const QRegularExpressionMatch m = rx.match(out);
        if (m.hasMatch()) {
            totalGB = m.captured(1).toDouble() / 1024.0;
            usedGB = m.captured(2).toDouble() / 1024.0;
        }
    }

    const QString model = modelFilePath();
    const double modelGB = QFileInfo::exists(model) ? QFileInfo(model).size() / 1073741824.0 : 0;
    const double freeGB = totalGB > 0 ? totalGB - usedGB : 0;
    const double overheadGB = 1.3; // KV 缓存与计算缓冲预留

    int ngl = m_nglSpin->value();
    if (modelGB > 0 && freeGB > 0) {
        if (modelGB + overheadGB <= freeGB)
            ngl = 999;
        else
            ngl = qBound(0, int(99.0 * (freeGB - overheadGB) / modelGB), 99);
    }
    const int threads = QThread::idealThreadCount();

    m_nglCheck->setChecked(true);
    m_nglSpin->setValue(ngl);
    m_threadsCheck->setChecked(true);
    m_threadsSpin->setValue(threads);
    if (!m_flashCheck->isChecked()) {
        m_flashCheck->setChecked(true);
        m_flashCombo->setCurrentText(QStringLiteral("on"));
    }
    // 显存放不下整个模型时建议开启 CPU MoE：注意力等稠密部分留在 GPU，MoE 专家权重放 CPU
    m_cpuMoeCheck->setChecked(ngl < 999);
    refreshAll();

    m_runLog->appendPlainText(QStringLiteral(
        "[GUI] 已生成最优启动参数: GPU 总显存 %1GB / 可用 %2GB, 模型 %3GB → "
        "-ngl %4, -t %5, -fa on%6")
        .arg(totalGB, 0, 'f', 1, QLatin1Char('0'))
        .arg(freeGB, 0, 'f', 1, QLatin1Char('0'))
        .arg(modelGB, 0, 'f', 2, QLatin1Char('0'))
        .arg(ngl).arg(threads)
        .arg(ngl < 999 ? QStringLiteral(", --cpu-moe(显存不足,建议MoE权重放CPU)") : QString()));
}

// ---------------------------------------------------------------- script / chat

void MainWindow::generateScript()
{
    if (!ensureCanStart())
        return;
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("生成 server 启动脚本"),
        QDir::toNativeSeparators(QFileInfo(modelFilePath()).dir().filePath(QStringLiteral("start_llama_server.bat"))),
        QStringLiteral("批处理脚本 (*.bat)"));
    if (path.isEmpty())
        return;

    QStringList args = buildServerArgs();
    for (int i = 0; i < args.size(); ++i)
        if (args[i].contains(QLatin1Char(' ')))
            args[i] = QLatin1Char('"') + args[i] + QLatin1Char('"');

    QString bat;
    bat += QStringLiteral("@echo off\r\n");
    bat += QStringLiteral("title llama-server (%1)\r\n").arg(m_modelIdEdit->text());
    bat += QStringLiteral("cd /d \"%1\"\r\n").arg(QDir::toNativeSeparators(m_toolPath->text()));
    bat += QStringLiteral("llama-server.exe %1\r\n").arg(args.join(QLatin1Char(' ')));
    bat += QStringLiteral("pause\r\n");

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法写入脚本文件: %1").arg(f.errorString()));
        return;
    }
    f.write(bat.toLocal8Bit());
    f.close();
    m_runLog->appendPlainText(QStringLiteral("[GUI] 已生成启动脚本: %1").arg(QDir::toNativeSeparators(path)));
    if (QMessageBox::question(this, QStringLiteral("生成完成"),
                              QStringLiteral("启动脚本已生成:\n%1\n\n是否立即打开脚本所在目录?").arg(path)) == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

void MainWindow::chatTest()
{
    QString host = m_listenEdit->text().trimmed();
    if (host.isEmpty() || host == QLatin1String("127.0.0.1") || host == QLatin1String("0.0.0.0"))
        host = QStringLiteral("localhost");

    const QString prompt = QStringLiteral("你好，请用一句话介绍你自己");

    // JSON 里非 ASCII 字符转成 \uXXXX，保证 bat 在任何代码页下都合法
    auto asciiJson = [](const QString &s) {
        QString out;
        for (QChar ch : s) {
            const ushort u = ch.unicode();
            if (u < 128)
                out += ch;
            else
                out += QStringLiteral("\\u%1").arg(u, 4, 16, QLatin1Char('0'));
        }
        return out;
    };
    const QString json = QStringLiteral(
        "{\"model\":\"%1\",\"messages\":[{\"role\":\"user\",\"content\":\"%2\"}],\"stream\":true}")
        .arg(asciiJson(m_modelIdEdit->text().trimmed()), asciiJson(prompt));

    QString bat;
    bat += QStringLiteral("@echo off\r\n");
    bat += QStringLiteral("title llama.cpp chat test (%1)\r\n").arg(m_urlEdit->text());
    bat += QStringLiteral("echo === llama.cpp chat test: %1 ===\r\n").arg(m_urlEdit->text());
    bat += QStringLiteral("echo Model: %1   API Key: %2\r\n").arg(m_modelIdEdit->text(), m_keyEdit->text());
    bat += QStringLiteral("echo.\r\n");
    // bat 内部需要用 \" 转义 JSON 的引号，curl 才能收到合法 JSON
    const QString jsonEscaped = QString(json).replace(QLatin1Char('"'), QStringLiteral("\\\""));
    bat += QStringLiteral("curl.exe -N \"%1/chat/completions\" -H \"Content-Type: application/json\" "
                          "-H \"Authorization: Bearer %2\" -d \"%3\"\r\n")
               .arg(m_urlEdit->text(), m_keyEdit->text(), jsonEscaped);
    bat += QStringLiteral("echo.\r\n");
    bat += QStringLiteral("pause\r\n");

    const QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                         + QStringLiteral("/llama_chat_test.bat");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
                             QStringLiteral("无法生成对话测试脚本: %1").arg(f.errorString()));
        return;
    }
    f.write(bat.toUtf8());
    f.close();
    // 通过 shell 打开，确保弹出独立控制台窗口
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("无法启动 cmd 窗口。"));
}

// ---------------------------------------------------------------- save / load / parse

void MainWindow::saveParams()
{
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存启动参数"),
        QDir::toNativeSeparators(QDir(m_modelDir->text()).filePath(QStringLiteral("启动参数.json"))),
        QStringLiteral("参数配置 (*.json)"));
    if (path.isEmpty())
        return;

    QJsonObject o;
    o.insert(QStringLiteral("toolPath"), m_toolPath->text());
    o.insert(QStringLiteral("modelDir"), m_modelDir->text());
    o.insert(QStringLiteral("modelFile"), m_modelCombo->currentText());
    o.insert(QStringLiteral("mmproj"), m_mmprojPath->text());
    o.insert(QStringLiteral("ctxEnabled"), m_ctxCheck->isChecked());
    o.insert(QStringLiteral("ctx"), m_ctxSpin->value());
    o.insert(QStringLiteral("threadsEnabled"), m_threadsCheck->isChecked());
    o.insert(QStringLiteral("threads"), m_threadsSpin->value());
    o.insert(QStringLiteral("flashEnabled"), m_flashCheck->isChecked());
    o.insert(QStringLiteral("flashAttn"), m_flashCombo->currentText());
    o.insert(QStringLiteral("nglEnabled"), m_nglCheck->isChecked());
    o.insert(QStringLiteral("ngl"), m_nglSpin->value());
    o.insert(QStringLiteral("noMmap"), m_noMmapCheck->isChecked());
    o.insert(QStringLiteral("cpuMoe"), m_cpuMoeCheck->isChecked());
    o.insert(QStringLiteral("specEnabled"), m_specCheck->isChecked());
    o.insert(QStringLiteral("specDraftNMax"), m_specSpin->value());
    o.insert(QStringLiteral("reasoningEnabled"), m_reasoningCheck->isChecked());
    o.insert(QStringLiteral("reasoning"), m_reasoningCombo->currentText());
    o.insert(QStringLiteral("splitEnabled"), m_splitCheck->isChecked());
    o.insert(QStringLiteral("splitMode"), m_splitCombo->currentText());
    o.insert(QStringLiteral("mmapLoad"), m_mmapLoadCheck->isChecked());
    o.insert(QStringLiteral("cacheKEnabled"), m_cacheKCheck->isChecked());
    o.insert(QStringLiteral("cacheK"), m_cacheKCombo->currentText());
    o.insert(QStringLiteral("cacheVEnabled"), m_cacheVCheck->isChecked());
    o.insert(QStringLiteral("cacheV"), m_cacheVCombo->currentText());
    o.insert(QStringLiteral("localOnly"), m_localOnlyCheck->isChecked());
    o.insert(QStringLiteral("listen"), m_listenEdit->text());
    o.insert(QStringLiteral("port"), m_portEdit->text());
    o.insert(QStringLiteral("apiKey"), m_keyEdit->text());
    o.insert(QStringLiteral("modelId"), m_modelIdEdit->text());
    o.insert(QStringLiteral("webuiDisabled"), m_webuiDisabled);
    o.insert(QStringLiteral("cors"), m_corsCheck->isChecked());

    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(o).toJson());
        m_runLog->appendPlainText(QStringLiteral("[GUI] 启动参数已保存: %1").arg(path));
    }
}

void MainWindow::loadParams()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("引入启动参数"), m_modelDir->text(),
        QStringLiteral("参数配置 (*.json);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("错误"), QStringLiteral("参数文件格式无效。"));
        return;
    }

    m_toolPath->setText(o.value(QStringLiteral("toolPath")).toString(m_toolPath->text()));
    m_modelDir->setText(o.value(QStringLiteral("modelDir")).toString(m_modelDir->text()));
    m_mmprojPath->setText(o.value(QStringLiteral("mmproj")).toString());
    scanModels();
    const QString modelFile = o.value(QStringLiteral("modelFile")).toString();
    if (!modelFile.isEmpty())
        selectModelFile(QDir(m_modelDir->text()).filePath(modelFile));
    m_ctxCheck->setChecked(o.value(QStringLiteral("ctxEnabled")).toBool(true));
    m_ctxSpin->setValue(o.value(QStringLiteral("ctx")).toInt(131072));
    m_threadsCheck->setChecked(o.value(QStringLiteral("threadsEnabled")).toBool(true));
    m_threadsSpin->setValue(o.value(QStringLiteral("threads")).toInt(12));
    m_flashCheck->setChecked(o.value(QStringLiteral("flashEnabled")).toBool());
    m_flashCombo->setCurrentText(o.value(QStringLiteral("flashAttn")).toString(QStringLiteral("on")));
    m_nglCheck->setChecked(o.value(QStringLiteral("nglEnabled")).toBool(true));
    m_nglSpin->setValue(o.value(QStringLiteral("ngl")).toInt(99));
    m_noMmapCheck->setChecked(o.value(QStringLiteral("noMmap")).toBool());
    m_cpuMoeCheck->setChecked(o.value(QStringLiteral("cpuMoe")).toBool());
    m_specCheck->setChecked(o.value(QStringLiteral("specEnabled")).toBool(false));
    m_specSpin->setValue(o.value(QStringLiteral("specDraftNMax")).toInt(2));
    m_reasoningCheck->setChecked(o.value(QStringLiteral("reasoningEnabled")).toBool());
    m_reasoningCombo->setCurrentText(o.value(QStringLiteral("reasoning")).toString(QStringLiteral("auto")));
    m_splitCheck->setChecked(o.value(QStringLiteral("splitEnabled")).toBool());
    m_splitCombo->setCurrentText(o.value(QStringLiteral("splitMode")).toString(QStringLiteral("layer")));
    m_mmapLoadCheck->setChecked(o.value(QStringLiteral("mmapLoad")).toBool());
    // 兼容旧版配置里的 cacheTypeK/cacheTypeV（旧版始终输出 q4_0）
    m_cacheKCheck->setChecked(o.value(QStringLiteral("cacheKEnabled")).toBool(false));
    m_cacheKCombo->setCurrentText(o.value(QStringLiteral("cacheK"))
                                      .toString(o.value(QStringLiteral("cacheTypeK")).toString(QStringLiteral("f16"))));
    m_cacheVCheck->setChecked(o.value(QStringLiteral("cacheVEnabled")).toBool(false));
    m_cacheVCombo->setCurrentText(o.value(QStringLiteral("cacheV"))
                                      .toString(o.value(QStringLiteral("cacheTypeV")).toString(QStringLiteral("f16"))));
    m_localOnlyCheck->setChecked(o.value(QStringLiteral("localOnly")).toBool(true));
    m_listenEdit->setText(o.value(QStringLiteral("listen")).toString(QStringLiteral("127.0.0.1")));
    m_portEdit->setText(o.value(QStringLiteral("port")).toString(QStringLiteral("8090")));
    m_keyEdit->setText(o.value(QStringLiteral("apiKey")).toString());
    m_modelIdEdit->setText(o.value(QStringLiteral("modelId")).toString());
    m_webuiDisabled = o.value(QStringLiteral("webuiDisabled")).toBool();
    m_corsCheck->setChecked(o.value(QStringLiteral("cors")).toBool(false));
    m_closeWebBtn->setText(m_webuiDisabled ? QStringLiteral("开启 llama.cpp 的web访问")
                                           : QStringLiteral("关闭 llama.cpp 的web访问"));

    refreshAll();
    m_runLog->appendPlainText(QStringLiteral("[GUI] 已引入启动参数: %1").arg(path));
}

void MainWindow::parseArgsText()
{
    const QString text = QInputDialog::getMultiLineText(
        this, QStringLiteral("动态解析启动参数"),
        QStringLiteral("粘贴 llama-server 命令行参数（支持 -m / -c / -t / -ngl / -fa / -sm / "
                       "-ctk / -ctv / --host / --port / --api-key / -a / --no-webui 等）:"),
        buildServerArgs().join(QLatin1Char(' ')));
    if (text.trimmed().isEmpty())
        return;

    const QStringList tokens = text.simplified().split(QLatin1Char(' '));
    QMap<QString, QString> opt;
    QStringList flags, unknown;
    for (int i = 0; i < tokens.size(); ++i) {
        const QString tk = tokens[i];
        if (!tk.startsWith(QLatin1Char('-')))
            continue;
        QString key = tk;
        while (key.startsWith(QLatin1Char('-')))
            key.remove(0, 1);
        // 长短名归一
        static const QMap<QString, QString> alias = {
            {QStringLiteral("model"), QStringLiteral("m")},
            {QStringLiteral("mmproj"), QStringLiteral("mm")},
            {QStringLiteral("ctx-size"), QStringLiteral("c")},
            {QStringLiteral("threads"), QStringLiteral("t")},
            {QStringLiteral("flash-attn"), QStringLiteral("fa")},
            {QStringLiteral("gpu-layers"), QStringLiteral("ngl")},
            {QStringLiteral("n-gpu-layers"), QStringLiteral("ngl")},
            {QStringLiteral("split-mode"), QStringLiteral("sm")},
            {QStringLiteral("cache-type-k"), QStringLiteral("ctk")},
            {QStringLiteral("cache-type-v"), QStringLiteral("ctv")},
            {QStringLiteral("alias"), QStringLiteral("a")},
            {QStringLiteral("rea"), QStringLiteral("reasoning")},
        };
        if (alias.contains(key))
            key = alias[key];
        if (key == QLatin1String("no-webui") || key == QLatin1String("no-mmap") ||
            key == QLatin1String("mmap") || key == QLatin1String("cpu-moe")) {
            flags << key;
        } else if (i + 1 < tokens.size() && !tokens[i + 1].startsWith(QLatin1Char('-'))) {
            opt.insert(key, tokens[++i]);
        } else {
            unknown << tk;
        }
    }

    if (opt.contains(QStringLiteral("m"))) {
        const QString mp = QDir::fromNativeSeparators(opt[QStringLiteral("m")]);
        m_modelDir->setText(QDir::toNativeSeparators(QFileInfo(mp).absolutePath()));
        scanModels();
        selectModelFile(mp);
    }
    if (opt.contains(QStringLiteral("mm")))
        m_mmprojPath->setText(QDir::toNativeSeparators(opt[QStringLiteral("mm")]));
    if (opt.contains(QStringLiteral("c"))) {
        m_ctxCheck->setChecked(true);
        m_ctxSpin->setValue(opt[QStringLiteral("c")].toInt());
    }
    if (opt.contains(QStringLiteral("t"))) {
        m_threadsCheck->setChecked(true);
        m_threadsSpin->setValue(opt[QStringLiteral("t")].toInt());
    }
    if (opt.contains(QStringLiteral("fa"))) {
        m_flashCheck->setChecked(true);
        m_flashCombo->setCurrentText(opt[QStringLiteral("fa")]);
    }
    if (opt.contains(QStringLiteral("ngl"))) {
        m_nglCheck->setChecked(true);
        m_nglSpin->setValue(opt[QStringLiteral("ngl")].toInt());
    }
    if (opt.contains(QStringLiteral("sm"))) {
        m_splitCheck->setChecked(true);
        m_splitCombo->setCurrentText(opt[QStringLiteral("sm")]);
    }
    if (opt.contains(QStringLiteral("ctk"))) {
        m_cacheKCheck->setChecked(true);
        m_cacheKCombo->setCurrentText(opt[QStringLiteral("ctk")]);
    }
    if (opt.contains(QStringLiteral("ctv"))) {
        m_cacheVCheck->setChecked(true);
        m_cacheVCombo->setCurrentText(opt[QStringLiteral("ctv")]);
    }
    if (opt.contains(QStringLiteral("host"))) {
        const QString h = opt[QStringLiteral("host")];
        m_localOnlyCheck->setChecked(h == QLatin1String("127.0.0.1"));
        m_listenEdit->setText(h);
    }
    if (opt.contains(QStringLiteral("port")))
        m_portEdit->setText(opt[QStringLiteral("port")]);
    if (opt.contains(QStringLiteral("api-key")))
        m_keyEdit->setText(opt[QStringLiteral("api-key")]);
    if (opt.contains(QStringLiteral("a")))
        m_modelIdEdit->setText(opt[QStringLiteral("a")]);
    if (opt.contains(QStringLiteral("cors-origins")))
        m_corsCheck->setChecked(opt[QStringLiteral("cors-origins")] != QLatin1String("localhost"));
    if (flags.contains(QLatin1String("no-webui"))) {
        m_webuiDisabled = true;
        m_closeWebBtn->setText(QStringLiteral("开启 llama.cpp 的web访问"));
    }
    if (flags.contains(QLatin1String("no-mmap")))
        m_noMmapCheck->setChecked(true);
    if (flags.contains(QLatin1String("cpu-moe")))
        m_cpuMoeCheck->setChecked(true);
    if (opt.contains(QStringLiteral("reasoning"))) {
        m_reasoningCheck->setChecked(true);
        m_reasoningCombo->setCurrentText(opt[QStringLiteral("reasoning")]);
    }
    if (flags.contains(QLatin1String("mmap")))
        m_mmapLoadCheck->setChecked(true);

    refreshAll();
    QString msg = QStringLiteral("已解析并应用 %1 个参数选项").arg(opt.size() + flags.size());
    if (!unknown.isEmpty())
        msg += QStringLiteral("\n未识别(忽略): %1").arg(unknown.join(QLatin1Char(' ')));
    QMessageBox::information(this, QStringLiteral("动态解析"), msg);
}

// ---------------------------------------------------------------- server control

bool MainWindow::ensureCanStart(QString *reason) const
{
    const QString exe = QDir(m_toolPath->text()).filePath(QStringLiteral("llama-server.exe"));
    if (!QFile::exists(exe)) {
        if (reason) *reason = QStringLiteral("未找到 llama-server.exe，请检查「Lamma.cpp工具地址」。\n%1").arg(exe);
        return false;
    }
    const QString model = modelFilePath();
    if (model.isEmpty() || !QFile::exists(model)) {
        if (reason) *reason = QStringLiteral("模型文件不存在，请选择有效的 GGUF 模型。\n%1").arg(model);
        return false;
    }
    return true;
}

void MainWindow::startServer()
{
    QString reason;
    if (!ensureCanStart(&reason)) {
        QMessageBox::warning(this, QStringLiteral("无法启动"), reason);
        return;
    }
    if (m_running)
        return;

    const QString exe = QDir(m_toolPath->text()).filePath(QStringLiteral("llama-server.exe"));
    const QStringList args = buildServerArgs();
    m_serverLog->appendPlainText(QStringLiteral("[GUI] 启动: %1 %2")
                                     .arg(exe, args.join(QLatin1Char(' '))));
    m_server->setWorkingDirectory(m_toolPath->text());
    m_server->start(exe, args);
    setRunningUi(true);
}

void MainWindow::stopServer()
{
    if (!m_running && m_server->state() == QProcess::NotRunning)
        return;
    m_serverLog->appendPlainText(QStringLiteral("[GUI] 正在停止服务..."));
    // Windows 上 console 子进程没有窗口可接收 WM_CLOSE，terminate() 无效，直接 kill
    m_stopping = true;
    m_server->kill();
    m_server->waitForFinished(3000);
    m_stopping = false;
    setRunningUi(false);
    m_serverLog->appendPlainText(QStringLiteral("[GUI] 服务已停止。"));
}

void MainWindow::restartServer()
{
    stopServer();
    QTimer::singleShot(500, this, [this] { if (!m_running) startServer(); });
}

void MainWindow::setRunningUi(bool running)
{
    m_running = running;
    bool portOk = false;
    const int port = m_portEdit->text().toInt(&portOk);
    if (running) {
        m_startPanel->setText(QStringLiteral("● llama-server 运行中\n端口 %1\n点击停止服务")
                                  .arg(portOk ? port : 8090));
        m_startPanel->setStyleSheet(QStringLiteral(
            "QPushButton{background:#16351f;border:1px solid #2e6b3c;border-radius:2px;"
            "color:#7fe08a;font-size:10pt;font-weight:600;}"));
        // webui 已关闭时没有页面可打开，保持禁用
        m_openWebBtn->setEnabled(!m_webuiDisabled);
        m_openWebBtn->setText(QStringLiteral("打开 %1").arg(webUiUrl()));
    } else {
        m_startPanel->setText(QStringLiteral("点击启动 llama-server"));
        m_startPanel->setStyleSheet(QStringLiteral(
            "QPushButton{background:#1b2028;border:1px solid #14181f;border-radius:2px;"
            "color:#56606e;font-size:10pt;}"));
        m_openWebBtn->setEnabled(false);
        m_openWebBtn->setText(QStringLiteral("打开 Web 网页"));
    }
}

QString MainWindow::webUiUrl() const
{
    return m_urlEdit->text().remove(QLatin1String("/v1"));
}

// ---------------------------------------------------------------- log parsing

void MainWindow::handleServerLine(const QString &line)
{
    m_serverLog->appendPlainText(line);

    // llama-server 监听成功
    if (!m_running && line.contains(QStringLiteral("server is listening"))) {
        setRunningUi(true);
        m_serverLog->appendPlainText(QStringLiteral("[GUI] 服务已就绪，API 地址: %1").arg(m_urlEdit->text()));
    }

    // slot 级统计：print_timing 行直接进入运行日志，并从中提取速度信息
    if (line.contains(QStringLiteral("print_timing")))
        m_runLog->appendPlainText(line);

    int slotId = -1;
    QRegularExpressionMatch m = QRegularExpression(QStringLiteral("id\\s+(\\d+)")).match(line);
    if (m.hasMatch())
        slotId = m.captured(1).toInt();

    // 提示词处理统计（本版本位于 print_timing 行内，也可能是独立汇总行）
    // 格式: prompt eval time = 15.44 ms / 60 tokens ( 3.88 ms per token, 257.77 tokens per second)
    m = QRegularExpression(QStringLiteral(
        "prompt eval time\\s*=\\s*[\\d.]+\\s*ms\\s*/\\s*(\\d+)\\s*tokens"
        "\\s*\\(\\s*[\\d.]+\\s*ms per token,\\s*([\\d.]+)\\s*tokens per second"))
            .match(line);
    if (!m.hasMatch())
        m = QRegularExpression(QStringLiteral(
            "prompt eval time\\s*=\\s*[\\d.]+\\s*ms\\s*/\\s*(\\d+)\\s*tokens\\s*\\(\\s*([\\d.]+)"))
                .match(line);
    if (m.hasMatch()) {
        updateTaskPromptStats(slotId < 0 ? 0 : slotId, m.captured(2).toDouble(), m.captured(1).toInt());
        return;
    }

    // 生成阶段统计（避免误匹配 "prompt eval time"）
    if (line.contains(QLatin1String("eval time")) && !line.contains(QLatin1String("prompt eval time"))) {
        m = QRegularExpression(QStringLiteral(
            "eval time\\s*=\\s*[\\d.]+\\s*ms\\s*/\\s*(\\d+)\\s*tokens"
            "\\s*\\(\\s*[\\d.]+\\s*ms per token,\\s*([\\d.]+)\\s*tokens per second"))
                .match(line);
        if (!m.hasMatch())
            m = QRegularExpression(QStringLiteral(
                "eval time\\s*=\\s*[\\d.]+\\s*ms\\s*/\\s*(\\d+)\\s*tokens\\s*\\(\\s*([\\d.]+)"))
                    .match(line);
        if (m.hasMatch()) {
            updateTaskGenStats(slotId < 0 ? 0 : slotId, m.captured(2).toDouble(), m.captured(1).toInt());
            return;
        }
    }

    // 参考图格式: n_gen =  N, tg =  X t/s
    if (line.contains(QLatin1String("n_gen"))) {
        int nGen = 0;
        double tg = 0;
        m = QRegularExpression(QStringLiteral("n_gen\\s*=\\s*(\\d+)")).match(line);
        if (m.hasMatch())
            nGen = m.captured(1).toInt();
        m = QRegularExpression(QStringLiteral("tg\\s*=\\s*([\\d.]+)\\s*t/s")).match(line);
        if (m.hasMatch())
            tg = m.captured(1).toDouble();
        if (nGen > 0 || tg > 0)
            updateTaskGenStats(slotId < 0 ? 0 : slotId, tg, nGen);
    }
}

void MainWindow::updateTaskPromptStats(int slotId, double speed, int tokens)
{
    QVector<double> st = m_taskStats.value(slotId);
    if (st.isEmpty()) st = QVector<double>{0, 0, 0, 0, 0, 0};
    st[0] = speed;
    st[1] = tokens;
    st[4] = qMax(st[4], double(tokens));
    m_taskStats.insert(slotId, st);
    refreshTaskInfo();
}

void MainWindow::updateTaskGenStats(int slotId, double speed, int tokens)
{
    QVector<double> st = m_taskStats.value(slotId);
    if (st.isEmpty()) st = QVector<double>{0, 0, 0, 0, 0, 0};
    if (speed > 0) st[2] = speed;
    if (tokens > 0) st[3] = tokens;
    st[5] = qMax(st[5], double(tokens));
    m_taskStats.insert(slotId, st);
    refreshTaskInfo();
}

QString MainWindow::buildTaskLine(int id) const
{
    const QVector<double> st = m_taskStats.value(id);
    auto v = [&st](int i) { return st.isEmpty() ? 0.0 : st[i]; };
    return QStringLiteral(
        "Task %1: 提示词处理: 速度约%2 tokens/秒, 共%3 tokens; 生成阶段: 速度约%4 tokens/秒, "
        "共%5 tokens; 总输入: %6 tokens, 总输出: %7 tokens")
        .arg(id)
        .arg(v(0), 0, 'f', 2)
        .arg(int(v(1)))
        .arg(v(2), 0, 'f', 2)
        .arg(int(v(3)))
        .arg(int(v(4)))
        .arg(int(v(5)));
}

void MainWindow::refreshTaskInfo()
{
    QList<int> ids = m_taskStats.keys();
    std::sort(ids.begin(), ids.end());
    if (ids.isEmpty()) {
        // 无数据时显示灰色提示
        m_taskInfo->setStyleSheet(QStringLiteral(
            "QPlainTextEdit{background:#ffffff;border:1px solid #c9ccd4;color:#898989;}"
            "QScrollBar:vertical{width:14px;}"));
        m_taskInfo->setPlainText(kTaskHint);
        return;
    }
    QString text;
    for (int id : ids)
        text += buildTaskLine(id) + QLatin1Char('\n');
    m_taskInfo->setStyleSheet(QStringLiteral(
        "QPlainTextEdit{background:#ffffff;border:1px solid #c9ccd4;color:#000000;}"
        "QScrollBar:vertical{width:14px;}"));
    m_taskInfo->setPlainText(text.trimmed());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_server->state() != QProcess::NotRunning) {
        m_stopping = true; // 关闭窗口时的 kill 同样不弹窗
        m_server->kill();
        m_server->waitForFinished(2000);
    }
    event->accept();
}
