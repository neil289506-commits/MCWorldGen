#include "core/ServerCoreInstaller.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QNetworkRequest>
#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {

QString normalizeJavaExecutable(const QString &value)
{
    QString result = value.trimmed();
    if (result.startsWith('"') && result.endsWith('"') && result.size() >= 2)
        result = result.mid(1, result.size() - 2).trimmed();
    return result;
}

QString quoteCommandArg(const QString &value)
{
    QString trimmed = value.trimmed();
    if (trimmed.isEmpty())
        return trimmed;
    if (trimmed.startsWith('"') && trimmed.endsWith('"') && trimmed.size() >= 2)
        return trimmed;
    return trimmed.contains(' ') ? QString("\"") + trimmed + QString("\"") : trimmed;
}

} // namespace

ServerCoreInstaller::ServerCoreInstaller(QObject *parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this))
{
}

ServerCoreInstaller::~ServerCoreInstaller() { cancel(); }

void ServerCoreInstaller::cancel()
{
    m_cancelled = true;
    if (m_reply)   m_reply->abort();
    if (m_process) m_process->kill();
    if (m_outFile && m_outFile->isOpen()) m_outFile->close();
}

void ServerCoreInstaller::hideProcessWindow(QProcess *p)
{
#ifdef Q_OS_WIN
    p->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= 0x08000000;
        args->startupInfo->dwFlags |= 0x01;
        args->startupInfo->wShowWindow = 0;
    });
#else
    Q_UNUSED(p)
#endif
}

void ServerCoreInstaller::ensureLauncherProfile(const QString &dir)
{
    // Forge/NeoForge 的 installer 需要這個檔案存在才不會抱怨（即使我們只裝 Server 端）
    QString path = dir + "/launcher_profiles.json";
    if (!QFile::exists(path)) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write("{\"profiles\":{\"(Default)\":{\"name\":\"(Default)\",\"type\":\"latest-release\"}},"
                    "\"selectedProfile\":\"(Default)\","
                    "\"clientToken\":\"00000000-0000-0000-0000-000000000000\","
                    "\"authenticationDatabase\":{},"
                    "\"launcherVersion\":{\"format\":21}}");
            f.close();
        }
    }
}

bool ServerCoreInstaller::mcAtLeast(const QString &ver, int minor, int patch)
{
    QStringList p = ver.split('.');
    if (p.size() < 2) return false;
    int mn = p[1].toInt();
    int pt = p.size() >= 3 ? p[2].toInt() : 0;
    if (mn > minor) return true;
    if (mn == minor && pt >= patch) return true;
    return false;
}

void ServerCoreInstaller::startDownload(const QUrl &url, const QString &destPath,
                                         std::function<void()> onSuccess)
{
    m_downloadCallback = onSuccess;

    if (m_outFile) delete m_outFile;
    m_outFile = new QFile(destPath, this);
    if (!m_outFile->open(QIODevice::WriteOnly)) {
        emit installFinished(false, "無法寫入檔案：" + destPath, {});
        return;
    }

    emit progressMessage("下載：" + url.toString());

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                      QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam->get(req);

    connect(m_reply, &QNetworkReply::downloadProgress, this, &ServerCoreInstaller::onDownloadProgress);
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_outFile) m_outFile->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::finished, this, &ServerCoreInstaller::onDownloadFinished);
}

void ServerCoreInstaller::onDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0) emit progressPercent(int(received * 100 / total));
    else emit progressPercent(-1);
}

void ServerCoreInstaller::onDownloadFinished()
{
    if (m_outFile) m_outFile->close();
    if (m_cancelled) return;

    if (m_reply->error() != QNetworkReply::NoError) {
        emit installFinished(false, "下載失敗：" + m_reply->errorString(), {});
        m_reply->deleteLater(); m_reply = nullptr;
        return;
    }
    m_reply->deleteLater(); m_reply = nullptr;
    emit progressPercent(100);
    if (m_downloadCallback) m_downloadCallback();
}

