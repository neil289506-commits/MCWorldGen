#include "core/VersionFetcher.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QXmlStreamReader>
#include <QCoreApplication>
#include <QNetworkReply>
#include <QDebug>
#ifdef Q_OS_WIN
#  include <windows.h>
#endif

VersionFetcher::VersionFetcher(QObject *parent)
    : QObject(parent), m_net(new QNetworkAccessManager(this))
{
}

void VersionFetcher::fetchAll()
{
    if (m_fetching) return;

    QString scriptPath = QCoreApplication::applicationDirPath() + "/requestVer.py";
    if (!QFile::exists(scriptPath))
        scriptPath = QDir::currentPath() + "/requestVer.py";
    if (!QFile::exists(scriptPath)) {
        generatePythonScript(scriptPath);
    }
    if (!QFile::exists(scriptPath)) {
        emit fetchError("requestVer.py not found and could not be created.");
        return;
    }

    m_versionDir = QCoreApplication::applicationDirPath() + "/version";
    m_fetching = true;

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(QCoreApplication::applicationDirPath());
    m_process->setProcessChannelMode(QProcess::MergedChannels);

#ifdef Q_OS_WIN
    m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= 0x08000000;
        args->startupInfo->dwFlags |= 0x01;
        args->startupInfo->wShowWindow = 0;
    });
#endif

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &VersionFetcher::onProcessOutput);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &VersionFetcher::onProcessFinished);

    emit fetchProgress("Starting version fetch...");

#ifdef Q_OS_WIN
    m_process->start("python", {scriptPath});
#else
    m_process->start("python3", {scriptPath});
#endif
}

void VersionFetcher::parseLocal(const QString &versionDir)
{
    m_versionDir = versionDir;
    parseVanilla(versionDir);
    parseFabric(versionDir);
    parseQuilt(versionDir);
    parseForge(versionDir);
    parseNeoForge(versionDir);
}

void VersionFetcher::onProcessOutput()
{
    QString out = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
    if (!out.isEmpty()) emit fetchProgress(out);
}

void VersionFetcher::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_fetching = false;
    m_process->deleteLater();
    m_process = nullptr;

    if (exitCode != 0 || status != QProcess::NormalExit) {
        emit fetchError(QString("Python script exited with code %1").arg(exitCode));
        return;
    }

    emit fetchFinished();
    parseLocal(m_versionDir);
}

// ─────────────────────────────────── Parsers ──────────────────────────────

// Split version string into numeric components for comparison
// e.g. "1.20.4" → [1, 20, 4],  "0.16.0" → [0, 16, 0]
// Non-numeric suffix in a component is ignored: "2beta" → 2
static QVector<int> semVec(const QString &ver)
{
    QVector<int> v;
    for (const QString &part : ver.split('.')) {
        QString num;
        for (QChar c : part) {
            if (c.isDigit()) num += c;
            else break;
        }
        v.append(num.isEmpty() ? 0 : num.toInt());
    }
    return v;
}

// Returns true when a is semantically newer than b  (sorts descending)
static bool semGt(const QString &a, const QString &b)
{
    auto va = semVec(a);
    auto vb = semVec(b);
    int n = qMax(va.size(), vb.size());
    for (int i = 0; i < n; ++i) {
        int ai = i < va.size() ? va[i] : 0;
        int bi = i < vb.size() ? vb[i] : 0;
        if (ai != bi) return ai > bi;
    }
    return false;
}

// Compare ModloaderVersion: by mcVersion desc, then loaderVersion desc
static bool mvGt(const ModloaderVersion &a, const ModloaderVersion &b)
{
    if (a.mcVersion != b.mcVersion) return semGt(a.mcVersion, b.mcVersion);
    return semGt(a.loaderVersion, b.loaderVersion);
}

void VersionFetcher::parseVanilla(const QString &dir)
{
    QFile f(dir + "/MojangData.json");
    if (!f.open(QIODevice::ReadOnly)) return;

    QJsonArray arr = QJsonDocument::fromJson(f.readAll()).object()["versions"].toArray();
    QList<ModloaderVersion> list;
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        QString type = o["type"].toString();
        if (type != "release" && type != "snapshot") continue;
        ModloaderVersion mv;
        mv.mcVersion     = o["id"].toString();
        mv.loaderVersion = "";
        mv.displayName   = mv.mcVersion + (type == "snapshot" ? " (snapshot)" : "");
        mv.isStable      = (type == "release");
        list.append(mv);
    }
    // Mojang API already returns newest-first, but sort explicitly to be safe
    std::sort(list.begin(), list.end(), [](const ModloaderVersion &a, const ModloaderVersion &b) {
        return semGt(a.mcVersion, b.mcVersion);
    });
    emit versionsReady(ModloaderType::Vanilla, list);
}

