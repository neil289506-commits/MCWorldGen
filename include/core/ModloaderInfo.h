#pragma once
#include <QString>

// 改寫自參考專案（MCServer-Manager-17）的 src/ModloaderInfo.h，
// 僅保留 worldgen 精靈用得到的 6 種載入器，並拿掉 Client/Both 安裝目標與
// 圖示/顏色等 UI 專用欄位（worldgen 只做 Server 端預生成，不需要那些）。
enum class ModloaderType {
    Vanilla, Fabric, Quilt, Forge, NeoForge, Paper,
    Unknown
};

// 單一版本項目：
// - Vanilla / Paper：只有 mcVersion 有值，loaderVersion 為空
// - Fabric / Quilt：versionsReady 給的項目只有 loaderVersion 有值（獨立於遊戲版本），
//                   對應的遊戲版本清單另外由 gameVersionsReady 提供
// - Forge / NeoForge：mcVersion + loaderVersion 成對出現（同一個 mc 版本可能對應多個 loader 版本）
struct ModloaderVersion {
    QString mcVersion;
    QString loaderVersion;
    QString displayName;
    bool    isStable = true;
};

inline QString modloaderName(ModloaderType t) {
    switch (t) {
        case ModloaderType::Vanilla:  return "Vanilla";
        case ModloaderType::Fabric:   return "Fabric";
        case ModloaderType::Quilt:    return "Quilt";
        case ModloaderType::Forge:    return "Forge";
        case ModloaderType::NeoForge: return "NeoForge";
        case ModloaderType::Paper:    return "Paper";
        default:                      return "Unknown";
    }
}

inline ModloaderType modloaderFromName(const QString &name) {
    const QString n = name.trimmed().toLower();
    if (n == "vanilla")  return ModloaderType::Vanilla;
    if (n == "fabric")   return ModloaderType::Fabric;
    if (n == "quilt")    return ModloaderType::Quilt;
    if (n == "forge")    return ModloaderType::Forge;
    if (n == "neoforge") return ModloaderType::NeoForge;
    if (n == "paper")    return ModloaderType::Paper;
    return ModloaderType::Unknown;
}

// Forge/NeoForge 這類「成對版本」載入器，UI 需要先選 mc 版本、再篩選對應的 loader 版本
inline bool isPairedVersionLoader(ModloaderType t) {
    return t == ModloaderType::Forge || t == ModloaderType::NeoForge;
}

// Fabric/Quilt 這類「獨立版本」載入器，mc 版本與 loader 版本各自獨立選擇
inline bool isIndependentVersionLoader(ModloaderType t) {
    return t == ModloaderType::Fabric || t == ModloaderType::Quilt;
}
