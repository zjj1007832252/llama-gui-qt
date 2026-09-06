#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QMap>
#include <QVector>

class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QProcess;
class QSpinBox;
class QPlainTextEdit;
class QPushButton;

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    // ---- UI builders ----
    QWidget *createModelPanel();
    QWidget *createParamPanel();
    QWidget *createCmdPanel();
    QWidget *createServerPage();
    void wireLogic();

    // ---- command generation / linked fields ----
    QString modelFilePath() const;
    QStringList buildServerArgs() const;
    void refreshCmdInfo();
    void refreshApiUrl();
    void refreshModelId();
    void refreshCtxInfo();
    void refreshAll();

    // ---- model scanning / defaults ----
    void scanModels();
    void selectModelFile(const QString &path);
    void autoDetectPaths();

    // ---- server process ----
    bool ensureCanStart(QString *reason = nullptr) const;
    void startServer();
    void stopServer();
    void restartServer();
    void setRunningUi(bool running);
    QString webUiUrl() const;
    void handleServerLine(const QString &line);
    void updateTaskPromptStats(int slotId, double speed, int tokens);
    void updateTaskGenStats(int slotId, double speed, int tokens);
    void refreshTaskInfo();

    // ---- button actions ----
    void browseToolPath();
    void browseModelDir();
    void browseMmproj();
    void generateScript();
    void chatTest();
    void bestParams();
    void saveParams();
    void loadParams();
    void parseArgsText();

    // ---- left: model panel ----
    QLineEdit *m_toolPath = nullptr;
    QLineEdit *m_modelDir = nullptr;
    QComboBox *m_modelCombo = nullptr;
    QLineEdit *m_mmprojPath = nullptr;

    // ---- left: param panel ----
    QCheckBox *m_ctxCheck = nullptr;    QSpinBox *m_ctxSpin = nullptr;   QLineEdit *m_ctxInfo = nullptr;
    QCheckBox *m_threadsCheck = nullptr; QSpinBox *m_threadsSpin = nullptr;
    QCheckBox *m_flashCheck = nullptr;  QComboBox *m_flashCombo = nullptr;
    QCheckBox *m_nglCheck = nullptr;    QSpinBox *m_nglSpin = nullptr;
    QCheckBox *m_noMmapCheck = nullptr;
    QCheckBox *m_cpuMoeCheck = nullptr;
    QCheckBox *m_specCheck = nullptr;   QSpinBox *m_specSpin = nullptr;
    QCheckBox *m_reasoningCheck = nullptr;  QComboBox *m_reasoningCombo = nullptr;
    QCheckBox *m_splitCheck = nullptr;  QComboBox *m_splitCombo = nullptr;
    QCheckBox *m_mmapLoadCheck = nullptr;
    QCheckBox *m_cacheKCheck = nullptr;  QComboBox *m_cacheKCombo = nullptr;
    QCheckBox *m_cacheVCheck = nullptr;  QComboBox *m_cacheVCombo = nullptr;

    // ---- left: command info + buttons ----
    QPlainTextEdit *m_cmdInfo = nullptr;

    // ---- right: server tab ----
    QLineEdit *m_listenEdit = nullptr;
    QCheckBox *m_localOnlyCheck = nullptr;
    QLineEdit *m_portEdit = nullptr;
    QCheckBox *m_corsCheck = nullptr;
    QLineEdit *m_urlEdit = nullptr;
    QLineEdit *m_keyEdit = nullptr;
    QLineEdit *m_modelIdEdit = nullptr;
    QPushButton *m_startPanel = nullptr;
    QPushButton *m_openWebBtn = nullptr;
    QPushButton *m_closeWebBtn = nullptr;
    bool m_webuiDisabled = false;

    // ---- right: info + logs ----
    QPlainTextEdit *m_taskInfo = nullptr;
    QPlainTextEdit *m_runLog = nullptr;
    QPlainTextEdit *m_testLog = nullptr;
    QPlainTextEdit *m_serverLog = nullptr;

    // ---- server runtime ----
    QProcess *m_server = nullptr;
    bool m_running = false;
    bool m_stopping = false; // 用户主动停止中，忽略由此产生的 Crashed 信号
    QMap<int, QVector<double>> m_taskStats; // id -> {ppSpeed, ppTokens, tgSpeed, tgTokens, in, out}
    QString buildTaskLine(int id) const;
};

#endif // MAINWINDOW_H