void VersionFetcher::parseFabric(const QString &dir)
{
    // --- Game versions ---
    QFile gf(dir + "/FabricGame.json");
    if (gf.open(QIODevice::ReadOnly)) {
        QJsonArray arr = QJsonDocument::fromJson(gf.readAll()).array();
        QStringList mcVersions;
        for (const QJsonValue &v : arr) {
            QString ver = v.toObject()["version"].toString();
            if (!ver.isEmpty()) mcVersions << ver;
        }
        std::sort(mcVersions.begin(), mcVersions.end(),
                  [](const QString &a, const QString &b) { return semGt(a, b); });
        emit gameVersionsReady(ModloaderType::Fabric, mcVersions);
    }

    // --- Loader versions ---
    QFile lf(dir + "/FabricLoader.json");
    if (!lf.open(QIODevice::ReadOnly)) return;

    QJsonArray arr = QJsonDocument::fromJson(lf.readAll()).array();
    QList<ModloaderVersion> list;
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        ModloaderVersion mv;
        mv.loaderVersion = o["version"].toString();
        mv.mcVersion     = "";
        mv.displayName   = mv.loaderVersion;
        mv.isStable      = o["stable"].toBool();
        list.append(mv);
    }
    std::sort(list.begin(), list.end(), [](const ModloaderVersion &a, const ModloaderVersion &b) {
        return semGt(a.loaderVersion, b.loaderVersion);
    });
    emit versionsReady(ModloaderType::Fabric, list);
}

void VersionFetcher::parseQuilt(const QString &dir)
{
    // --- Game versions ---
    QFile gf(dir + "/QuiltGame.json");
    if (gf.open(QIODevice::ReadOnly)) {
        QJsonArray arr = QJsonDocument::fromJson(gf.readAll()).array();
        QStringList mcVersions;
        for (const QJsonValue &v : arr) {
            QString ver = v.toObject()["version"].toString();
            if (!ver.isEmpty()) mcVersions << ver;
        }
        std::sort(mcVersions.begin(), mcVersions.end(),
                  [](const QString &a, const QString &b) { return semGt(a, b); });
        emit gameVersionsReady(ModloaderType::Quilt, mcVersions);
    }

    // --- Loader versions ---
    QFile lf(dir + "/QuiltLoader.json");
    if (!lf.open(QIODevice::ReadOnly)) return;

    QJsonArray arr = QJsonDocument::fromJson(lf.readAll()).array();
    QList<ModloaderVersion> list;
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        ModloaderVersion mv;
        mv.loaderVersion = o["version"].toString();
        mv.mcVersion     = "";
        mv.displayName   = mv.loaderVersion;
        mv.isStable      = !mv.loaderVersion.contains("beta") &&
                            !mv.loaderVersion.contains("alpha");
        list.append(mv);
    }
    std::sort(list.begin(), list.end(), [](const ModloaderVersion &a, const ModloaderVersion &b) {
        return semGt(a.loaderVersion, b.loaderVersion);
    });
    emit versionsReady(ModloaderType::Quilt, list);
}

static QStringList parseMavenVersions(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QXmlStreamReader xml(&f);
    QStringList versions;
    bool inVersioning = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QLatin1String("versioning")) inVersioning = true;
            if (inVersioning && xml.name() == QLatin1String("version"))
                versions << xml.readElementText();
        }
        if (xml.isEndElement() && xml.name() == QLatin1String("versioning"))
            inVersioning = false;
    }
    return versions;
}

void VersionFetcher::parseForge(const QString &dir)
{
    // Parse both "latest" and "recommended" tags per MC version
    QFile pf(dir + "/ForgePromotion.json");
    QMap<QString, QString> latestMap;       // mcVer → loader version
    QMap<QString, QString> recommendedMap;  // mcVer → loader version

    if (pf.open(QIODevice::ReadOnly)) {
        QJsonObject promos = QJsonDocument::fromJson(pf.readAll())
                                 .object()["promos"].toObject();
        for (auto it = promos.begin(); it != promos.end(); ++it) {
            if (it.key().endsWith("-latest"))
                latestMap[it.key().chopped(7)]       = it.value().toString();
            else if (it.key().endsWith("-recommended"))
                recommendedMap[it.key().chopped(13)] = it.value().toString();
        }
    }

    QStringList raw = parseMavenVersions(dir + "/ForgeMaven.xml");
    QList<ModloaderVersion> list;
    for (const QString &ver : raw) {
        int dash = ver.indexOf('-');
        if (dash < 0) continue;

        ModloaderVersion mv;
        mv.mcVersion     = ver.left(dash);
        mv.loaderVersion = ver.mid(dash + 1);

        bool isLatest = (latestMap.value(mv.mcVersion) == mv.loaderVersion);
        bool isRec    = (recommendedMap.value(mv.mcVersion) == mv.loaderVersion);

        mv.isStable = isLatest || isRec;

        if (isLatest)
            mv.displayName = mv.loaderVersion + "  [Latest]";
        else if (isRec)
            mv.displayName = mv.loaderVersion + "  ★";
        else
            mv.displayName = mv.loaderVersion;

        list.append(mv);
    }

    // Sort: MC version desc, then within same MC version:
    //   Latest first → Recommended second (★) → others by loader version desc
    std::sort(list.begin(), list.end(),
        [&](const ModloaderVersion &a, const ModloaderVersion &b)
    {
        if (a.mcVersion != b.mcVersion)
            return semGt(a.mcVersion, b.mcVersion);

        bool aLatest = (latestMap.value(a.mcVersion) == a.loaderVersion);
        bool bLatest = (latestMap.value(b.mcVersion) == b.loaderVersion);
        bool aRec    = (recommendedMap.value(a.mcVersion) == a.loaderVersion);
        bool bRec    = (recommendedMap.value(b.mcVersion) == b.loaderVersion);

        int aPrio = aLatest ? 2 : (aRec ? 1 : 0);
        int bPrio = bLatest ? 2 : (bRec ? 1 : 0);

        if (aPrio != bPrio) return aPrio > bPrio;
        return semGt(a.loaderVersion, b.loaderVersion);
    });

    emit versionsReady(ModloaderType::Forge, list);
}

