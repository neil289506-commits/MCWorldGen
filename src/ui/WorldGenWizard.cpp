#include "ui/WorldGenWizard.h"
#include "core/WorldGenController.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QLabel>
#include <QDirIterator>
#include <QFileInfo>

namespace {

QString quoteCommandArg(const QString &value)
{
    if (value.isEmpty())
        return value;
    const QString trimmed = value.trimmed();
    if (trimmed.startsWith('"') && trimmed.endsWith('"') && trimmed.size() >= 2)
        return trimmed;
    return trimmed.contains(' ') ? QString("\"") + trimmed + QString("\"") : trimmed;
}

QString defaultJavaExecutable()
{
    const QStringList roots = {
        QStringLiteral("C:/Program Files/Java"),
        QStringLiteral("C:/Program Files (x86)/Java")
    };

    for (const QString &root : roots) {
        QDir dir(root);
        if (!dir.exists())
            continue;

        QDirIterator it(root, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString fileName = QFileInfo(path).fileName();
            if (fileName.compare(QStringLiteral("java.exe"), Qt::CaseInsensitive) == 0)
                return path;
            if (fileName.compare(QStringLiteral("java"), Qt::CaseInsensitive) == 0)
                return path;
        }
    }
    return QStringLiteral("java");
}

} // namespace

// ---------------- LoaderPage ----------------

LoaderPage::LoaderPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle(QStringLiteral("選擇載入器與版本"));

    m_loaderCombo = new QComboBox;
    m_loaderCombo->addItem("Vanilla",  int(ModloaderType::Vanilla));
    m_loaderCombo->addItem("Fabric",   int(ModloaderType::Fabric));
    m_loaderCombo->addItem("Quilt",    int(ModloaderType::Quilt));
    m_loaderCombo->addItem("Forge",    int(ModloaderType::Forge));
    m_loaderCombo->addItem("NeoForge", int(ModloaderType::NeoForge));
    m_loaderCombo->addItem("Paper",    int(ModloaderType::Paper));

    m_gameVersionCombo = new QComboBox;
    m_loaderVersionCombo = new QComboBox;
    m_fetcher = new VersionFetcher(this);
    m_fetchConsole = new ConsoleWidget;
    m_fetchConsole->setFixedHeight(140);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("載入器："), m_loaderCombo);
    form->addRow(QStringLiteral("遊戲版本："), m_gameVersionCombo);
    form->addRow(QStringLiteral("載入器版本："), m_loaderVersionCombo);

    auto *layout = new QVBoxLayout;
    layout->addLayout(form);
    layout->addWidget(new QLabel(QStringLiteral("版本抓取狀態：")));
    layout->addWidget(m_fetchConsole);
    setLayout(layout);

    connect(m_loaderCombo, &QComboBox::currentTextChanged, this, [this]() { onLoaderChanged(); });
    connect(m_gameVersionCombo, &QComboBox::currentTextChanged, this, [this]() { onGameVersionChanged(); });

    connect(m_fetcher, &VersionFetcher::fetchProgress, this, [this](const QString &msg) {
        m_fetchConsole->appendLine(msg);
    });
    connect(m_fetcher, &VersionFetcher::fetchFinished, this, [this]() {
        m_fetchConsole->appendLine(QStringLiteral(">>> 版本清單下載完成，開始解析..."));
    });

    // fetchAll() 一次觸發 Python 腳本抓齊 Vanilla/Fabric/Quilt/Forge/NeoForge，
    // 每種來源解析完成後各自非同步發出 versionsReady/gameVersionsReady，
    // 全部先存進成員快取，等使用者切換載入器下拉選單時再依當下選擇重新套用到 UI
    connect(m_fetcher, &VersionFetcher::versionsReady, this,
            [this](ModloaderType type, const QList<ModloaderVersion> &versions) {
        m_versionsCache[type] = versions;
        m_fetchConsole->appendLine(QStringLiteral(">>> %1：已解析 %2 個版本")
                                    .arg(modloaderName(type)).arg(versions.size()));
        if (type == selectedLoader())
            applyModeForCurrentLoader();
    });
    connect(m_fetcher, &VersionFetcher::gameVersionsReady, this,
            [this](ModloaderType type, const QStringList &mcVersions) {
        m_gameVersionsCache[type] = mcVersions;
        if (type == selectedLoader())
            applyModeForCurrentLoader();
    });
    connect(m_fetcher, &VersionFetcher::fetchError, this, [this](const QString &reason) {
        m_fetchConsole->appendLine(QStringLiteral(">>> [錯誤] %1").arg(reason));
        qWarning("版本清單抓取失敗：%s", qUtf8Printable(reason));
    });

    onLoaderChanged();
}

