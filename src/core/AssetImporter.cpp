#include "core/AssetImporter.h"
#include <QDir>
#include <QFileInfo>

namespace AssetImporter {

static bool copyIntoDir(const QString &srcFile, const QString &destDir)
{
    QDir().mkpath(destDir);
    const QFileInfo info(srcFile);
    const QString destPath = destDir + "/" + info.fileName();

    // 若目標已存在（例如重新執行預生成），先移除避免 QFile::copy 失敗
    if (QFile::exists(destPath))
        QFile::remove(destPath);

    return QFile::copy(srcFile, destPath);
}

QStringList importAll(const WorldGenConfig &config, const QString &pregenDir)
{
    QStringList failed;

    // mods 只有非 Vanilla（Fabric/Forge/NeoForge/Quilt）才有意義，
    // 但這裡不特別限制，交由呼叫端在 UI 決定是否顯示該選項
    const QString modsDir = pregenDir + "/mods";
    for (const QString &mod : config.modFiles) {
        if (!copyIntoDir(mod, modsDir))
            failed << mod;
    }

    // datapack 需放進世界資料夾底下： <level-name>/datapacks/
    const QString datapackDir = pregenDir + "/" + config.levelName + "/datapacks";
    for (const QString &dp : config.datapackFiles) {
        if (!copyIntoDir(dp, datapackDir))
            failed << dp;
    }

    return failed;
}

} // namespace AssetImporter
