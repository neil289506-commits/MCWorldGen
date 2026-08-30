#pragma once
#include <QString>
#include <QStringList>
#include "ModloaderInfo.h"

// 世界類型（對應 1.16+ 的 level-type 命名空間值）
enum class WorldType {
    Normal,              // minecraft:normal
    Flat,                // minecraft:flat（平地）
    LargeBiomes,         // minecraft:large_biomes
    Amplified,           // minecraft:amplified
    SingleBiomeSurface   // minecraft:single_biome_surface
};

// 將 WorldType 轉換為 server.properties 使用的字串
inline QString worldTypeToLevelType(WorldType type)
{
    switch (type) {
        case WorldType::Normal:             return "minecraft:normal";
        case WorldType::Flat:               return "minecraft:flat";
        case WorldType::LargeBiomes:        return "minecraft:large_biomes";
        case WorldType::Amplified:          return "minecraft:amplified";
        case WorldType::SingleBiomeSurface: return "minecraft:single_biome_surface";
    }
    return "minecraft:normal";
}

// 使用者在精靈頁面中設定的完整參數，貫穿整個預生成流程
struct WorldGenConfig {
    // 載入器 / 版本
    ModloaderType loader = ModloaderType::Vanilla;
    QString loaderVersion;   // 例如 Fabric loader 0.16.9（Vanilla/Paper 為空）
    QString gameVersion;     // 例如 1.21.1

    // 世界設定
    WorldType worldType = WorldType::Normal;
    bool hardcore = false;
    bool bonusChest = false;
    bool generateStructures = true;
    QString seed;            // 空字串代表隨機種子
    QString levelName = "world";

    // 匯入項目（絕對路徑清單）
    QStringList modFiles;
    QStringList datapackFiles;

    // 執行參數
    QString javaExecutable = "java";
    int maxMemoryMB = 4096;
    int pregenTimeoutSec = 1800; // 預生成逾時保護

    // 匯出
    QString exportZipPath; // 使用者最後選擇的匯出路徑
};