void ServerCoreInstaller::runInstallerJar(const QString &jarPath, const QStringList &extraArgs,
                                           std::function<void(bool)> onFinished)
{
    m_processCallback = onFinished;
    QString java = normalizeJavaExecutable(m_config.javaExecutable);
    if (java.isEmpty()) java = QStringLiteral("java");
    QStringList args;
    args << "-Dfile.encoding=UTF-8" << "-jar" << jarPath << extraArgs;

    emit progressMessage("執行：" + quoteCommandArg(java) + " " + args.join(' '));
    emit progressPercent(-1);

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(m_installDir);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    hideProcessWindow(m_process);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &ServerCoreInstaller::onProcessOutput);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ServerCoreInstaller::onProcessFinished);

    m_process->start(java, args);
}

void ServerCoreInstaller::onProcessOutput()
{
    QString out = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
    if (!out.isEmpty()) emit progressMessage(out);
}

void ServerCoreInstaller::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_process->deleteLater(); m_process = nullptr;
    bool ok = (exitCode == 0 && status == QProcess::NormalExit);
    if (m_processCallback) m_processCallback(ok);
}

// 依載入器安裝完成後，掃描 installDir 找出實際可執行的伺服器進入點
LaunchSpec ServerCoreInstaller::resolveLaunchSpec(bool forgeNewStyle) const
{
    LaunchSpec spec;
    spec.workingDir = m_installDir;
    QString java = normalizeJavaExecutable(m_config.javaExecutable);
    if (java.isEmpty()) java = QStringLiteral("java");
    const QString xmx = QString("-Xmx%1M").arg(m_config.maxMemoryMB);

    switch (m_config.loader) {
        case ModloaderType::Vanilla: {
            spec.program = java;
            spec.arguments = {xmx, "-jar", "server.jar", "nogui"};
            spec.valid = QFile::exists(m_installDir + "/server.jar");
            return spec;
        }
        case ModloaderType::Fabric: {
            spec.program = java;
            spec.arguments = {xmx, "-jar", "fabric-server-launch.jar", "nogui"};
            spec.valid = QFile::exists(m_installDir + "/fabric-server-launch.jar");
            return spec;
        }
        case ModloaderType::Quilt: {
            spec.program = java;
            spec.arguments = {xmx, "-jar", "quilt-server-launch.jar", "nogui"};
            spec.valid = QFile::exists(m_installDir + "/quilt-server-launch.jar");
            return spec;
        }
        case ModloaderType::Paper: {
            spec.program = java;
            // Paper jar 檔名含 build 編號，動態搜尋 installDir 下的 paper-*.jar
            QDir dir(m_installDir);
            const auto matches = dir.entryList({"paper-*.jar"}, QDir::Files);
            if (matches.isEmpty()) return spec;
            spec.arguments = {xmx, "-jar", matches.first(), "nogui"};
            spec.valid = true;
            return spec;
        }
        case ModloaderType::Forge:
        case ModloaderType::NeoForge: {
            if (forgeNewStyle) {
                // 1.18.1+：installer 已產生 run.bat / run.sh，內部才是真正的 classpath 組合
#ifdef Q_OS_WIN
                spec.program = "cmd";
                spec.arguments = {"/c", "run.bat", "nogui"};
                spec.valid = QFile::exists(m_installDir + "/run.bat");
#else
                spec.program = "bash";
                spec.arguments = {"run.sh", "nogui"};
                spec.valid = QFile::exists(m_installDir + "/run.sh");
#endif
            } else {
                // 舊版 Forge：直接執行 forge-*-universal.jar
                QDir dir(m_installDir);
                const auto matches = dir.entryList({"forge-*-universal.jar"}, QDir::Files);
                if (matches.isEmpty()) return spec;
                spec.program = java;
                spec.arguments = {xmx, "-jar", matches.first(), "nogui"};
                spec.valid = true;
            }
            return spec;
        }
        default:
            return spec;
    }
}

