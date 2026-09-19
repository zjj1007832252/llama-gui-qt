#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QMap>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
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
    void browseSpecDraftModel();
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

    // ---- left: speculative decoding tab ----
    QCheckBox *m_specTypeCheck = nullptr;    QComboBox *m_specTypeCombo = nullptr;
    QCheckBox *m_specNMaxCheck = nullptr;    QSpinBox *m_specNMaxSpin = nullptr;
    QCheckBox *m_specNMinCheck = nullptr;    QSpinBox *m_specNMinSpin = nullptr;
    QCheckBox *m_specModelCheck = nullptr;   QLineEdit *m_specModelEdit = nullptr;
    QCheckBox *m_specNglCheck = nullptr;     QLineEdit *m_specNglEdit = nullptr;
    QCheckBox *m_specThreadsCheck = nullptr; QSpinBox *m_specThreadsSpin = nullptr;
    QCheckBox *m_specCpuMoeCheck = nullptr;
    QCheckBox *m_specPSplitCheck = nullptr;  QDoubleSpinBox *m_specPSplitSpin = nullptr;
    QCheckBox *m_specPMinCheck = nullptr;    QDoubleSpinBox *m_specPMinSpin = nullptr;
    QCheckBox *m_ngModNMinCheck = nullptr;   QSpinBox *m_ngModNMinSpin = nullptr;
    QCheckBox *m_ngModNMaxCheck = nullptr;   QSpinBox *m_ngModNMaxSpin = nullptr;
    QCheckBox *m_ngModNMatchCheck = nullptr; QSpinBox *m_ngModNMatchSpin = nullptr;
    QCheckBox *m_ngSimpleNCheck = nullptr;   QSpinBox *m_ngSimpleNSpin = nullptr;
    QCheckBox *m_ngSimpleMCheck = nullptr;   QSpinBox *m_ngSimpleMSpin = nullptr;
    QCheckBox *m_ngSimpleHitsCheck = nullptr; QSpinBox *m_ngSimpleHitsSpin = nullptr;
    QCheckBox *m_ngMapKNCheck = nullptr;     QSpinBox *m_ngMapKNSpin = nullptr;
    QCheckBox *m_ngMapKMCheck = nullptr;     QSpinBox *m_ngMapKMSpin = nullptr;
    QCheckBox *m_ngMapKHitsCheck = nullptr;  QSpinBox *m_ngMapKHitsSpin = nullptr;
    QCheckBox *m_specDefaultCheck = nullptr;

    // ---- left: batch / multi-gpu / parallel ----
    QCheckBox *m_batchCheck = nullptr;    QSpinBox *m_batchSpin = nullptr;
    QCheckBox *m_ubatchCheck = nullptr;   QSpinBox *m_ubatchSpin = nullptr;
    QCheckBox *m_tsCheck = nullptr;       QLineEdit *m_tsEdit = nullptr;
    QCheckBox *m_mainGpuCheck = nullptr;  QSpinBox *m_mainGpuSpin = nullptr;
    QCheckBox *m_parallelCheck = nullptr; QSpinBox *m_parallelSpin = nullptr;

    // ---- left: sampling params ----
    QCheckBox *m_tempCheck = nullptr;     QDoubleSpinBox *m_tempSpin = nullptr;
    QCheckBox *m_topKCheck = nullptr;     QSpinBox *m_topKSpin = nullptr;
    QCheckBox *m_topPCheck = nullptr;     QDoubleSpinBox *m_topPSpin = nullptr;
    QCheckBox *m_minPCheck = nullptr;     QDoubleSpinBox *m_minPSpin = nullptr;
    QCheckBox *m_repPenCheck = nullptr;   QDoubleSpinBox *m_repPenSpin = nullptr;
    QCheckBox *m_seedCheck = nullptr;     QSpinBox *m_seedSpin = nullptr;

    // ---- left: service / logging ----
    QCheckBox *m_metricsCheck = nullptr;
    QCheckBox *m_timeoutCheck = nullptr;  QSpinBox *m_timeoutSpin = nullptr;
    QCheckBox *m_logFileCheck = nullptr;  QLineEdit *m_logFileEdit = nullptr;
    QCheckBox *m_chatTmplCheck = nullptr; QComboBox *m_chatTmplCombo = nullptr;
    QCheckBox *m_noJinjaCheck = nullptr;
    QCheckBox *m_thinkBudgetCheck = nullptr; QSpinBox *m_thinkBudgetSpin = nullptr;
    QCheckBox *m_reasoningCheck = nullptr;  QComboBox *m_reasoningCombo = nullptr;
    QCheckBox *m_splitCheck = nullptr;  QComboBox *m_splitCombo = nullptr;
    QCheckBox *m_mmapLoadCheck = nullptr;
    QCheckBox *m_cacheKCheck = nullptr;  QComboBox *m_cacheKCombo = nullptr;
    QCheckBox *m_cacheVCheck = nullptr;  QComboBox *m_cacheVCombo = nullptr;
    QCheckBox *m_keepCheck = nullptr;     QSpinBox *m_keepSpin = nullptr;
    QCheckBox *m_cacheRamCheck = nullptr; QSpinBox *m_cacheRamSpin = nullptr;
    QCheckBox *m_ctxCpCheck = nullptr;    QSpinBox *m_ctxCpSpin = nullptr;
    QCheckBox *m_ctxShiftCheck = nullptr;
    QCheckBox *m_kvuCheck = nullptr;

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
