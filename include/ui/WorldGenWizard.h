#pragma once
#include <QWizard>
#include <QWizardPage>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QProgressBar>
#include <QMap>
#include "core/WorldGenTypes.h"
#include "core/VersionFetcher.h"
#include "ui/ConsoleWidget.h"

// 頁面 1：選擇載入器 / 載入器版本 / 遊戲版本
// 三種版本選擇模式（依載入器特性不同）：
//   - Vanilla / Paper：只有一個「遊戲版本」下拉選單
//   - Fabric / Quilt：遊戲版本、載入器版本各自獨立的兩個下拉選單
//   - Forge / NeoForge：兩者成對出現，先選遊戲版本會篩選對應的載入器版本清單
class LoaderPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit LoaderPage(QWidget *parent = nullptr);
    ModloaderType selectedLoader() const;
    QString selectedGameVersion() const { return m_gameVersionCombo->currentData().toString(); }
    QString selectedLoaderVersion() const { return m_loaderVersionCombo->currentData().toString(); }

private:
    QComboBox *m_loaderCombo;
    QComboBox *m_gameVersionCombo;
    QComboBox *m_loaderVersionCombo;
    ConsoleWidget *m_fetchConsole; // 顯示版本抓取進度（VersionFetcher::fetchProgress/fetchError）
    VersionFetcher *m_fetcher;

    // VersionFetcher 的訊號是非同步的，且 fetchAll() 一次會陸續收到
    // 5 種載入器的資料，因此全部先快取起來，等使用者切換載入器下拉選單時
    // 再依快取內容重新填值，不用每次都重新發一次網路請求
    QMap<ModloaderType, QList<ModloaderVersion>> m_versionsCache;      // Fabric/Quilt/Forge/NeoForge 用；Vanilla/Paper 也共用同一份存 mcVersion 清單
    QMap<ModloaderType, QStringList> m_gameVersionsCache;              // Fabric/Quilt 專用的獨立遊戲版本清單
    bool m_bulkFetchStarted = false;

    void onLoaderChanged();
    void onGameVersionChanged();
    void applyModeForCurrentLoader();
};

// 頁面 2：世界類型與旗標選項
class WorldTypePage : public QWizardPage
{
    Q_OBJECT
public:
    explicit WorldTypePage(QWidget *parent = nullptr);
    WorldType selectedWorldType() const;
    bool hardcore() const { return m_hardcoreCheck->isChecked(); }
    bool bonusChest() const { return m_bonusChestCheck->isChecked(); }
    bool generateStructures() const { return m_structuresCheck->isChecked(); }
    QString seed() const { return m_seedEdit->text(); }

private:
    QComboBox *m_worldTypeCombo;
    QCheckBox *m_hardcoreCheck;
    QCheckBox *m_bonusChestCheck;
    QCheckBox *m_structuresCheck;
    QLineEdit *m_seedEdit;
};

// 頁面 3：Mods / Datapack 一鍵匯入
class AssetsPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit AssetsPage(QWidget *parent = nullptr);
    QStringList modFiles() const;
    QStringList datapackFiles() const;

private:
    QListWidget *m_modList;
    QListWidget *m_datapackList;
};

// 頁面 4：執行預生成 + 即時 console + 匯出路徑選擇
class RunExportPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit RunExportPage(QWidget *parent = nullptr);
    void initializePage() override;
    bool isComplete() const override { return m_finished; }

signals:
    void requestRun(const QString &exportPath, const QString &javaExecutable);

private:
    ConsoleWidget *m_console;
    QProgressBar *m_stageBusyBar;   // 目前階段的忙碌指示（不確定進度時滾動）
    QProgressBar *m_totalProgressBar; // 總進度：依 6 個步驟換算百分比
    QLabel *m_stageLabel;
    QLineEdit *m_exportPathEdit;
    QLineEdit *m_javaExecutableEdit;
    bool m_finished = false;

public slots:
    void onConsoleLine(const QString &line) { m_console->appendLine(line); }
    void onStageChanged(const QString &stage);
    void onOverallProgress(int currentStep, int totalSteps, const QString &stageName);
    void onPipelineFinished(bool success, const QString &message);
};

// 整合所有頁面並串接 WorldGenController 的主精靈
class WorldGenWizard : public QWizard
{
    Q_OBJECT
public:
    explicit WorldGenWizard(QWidget *parent = nullptr);

private:
    LoaderPage *m_loaderPage;
    WorldTypePage *m_worldTypePage;
    AssetsPage *m_assetsPage;
    RunExportPage *m_runExportPage;

    void startPipeline(const QString &exportPath, const QString &javaExecutable);
};
