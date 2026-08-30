#pragma once
#include <QObject>
#include "WorldGenTypes.h"
#include "ServerCoreInstaller.h"
#include "PreGenRunner.h"

// 整條流程的協調者，UI 只需要呼叫 runFullPipeline()，
// 並監聽 consoleLine / stageChanged / overallProgress / pipelineFinished 即可
class WorldGenController : public QObject
{
    Q_OBJECT
public:
    explicit WorldGenController(QObject *parent = nullptr);

    void runFullPipeline(const WorldGenConfig &config);

signals:
    void consoleLine(const QString &line);
    void stageChanged(const QString &stageName);
    void overallProgress(int currentStep, int totalSteps, const QString &stageName);
    void pipelineFinished(bool success, const QString &message);

private:
    void stepInstallCore();
    void stepImportAssets();
    void stepWriteProperties();
    void stepRunPreGen();
    void stepExport();
    void cleanupTempDir();
    void enterStage(int step, const QString &name); // 同時發出 stageChanged 與 overallProgress

    WorldGenConfig m_config;
    QString m_pregenDir;    // %TEMP%/worldgen-pregen-<時間戳>
    LaunchSpec m_launchSpec;

    ServerCoreInstaller *m_installer = nullptr;
    PreGenRunner *m_pregenRunner = nullptr;

    static constexpr int kTotalSteps = 5; // 安裝核心/匯入資源/寫設定/預生成/匯出
};
