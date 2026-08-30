#pragma once
#include <QString>
#include "WorldGenTypes.h"

namespace ServerPropertiesWriter {
    // 依 WorldGenConfig 寫出 server.properties 到 dirPath/server.properties
    // 回傳是否寫入成功
    bool write(const WorldGenConfig &config, const QString &dirPath);

    // 若 seed 為空，產生一組隨機種子字串（純數字，避免部分載入器對特殊字元處理不一致）
    QString resolveSeed(const QString &seedInput);
}
