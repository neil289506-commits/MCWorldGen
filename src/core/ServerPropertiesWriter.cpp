#include "core/ServerPropertiesWriter.h"
#include "core/PropertiesFile.h"
#include <QRandomGenerator64>

// 改用參考專案的 PropertiesFile（loadOrDefault 已內建與官方伺服器
// 首次啟動一致的完整預設欄位），worldgen 只需要在其上覆寫世界生成相關的欄位，
// 不用自己重新維護一份不完整的 server.properties 範本。
namespace ServerPropertiesWriter {

QString resolveSeed(const QString &seedInput)
{
    if (!seedInput.trimmed().isEmpty())
        return seedInput.trimmed();
    return QString::number(static_cast<qint64>(QRandomGenerator64::global()->generate64()));
}

bool write(const WorldGenConfig &config, const QString &dirPath)
{
    const QString path = dirPath + "/server.properties";

    PropertiesFile props;
    props.loadOrDefault(path); // 檔案還不存在時會載入內建的官方預設範本

    props.setValue("level-seed", resolveSeed(config.seed));
    props.setValue("level-name", config.levelName);
    props.setValue("level-type", worldTypeToLevelType(config.worldType));
    props.setValue("generate-structures", config.generateStructures ? "true" : "false");
    props.setValue("hardcore", config.hardcore ? "true" : "false");

    // 獎勵箱在 hardcore 模式下無效，寫入前先做保護判斷
    const bool bonusChestEffective = config.bonusChest && !config.hardcore;
    props.setValue("generate-bonus-chest", bonusChestEffective ? "true" : "false");

    if (config.worldType == WorldType::Flat) {
        props.setValue("generator-settings",
            "{\"biome\":\"minecraft:plains\",\"layers\":"
            "[{\"block\":\"minecraft:bedrock\",\"height\":1},"
            "{\"block\":\"minecraft:dirt\",\"height\":2},"
            "{\"block\":\"minecraft:grass_block\",\"height\":1}]}");
    }

    return props.save(path);
}

} // namespace ServerPropertiesWriter