ModloaderType LoaderPage::selectedLoader() const
{
    return static_cast<ModloaderType>(m_loaderCombo->currentData().toInt());
}

void LoaderPage::onLoaderChanged()
{
    const ModloaderType t = selectedLoader();

    if (t == ModloaderType::Paper) {
        if (!m_versionsCache.contains(ModloaderType::Paper))
            m_fetcher->fetchPaperVersions();
    } else if (!m_bulkFetchStarted) {
        // Vanilla/Fabric/Quilt/Forge/NeoForge 共用同一次 Python 批次抓取
        m_bulkFetchStarted = true;
        m_fetcher->fetchAll();
    }

    applyModeForCurrentLoader();
}

// 依目前選擇的載入器類型，決定要用哪種下拉選單填值方式，
// 並把已快取（若有）的資料實際套用進 UI
void LoaderPage::applyModeForCurrentLoader()
{
    const ModloaderType t = selectedLoader();

    m_gameVersionCombo->blockSignals(true);
    m_gameVersionCombo->clear();
    m_loaderVersionCombo->clear();

    if (t == ModloaderType::Vanilla || t == ModloaderType::Paper) {
        // 單一下拉選單：只需要遊戲版本，載入器版本停用
        m_loaderVersionCombo->setEnabled(false);
        for (const auto &mv : m_versionsCache.value(t)) {
            if (!mv.isStable) continue; // 只顯示正式版，略過快照版
            m_gameVersionCombo->addItem(mv.displayName, mv.mcVersion);
        }
    } else if (isIndependentVersionLoader(t)) {
        // Fabric/Quilt：遊戲版本、載入器版本各自獨立
        m_loaderVersionCombo->setEnabled(true);
        for (const QString &mc : m_gameVersionsCache.value(t))
            m_gameVersionCombo->addItem(mc, mc);
        for (const auto &mv : m_versionsCache.value(t)) {
            if (!mv.isStable) continue;
            m_loaderVersionCombo->addItem(mv.displayName, mv.loaderVersion);
        }
    } else if (isPairedVersionLoader(t)) {
        // Forge/NeoForge：先列出所有出現過的遊戲版本（去重，維持由新到舊順序），
        // 選定後由 onGameVersionChanged() 篩出該版本可用的載入器版本
        m_loaderVersionCombo->setEnabled(true);
        QStringList seen;
        for (const auto &mv : m_versionsCache.value(t)) {
            if (seen.contains(mv.mcVersion)) continue;
            seen << mv.mcVersion;
            m_gameVersionCombo->addItem(mv.mcVersion, mv.mcVersion);
        }
    }

    m_gameVersionCombo->blockSignals(false);
    onGameVersionChanged(); // 手動觸發一次，確保 Forge/NeoForge 的載入器版本清單同步
}

void LoaderPage::onGameVersionChanged()
{
    const ModloaderType t = selectedLoader();
    if (!isPairedVersionLoader(t)) return;

    m_loaderVersionCombo->clear();
    const QString mc = m_gameVersionCombo->currentData().toString();
    for (const auto &mv : m_versionsCache.value(t)) {
        if (mv.mcVersion == mc)
            m_loaderVersionCombo->addItem(mv.displayName, mv.loaderVersion);
    }
}

// ---------------- WorldTypePage ----------------

