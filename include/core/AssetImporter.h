#pragma once
#include <QString>
#include "WorldGenTypes.h"

namespace AssetImporter {
    // 將 config.modFiles 複製到 <pregenDir>/mods/
    // 將 config.datapackFiles 複製到 <pregenDir>/<levelName>/datapacks/
    // 回傳失敗的檔案清單（皆成功則為空）
    QStringList importAll(const WorldGenConfig &config, const QString &pregenDir);
}
