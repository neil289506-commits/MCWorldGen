#include "core/WorldGenController.h"
#include "core/AssetImporter.h"
#include "core/ServerPropertiesWriter.h"
#include "core/ExportPackager.h"
#include <QDir>
#include <QDateTime>

WorldGenController::WorldGenController(QObject *parent) : QObject(parent)
{
}

void WorldGenController::enterStage(int step, const QString &name)
{
    emit stageChanged(name);
    emit overallProgress(step, kTotalSteps, name);
}

void WorldGenController::runFullPipeline(const WorldGenConfig &config)
{
    m_config = config;

    // 每次執行使用獨立子目錄，避免多次執行互相污染，路徑落在 %TEMP% 底下
    const QString tempRoot = QDir::tempPath(); // Windows 上對應 %TMP%/%TEMP%
    m_pregenDir = QString("%1/worldgen-pregen-%2")
        .arg(tempRoot, QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"));
    QDir().mkpath(m_pregenDir);

    emit consoleLine(QStringLiteral(">>> 預生成暫存資料夾：%1").arg(m_pregenDir));
    stepInstallCore();
}

void WorldGenController::stepInstallCore()
{
    enterStage(1, QStringLiteral("下載並安裝伺服器核心"));

    m_installer = new ServerCoreInstaller(this);
    connect(m_installer, &ServerCoreInstaller::progressMessage, this, [this](const QString &msg) {
        emit consoleLine(QStringLiteral(">>> %1").arg(msg));
    });
    connect(m_installer, &ServerCoreInstaller::progressPercent, this, [this](int pct) {
        if (pct >= 0)
            emit consoleLine(QStringLiteral(">>> 進度：%1%").arg(pct));
    });
    connect(m_installer, &ServerCoreInstaller::installFinished, this,
            [this](bool success, const QString &message, const LaunchSpec &spec) {
        emit consoleLine(QStringLiteral(">>> %1").arg(message));
        if (!success) {
            emit pipelineFinished(false, message);
            return;
        }
        m_launchSpec = spec;
        stepImportAssets();
    });

    m_installer->install(m_config, m_pregenDir);
}

void WorldGenController::stepImportAssets()
{
    enterStage(2, QStringLiteral("匯入 Mods 與 Datapack"));

    const QStringList failed = AssetImporter::importAll(m_config, m_pregenDir);
    for (const QString &f : failed)
        emit consoleLine(QStringLiteral(">>> [警告] 匯入失敗：%1").arg(f));

    emit consoleLine(QStringLiteral(">>> Mods/Datapack 匯入完成（共 %1 個失敗）").arg(failed.size()));
    stepWriteProperties();
}

void WorldGenController::stepWriteProperties()
{
    enterStage(3, QStringLiteral("寫入 server.properties"));

    if (!ServerPropertiesWriter::write(m_config, m_pregenDir)) {
        emit pipelineFinished(false, QStringLiteral("無法寫入 server.properties"));
        return;
    }
    emit consoleLine(QStringLiteral(">>> server.properties 已寫入"));
    stepRunPreGen();
}

void WorldGenController::stepRunPreGen()
{
    enterStage(4, QStringLiteral("預生成世界"));

    m_pregenRunner = new PreGenRunner(this);
    connect(m_pregenRunner, &PreGenRunner::consoleLine, this, &WorldGenController::consoleLine);
    connect(m_pregenRunner, &PreGenRunner::pregenFinished, this, [this](bool success, const QString &msg) {
        emit consoleLine(QStringLiteral(">>> %1").arg(msg));
        if (!success) {
            emit pipelineFinished(false, msg);
            return;
        }
        stepExport();
    });

    m_pregenRunner->start(m_config, m_launchSpec);
}

void WorldGenController::stepExport()
{
    enterStage(5, QStringLiteral("打包匯出"));

    if (m_config.exportZipPath.isEmpty()) {
        emit pipelineFinished(false, QStringLiteral("尚未選擇匯出路徑"));
        return;
    }

    // 只擷取世界資料夾本身，server.jar/mods/installer 等其他暫存檔案不匯出
    const QString worldDir = m_pregenDir + "/" + m_config.levelName;
    if (!QDir(worldDir).exists()) {
        emit pipelineFinished(false, QStringLiteral("找不到世界資料夾：%1").arg(worldDir));
        return;
    }

    emit consoleLine(QStringLiteral(">>> 開始以 BZip2 壓縮打包世界資料夾：%1").arg(worldDir));

    QString err;
    const bool ok = ExportPackager::packDirectoryAsBzip2Zip(worldDir, m_config.exportZipPath, &err);
    if (!ok) {
        emit pipelineFinished(false, QStringLiteral("匯出失敗：%1").arg(err));
        return;
    }

    emit consoleLine(QStringLiteral(">>> 匯出完成：%1").arg(m_config.exportZipPath));

    cleanupTempDir();

    emit pipelineFinished(true, QStringLiteral("全部流程完成"));
}

void WorldGenController::cleanupTempDir()
{
    emit consoleLine(QStringLiteral(">>> 清除暫存資料夾：%1").arg(m_pregenDir));
    QDir dir(m_pregenDir);
    if (dir.exists() && !dir.removeRecursively()) {
        emit consoleLine(QStringLiteral(
            ">>> [警告] 暫存資料夾未能完全清除，可能有檔案仍被佔用（例如 java 尚未完全釋放），可稍後手動刪除：%1")
            .arg(m_pregenDir));
    }
}