WorldTypePage::WorldTypePage(QWidget *parent) : QWizardPage(parent)
{
    setTitle(QStringLiteral("世界設定"));

    m_worldTypeCombo = new QComboBox;
    m_worldTypeCombo->addItem(QStringLiteral("一般"), int(WorldType::Normal));
    m_worldTypeCombo->addItem(QStringLiteral("平地"), int(WorldType::Flat));
    m_worldTypeCombo->addItem(QStringLiteral("大型生態域"), int(WorldType::LargeBiomes));
    m_worldTypeCombo->addItem(QStringLiteral("放大化"), int(WorldType::Amplified));
    m_worldTypeCombo->addItem(QStringLiteral("單一生態域"), int(WorldType::SingleBiomeSurface));

    m_hardcoreCheck = new QCheckBox(QStringLiteral("極限模式（Hardcore）"));
    m_bonusChestCheck = new QCheckBox(QStringLiteral("生成獎勵箱"));
    m_structuresCheck = new QCheckBox(QStringLiteral("生成結構（村莊、要塞等）"));
    m_structuresCheck->setChecked(true);
    m_seedEdit = new QLineEdit;
    m_seedEdit->setPlaceholderText(QStringLiteral("留空則使用隨機種子"));

    // 極限模式勾選時，獎勵箱在遊戲機制上不會生效，直接停用該選項避免使用者誤解
    connect(m_hardcoreCheck, &QCheckBox::toggled, m_bonusChestCheck, [this](bool checked) {
        m_bonusChestCheck->setDisabled(checked);
        if (checked) m_bonusChestCheck->setChecked(false);
    });

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("世界類型："), m_worldTypeCombo);
    form->addRow(m_hardcoreCheck);
    form->addRow(m_bonusChestCheck);
    form->addRow(m_structuresCheck);
    form->addRow(QStringLiteral("種子碼："), m_seedEdit);
    setLayout(form);
}

WorldType WorldTypePage::selectedWorldType() const
{
    return static_cast<WorldType>(m_worldTypeCombo->currentData().toInt());
}

// ---------------- AssetsPage ----------------

AssetsPage::AssetsPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle(QStringLiteral("匯入 Mods 與 Datapack（可略過）"));

    m_modList = new QListWidget;
    m_datapackList = new QListWidget;

    auto *addModBtn = new QPushButton(QStringLiteral("加入 Mod 檔案（.jar）"));
    auto *addDpBtn = new QPushButton(QStringLiteral("加入 Datapack（.zip）"));

    connect(addModBtn, &QPushButton::clicked, this, [this]() {
        const auto files = QFileDialog::getOpenFileNames(this, QStringLiteral("選擇 Mod 檔案"),
                                                           QString(), "Jar Files (*.jar)");
        m_modList->addItems(files);
    });
    connect(addDpBtn, &QPushButton::clicked, this, [this]() {
        const auto files = QFileDialog::getOpenFileNames(this, QStringLiteral("選擇 Datapack 檔案"),
                                                           QString(), "Zip Files (*.zip)");
        m_datapackList->addItems(files);
    });

    auto *layout = new QVBoxLayout;
    layout->addWidget(new QLabel(QStringLiteral("Mods：")));
    layout->addWidget(m_modList);
    layout->addWidget(addModBtn);
    layout->addWidget(new QLabel(QStringLiteral("Datapacks：")));
    layout->addWidget(m_datapackList);
    layout->addWidget(addDpBtn);
    setLayout(layout);
}

QStringList AssetsPage::modFiles() const
{
    QStringList result;
    for (int i = 0; i < m_modList->count(); ++i)
        result << m_modList->item(i)->text();
    return result;
}

QStringList AssetsPage::datapackFiles() const
{
    QStringList result;
    for (int i = 0; i < m_datapackList->count(); ++i)
        result << m_datapackList->item(i)->text();
    return result;
}

// ---------------- RunExportPage ----------------