void ServerCoreInstaller::install(const WorldGenConfig &config, const QString &installDir)
{
    m_config = config;
    m_installDir = installDir;
    m_cancelled = false;
    QDir().mkpath(installDir);

    switch (config.loader) {
        case ModloaderType::Vanilla:  installVanilla();  break;
        case ModloaderType::Fabric:   installFabric();   break;
        case ModloaderType::Quilt:    installQuilt();    break;
        case ModloaderType::Forge:    installForge();    break;
        case ModloaderType::NeoForge: installNeoForge(); break;
        case ModloaderType::Paper:    installPaper();    break;
        default:
            emit installFinished(false, "不支援的載入器類型", {});
    }
}

// ─────────────────────────── Vanilla ──────────────────────────────────────

void ServerCoreInstaller::installVanilla()
{
    emit progressMessage("查詢 Vanilla " + m_config.gameVersion + " 的伺服器下載位址...");

    auto *manifestReply = m_nam->get(QNetworkRequest(
        QUrl("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json")));
    connect(manifestReply, &QNetworkReply::finished, this, [this, manifestReply]() {
        manifestReply->deleteLater();
        if (manifestReply->error() != QNetworkReply::NoError) {
            emit installFinished(false, "取得版本清單失敗：" + manifestReply->errorString(), {});
            return;
        }
        const auto doc = QJsonDocument::fromJson(manifestReply->readAll());
        QString versionJsonUrl;
        for (const auto &v : doc["versions"].toArray()) {
            const auto obj = v.toObject();
            if (obj["id"].toString() == m_config.gameVersion) {
                versionJsonUrl = obj["url"].toString();
                break;
            }
        }
        if (versionJsonUrl.isEmpty()) {
            emit installFinished(false, "找不到指定的遊戲版本：" + m_config.gameVersion, {});
            return;
        }

        auto *detailReply = m_nam->get(QNetworkRequest(QUrl(versionJsonUrl)));
        connect(detailReply, &QNetworkReply::finished, this, [this, detailReply]() {
            detailReply->deleteLater();
            if (detailReply->error() != QNetworkReply::NoError) {
                emit installFinished(false, "取得版本詳細資料失敗：" + detailReply->errorString(), {});
                return;
            }
            const auto detail = QJsonDocument::fromJson(detailReply->readAll());
            const QString serverUrl = detail["downloads"]["server"]["url"].toString();
            if (serverUrl.isEmpty()) {
                emit installFinished(false, "此版本沒有提供伺服器核心下載", {});
                return;
            }
            startDownload(QUrl(serverUrl), m_installDir + "/server.jar", [this]() {
                emit installFinished(true, "Vanilla 伺服器核心已就緒", resolveLaunchSpec(false));
            });
        });
    });
}

// ─────────────────────────── Fabric ───────────────────────────────────────

void ServerCoreInstaller::installFabric()
{
    QString installerUrl = "https://maven.fabricmc.net/net/fabricmc/fabric-installer/1.0.1/"
                            "fabric-installer-1.0.1.jar";
    QString jarPath = m_installDir + "/fabric-installer.jar";

    emit progressMessage("下載 Fabric installer...");
    startDownload(QUrl(installerUrl), jarPath, [this, jarPath]() {
        runInstallerJar(jarPath,
            {"server", "-dir", m_installDir,
             "-mcversion", m_config.gameVersion,
             "-loader", m_config.loaderVersion,
             "-downloadMinecraft"},
            [this, jarPath](bool ok) {
                QFile::remove(jarPath);
                if (!ok) { emit installFinished(false, "Fabric installer 執行失敗", {}); return; }
                emit installFinished(true, "Fabric 伺服器核心已就緒", resolveLaunchSpec(false));
            });
    });
}

// ─────────────────────────── Quilt ────────────────────────────────────────

void ServerCoreInstaller::installQuilt()
{
    QString installerUrl = "https://quiltmc.org/api/v1/download-latest-installer/java-universal";
    QString jarPath = m_installDir + "/quilt-installer.jar";

    emit progressMessage("下載 Quilt installer...");
    startDownload(QUrl(installerUrl), jarPath, [this, jarPath]() {
        runInstallerJar(jarPath,
            {"install", "server",
             m_config.gameVersion, m_config.loaderVersion,
             "--install-dir=" + m_installDir,
             "--download-server"},
            [this, jarPath](bool ok) {
                QFile::remove(jarPath);
                if (!ok) { emit installFinished(false, "Quilt installer 執行失敗", {}); return; }
                emit installFinished(true, "Quilt 伺服器核心已就緒", resolveLaunchSpec(false));
            });
    });
}

