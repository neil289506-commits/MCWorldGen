#include "core/PropertiesFile.h"
#include <QFile>
#include <QTextStream>
#include <QPair>

bool PropertiesFile::load(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

    m_entries.clear();
    m_keyIndex.clear();

    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QString trimmed = line.trimmed();

        Entry e;
        if (trimmed.isEmpty() || trimmed.startsWith('#')) {
            e.rawLine = line; // 保留原樣，含縮排
        } else {
            const int idx = trimmed.indexOf('=');
            if (idx < 0) {
                e.rawLine = line; // 不是合法 key=value，當作原樣行保留，避免資料流失
            } else {
                e.key   = trimmed.left(idx).trimmed();
                e.value = trimmed.mid(idx + 1); // Minecraft properties 值允許前導/尾隨空白，不 trim
                m_keyIndex.insert(e.key, m_entries.size());
            }
        }
        m_entries.push_back(e);
    }
    return true;
}

bool PropertiesFile::loadOrDefault(const QString &filePath)
{
    if (load(filePath)) return true;

    // 檔案不存在（伺服器還沒啟動過一次）：載入跟官方 Minecraft 伺服器
    // 首次啟動時自動產生的內容一致的預設範本，讓 UI 有東西可顯示、
    // 使用者可以先調整，等第一次啟動時這些值也會被寫入真正的檔案。
    m_entries.clear();
    m_keyIndex.clear();

    static const QVector<QPair<QString, QString>> defaults = {
        {"accepts-transfers", "false"},
        {"allow-flight", "false"},
        {"allow-nether", "true"},
        {"broadcast-console-to-ops", "true"},
        {"broadcast-rcon-to-ops", "true"},
        {"bug-report-link", ""},
        {"difficulty", "easy"},
        {"enable-command-block", "false"},
        {"enable-jmx-monitoring", "false"},
        {"enable-query", "false"},
        {"enable-rcon", "false"},
        {"enable-status", "true"},
        {"enforce-secure-profile", "true"},
        {"enforce-whitelist", "false"},
        {"entity-broadcast-range-percentage", "100"},
        {"force-gamemode", "false"},
        {"function-permission-level", "2"},
        {"gamemode", "survival"},
        {"generate-structures", "true"},
        {"generator-settings", "{}"},
        {"hardcore", "false"},
        {"hide-online-players", "false"},
        {"initial-disabled-packs", ""},
        {"initial-enabled-packs", "vanilla"},
        {"level-name", "world"},
        {"level-seed", ""},
        {"level-type", "minecraft:normal"},
        {"log-ips", "true"},
        {"max-chained-neighbor-updates", "1000000"},
        {"max-players", "20"},
        {"max-tick-time", "60000"},
        {"max-world-size", "29999984"},
        {"motd", "A Minecraft Server"},
        {"network-compression-threshold", "256"},
        {"online-mode", "true"},
        {"op-permission-level", "4"},
        {"pause-when-empty-seconds", "60"},
        {"player-idle-timeout", "0"},
        {"prevent-proxy-connections", "false"},
        {"pvp", "true"},
        {"query.port", "25565"},
        {"rate-limit", "0"},
        {"rcon.password", ""},
        {"rcon.port", "25575"},
        {"require-resource-pack", "false"},
        {"resource-pack", ""},
        {"resource-pack-id", ""},
        {"resource-pack-prompt", ""},
        {"resource-pack-sha1", ""},
        {"server-ip", ""},
        {"server-port", "25565"},
        {"simulation-distance", "10"},
        {"spawn-monsters", "true"},
        {"spawn-protection", "16"},
        {"sync-chunk-writes", "true"},
        {"text-filtering-config", ""},
        {"text-filtering-version", "0"},
        {"use-native-transport", "true"},
        {"view-distance", "10"},
        {"white-list", "false"},
    };

    for (const auto &kv : defaults) {
        Entry e;
        e.key   = kv.first;
        e.value = kv.second;
        m_keyIndex.insert(e.key, m_entries.size());
        m_entries.push_back(e);
    }
    return true;
}

bool PropertiesFile::save(const QString &filePath) const
{
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;

    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    for (const Entry &e : m_entries) {
        if (e.isComment()) {
            out << e.rawLine << '\n';
        } else {
            out << e.key << '=' << e.value << '\n';
        }
    }
    return true;
}

QString PropertiesFile::value(const QString &key, const QString &defaultValue) const
{
    const auto it = m_keyIndex.constFind(key);
    if (it == m_keyIndex.constEnd()) return defaultValue;
    return m_entries.at(it.value()).value;
}

void PropertiesFile::setValue(const QString &key, const QString &value)
{
    const auto it = m_keyIndex.constFind(key);
    if (it != m_keyIndex.constEnd()) {
        m_entries[it.value()].value = value;
        return;
    }
    Entry e;
    e.key = key;
    e.value = value;
    m_keyIndex.insert(key, m_entries.size());
    m_entries.push_back(e);
}

void PropertiesFile::removeKey(const QString &key)
{
    const auto it = m_keyIndex.constFind(key);
    if (it == m_keyIndex.constEnd()) return;
    const int idx = it.value();
    m_entries.remove(idx);
    m_keyIndex.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        if (!m_entries[i].isComment())
            m_keyIndex.insert(m_entries[i].key, i);
    }
}

int PropertiesFile::intValue(const QString &key, int defaultValue) const
{
    bool ok = false;
    const int v = value(key).toInt(&ok);
    return ok ? v : defaultValue;
}

bool PropertiesFile::boolValue(const QString &key, bool defaultValue) const
{
    const QString v = value(key).trimmed().toLower();
    if (v == "true") return true;
    if (v == "false") return false;
    return defaultValue;
}