RunExportPage::RunExportPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle(QStringLiteral("預生成與匯出"));

    m_console = new ConsoleWidget;

    m_stageLabel = new QLabel(QStringLiteral("尚未開始"));
    m_stageLabel->setObjectName("stageLabel");

    m_totalProgressBar = new QProgressBar;
    m_totalProgressBar->setRange(0, 100);
    m_totalProgressBar->setValue(0);
    m_totalProgressBar->setFormat(QStringLiteral("總進度 %p%"));
    m_totalProgressBar->setTextVisible(true);

    m_stageBusyBar = new QProgressBar;
    m_stageBusyBar->setRange(0, 0); // 目前階段忙碌指示，不確定進度時持續滾動
    m_stageBusyBar->setTextVisible(false);
    m_stageBusyBar->setFixedHeight(6);
    m_stageBusyBar->setObjectName("busyBar");

    m_exportPathEdit = new QLineEdit;
    m_exportPathEdit->setPlaceholderText(QStringLiteral("匯出的 .zip 路徑"));
    auto *browseBtn = new QPushButton(QStringLiteral("瀏覽…"));
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("選擇匯出路徑"),
                                                            QString(), "Zip (*.zip)");
        if (!path.isEmpty()) m_exportPathEdit->setText(path);
    });

    m_javaExecutableEdit = new QLineEdit;
    const QString defaultJava = defaultJavaExecutable();
    if (defaultJava != QStringLiteral("java"))
        m_javaExecutableEdit->setText(defaultJava);
    else
        m_javaExecutableEdit->setPlaceholderText(QStringLiteral("例如：C:/Program Files/Java/jre/bin/java.exe"));

    auto *javaBrowseBtn = new QPushButton(QStringLiteral("選擇 Java…"));
    connect(javaBrowseBtn, &QPushButton::clicked, this, [this]() {
        const QString prev = m_javaExecutableEdit->text().trimmed();
        const QString initialDir = !prev.isEmpty() ? QFileInfo(prev).absolutePath() : QStringLiteral("C:/Program Files/Java");
        const QString javaPath = QFileDialog::getOpenFileName(this,
                                                              QStringLiteral("選擇 Java 執行檔"),
                                                              initialDir,
                                                              QStringLiteral("java.exe (java.exe);;所有檔案 (*.*)"));
        if (!javaPath.isEmpty())
            m_javaExecutableEdit->setText(javaPath);
    });

    auto *startBtn = new QPushButton(QStringLiteral("開始執行"));
    startBtn->setObjectName("primaryButton");
    connect(startBtn, &QPushButton::clicked, this, [this, startBtn]() {
        if (m_exportPathEdit->text().isEmpty()) return;

        QString javaPath = m_javaExecutableEdit->text().trimmed();
        if (javaPath.isEmpty()) {
            const QString selected = QFileDialog::getOpenFileName(this,
                                                                 QStringLiteral("選擇 Java 執行檔"),
                                                                 QStringLiteral("C:/Program Files/Java"),
                                                                 QStringLiteral("java.exe (java.exe);;所有檔案 (*.*)"));
            if (selected.isEmpty())
                return;
            javaPath = selected;
            m_javaExecutableEdit->setText(javaPath);
        }

        startBtn->setEnabled(false);
        emit requestRun(m_exportPathEdit->text(), javaPath);
    });

    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(m_exportPathEdit);
    pathRow->addWidget(browseBtn);

    auto *javaRow = new QHBoxLayout;
    javaRow->addWidget(new QLabel(QStringLiteral("Java：")));
    javaRow->addWidget(m_javaExecutableEdit, 1);
    javaRow->addWidget(javaBrowseBtn);

    auto *progressBlock = new QVBoxLayout;
    progressBlock->setSpacing(4);
    progressBlock->addWidget(m_stageLabel);
    progressBlock->addWidget(m_totalProgressBar);
    progressBlock->addWidget(m_stageBusyBar);

    auto *layout = new QVBoxLayout;
    layout->addLayout(pathRow);
    layout->addLayout(javaRow);
    layout->addWidget(startBtn);
    layout->addSpacing(8);
    layout->addLayout(progressBlock);
    layout->addWidget(m_console);
    setLayout(layout);
}

void RunExportPage::initializePage()
{
    m_finished = false;
    m_stageLabel->setText(QStringLiteral("尚未開始"));
    m_totalProgressBar->setValue(0);
}