// ─────────────────────────── Forge ────────────────────────────────────────

void ServerCoreInstaller::installForge()
{
    const QString ver = m_config.gameVersion + "-" + m_config.loaderVersion;
    const QString url = QString("https://maven.minecraftforge.net/net/minecraftforge/"
                                 "forge/%1/forge-%1-installer.jar").arg(ver);
    const QString jarPath = m_installDir + "/forge-installer.jar";
    const bool newStyle = mcAtLeast(m_config.gameVersion, 18, 1);

    emit progressMessage("下載 Forge installer（" + ver + "）...");
    startDownload(QUrl(url), jarPath, [this, jarPath, newStyle]() {
        ensureLauncherProfile(m_installDir);
        runInstallerJar(jarPath, {"--installServer", m_installDir},
            [this, jarPath, newStyle](bool ok) {
                QFile::remove(jarPath);
                if (!ok) { emit installFinished(false, "Forge installer 執行失敗", {}); return; }
                emit installFinished(true, "Forge 伺服器核心已就緒", resolveLaunchSpec(newStyle));
            });
    });
}

// ─────────────────────────── NeoForge ─────────────────────────────────────

void ServerCoreInstaller::installNeoForge()
{
    const QString ver = m_config.loaderVersion;
    const QString url = QString("https://maven.neoforged.net/releases/net/neoforged/"
                                 "neoforge/%1/neoforge-%1-installer.jar").arg(ver);
    const QString jarPath = m_installDir + "/neoforge-installer.jar";

    emit progressMessage("下載 NeoForge installer（" + ver + "）...");
    startDownload(QUrl(url), jarPath, [this, jarPath]() {
        ensureLauncherProfile(m_installDir);
        runInstallerJar(jarPath, {"--installServer", m_installDir},
            [this, jarPath](bool ok) {
                QFile::remove(jarPath);
                if (!ok) { emit installFinished(false, "NeoForge installer 執行失敗", {}); return; }
                // NeoForge 目前支援的版本皆為新式 run.bat/run.sh 安裝
                emit installFinished(true, "NeoForge 伺服器核心已就緒", resolveLaunchSpec(true));
            });
    });
}

// ─────────────────────────── Paper ────────────────────────────────────────
// 改寫自 PaperPurpurDownloader.cpp：Paper 官方直接提供編譯好的 jar，不需要安裝步驟

void ServerCoreInstaller::installPaper()
{
    emit progressMessage("查詢 Paper " + m_config.gameVersion + " 的最新 build...");

    const QUrl buildsUrl(QString("https://api.papermc.io/v2/projects/paper/versions/%1/builds")
                          .arg(m_config.gameVersion));
    auto *reply = m_nam->get(QNetworkRequest(buildsUrl));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit installFinished(false, "查詢 Paper build 失敗：" + reply->errorString(), {});
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        const auto builds = doc.object().value("builds").toArray();
        if (builds.isEmpty()) {
            emit installFinished(false, "找不到 Paper " + m_config.gameVersion + " 的任何 build", {});
            return;
        }
        const auto latest = builds.last().toObject();
        const int build = latest.value("build").toInt();
        const QString jarName = latest.value("downloads").toObject()
                                     .value("application").toObject()
                                     .value("name").toString();

        emit progressMessage(QString("找到 build %1（%2），開始下載...").arg(build).arg(jarName));

        const QUrl jarUrl(QString("https://api.papermc.io/v2/projects/paper/versions/%1/builds/%2/downloads/%3")
                           .arg(m_config.gameVersion).arg(build).arg(jarName));
        startDownload(jarUrl, m_installDir + "/" + jarName, [this]() {
            emit installFinished(true, "Paper 伺服器核心已就緒", resolveLaunchSpec(false));
        });
    });
}