void VersionFetcher::parseNeoForge(const QString &dir)
{
    QStringList raw = parseMavenVersions(dir + "/NeoForgeMaven.xml");
    QList<ModloaderVersion> list;
    for (const QString &ver : raw) {
        ModloaderVersion mv;
        QStringList parts = ver.split('.');
        mv.mcVersion     = parts.size() >= 2 ? "1." + parts[0] + "." + parts[1] : ver;
        mv.loaderVersion = ver;
        mv.displayName   = ver;
        mv.isStable      = !ver.contains("beta") && !ver.contains("alpha");
        list.append(mv);
    }
    // Sort: MC version desc, then full loader version desc
    std::sort(list.begin(), list.end(), mvGt);
    emit versionsReady(ModloaderType::NeoForge, list);
}

void VersionFetcher::fetchPaperVersions()
{
    auto *reply = m_net->get(QNetworkRequest(QUrl("https://api.papermc.io/v2/projects/paper")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit fetchError(reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        QList<ModloaderVersion> list;
        for (const auto &v : doc["versions"].toArray()) {
            ModloaderVersion mv;
            mv.mcVersion = v.toString();
            mv.displayName = mv.mcVersion;
            list.append(mv);
        }
        // Paper API 已經是舊到新排序，反轉成新到舊以跟其他載入器一致
        std::reverse(list.begin(), list.end());
        emit versionsReady(ModloaderType::Paper, list);
    });
}

// ─────────────────────── Auto-generate Python script ──────────────────────

void VersionFetcher::generatePythonScript(const QString &scriptPath)
{
    const QString script = R"SCRIPT(#!/usr/bin/env python3
import os, sys, requests

VERSION_DIR = "version"
os.makedirs(VERSION_DIR, exist_ok=True)

def download(url, filename, name):
    try:
        print("[{}] Downloading...".format(name), flush=True)
        r = requests.get(url, stream=True, timeout=30, headers={"User-Agent": "Mozilla/5.0"})
        r.raise_for_status()
        with open(os.path.join(VERSION_DIR, filename), "wb") as f:
            for chunk in r.iter_content(chunk_size=8192):
                if chunk:
                    f.write(chunk)
        print("[{}] Done".format(name), flush=True)
        return True
    except Exception as e:
        print("[{}] Error: {}".format(name, e), flush=True)
        return False

try:
    download("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json", "MojangData.json", "Vanilla")
    download("https://meta.fabricmc.net/v2/versions/game", "FabricGame.json", "Fabric Game")
    download("https://meta.fabricmc.net/v2/versions/loader", "FabricLoader.json", "Fabric Loader")
    download("https://meta.quiltmc.org/v3/versions/game", "QuiltGame.json", "Quilt Game")
    download("https://meta.quiltmc.org/v3/versions/loader", "QuiltLoader.json", "Quilt Loader")
    download("https://maven.minecraftforge.net/net/minecraftforge/forge/maven-metadata.xml", "ForgeMaven.xml", "Forge Maven")
    download("https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json", "ForgePromotion.json", "Forge Promos")
    download("https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml", "NeoForgeMaven.xml", "NeoForge")
    print("\n[Complete] All version data downloaded", flush=True)
    sys.exit(0)
except Exception as e:
    print("[Fatal] {}".format(e), flush=True)
    sys.exit(1)
)SCRIPT";

    QFile f(scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit fetchError("Cannot create requestVer.py: " + scriptPath);
        return;
    }
    f.write(script.toUtf8());
    f.close();
    emit fetchProgress("Auto-generated requestVer.py");
}