void RunExportPage::onStageChanged(const QString &stage)
{
    setSubTitle(QStringLiteral("目前階段：%1").arg(stage));
}

void RunExportPage::onOverallProgress(int currentStep, int totalSteps, const QString &stageName)
{
    const int pct = totalSteps > 0 ? (currentStep * 100 / totalSteps) : 0;
    m_totalProgressBar->setValue(pct);
    m_stageLabel->setText(QStringLiteral("步驟 %1 / %2：%3").arg(currentStep).arg(totalSteps).arg(stageName));
}

void RunExportPage::onPipelineFinished(bool success, const QString &message)
{
    m_totalProgressBar->setValue(100);
    m_stageBusyBar->setRange(0, 1);
    m_stageBusyBar->setValue(1);
    m_stageLabel->setText(success ? QStringLiteral("完成") : QStringLiteral("失敗"));
    m_console->appendLine(QStringLiteral(">>> [結果] %1：%2")
                           .arg(success ? QStringLiteral("成功") : QStringLiteral("失敗"), message));
    m_finished = true;
    emit completeChanged();
}

// ---------------- WorldGenWizard ----------------

WorldGenWizard::WorldGenWizard(QWidget *parent) : QWizard(parent)
{
    setWindowTitle(QStringLiteral("Minecraft 世界預生成精靈"));
    resize(720, 560);

    // Windows 上 QWizard 預設會自動選用 AeroStyle（毛玻璃質感），
    // 但在某些環境（虛擬機、遠端桌面、部分顯示縮放設定）DWM 合成沒有正確運作時，
    // 會整個塌成一條黑色橫幅、內容區塊完全空白（本專案先前遇到的畫面全白問題即為此）。
    // 改用 ModernStyle 完全不依賴 DWM glass 特效，穩定在各種環境下都能正常渲染。
    setWizardStyle(QWizard::ModernStyle);

    m_loaderPage = new LoaderPage;
    m_worldTypePage = new WorldTypePage;
    m_assetsPage = new AssetsPage;
    m_runExportPage = new RunExportPage;

    addPage(m_loaderPage);
    addPage(m_worldTypePage);
    addPage(m_assetsPage);
    addPage(m_runExportPage);

    connect(m_runExportPage, &RunExportPage::requestRun, this, &WorldGenWizard::startPipeline);
}

void WorldGenWizard::startPipeline(const QString &exportPath, const QString &javaExecutable)
{
    WorldGenConfig config;
    config.loader = m_loaderPage->selectedLoader();
    config.gameVersion = m_loaderPage->selectedGameVersion();
    config.loaderVersion = m_loaderPage->selectedLoaderVersion();

    config.worldType = m_worldTypePage->selectedWorldType();
    config.hardcore = m_worldTypePage->hardcore();
    config.bonusChest = m_worldTypePage->bonusChest();
    config.generateStructures = m_worldTypePage->generateStructures();
    config.seed = m_worldTypePage->seed();

    config.modFiles = m_assetsPage->modFiles();
    config.datapackFiles = m_assetsPage->datapackFiles();

    config.javaExecutable = javaExecutable.trimmed();
    if (config.javaExecutable.startsWith('"') && config.javaExecutable.endsWith('"') && config.javaExecutable.size() >= 2)
        config.javaExecutable = config.javaExecutable.mid(1, config.javaExecutable.size() - 2);
    if (config.javaExecutable.isEmpty())
        config.javaExecutable = defaultJavaExecutable();

    config.exportZipPath = exportPath;

    auto *controller = new WorldGenController(this);
    connect(controller, &WorldGenController::consoleLine, m_runExportPage, &RunExportPage::onConsoleLine);
    connect(controller, &WorldGenController::stageChanged, m_runExportPage, &RunExportPage::onStageChanged);
    connect(controller, &WorldGenController::overallProgress, m_runExportPage, &RunExportPage::onOverallProgress);
    connect(controller, &WorldGenController::pipelineFinished, m_runExportPage, &RunExportPage::onPipelineFinished);

    controller->runFullPipeline(config);
}
