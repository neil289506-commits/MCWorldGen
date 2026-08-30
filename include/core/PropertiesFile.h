#pragma once
#include <QString>
#include <QVector>
#include <QMap>

// 讀寫 server.properties，並保留原始行順序與註解行（#開頭），
// 讓使用者在 UI 編輯後存檔不會把手動加的註解洗掉。
class PropertiesFile
{
public:
    struct Entry {
        QString key;      // 空字串代表這行是註解或空白行（原樣保留於 rawLine）
        QString value;
        QString rawLine;  // 非 key=value 行時使用（註解、空行）
        bool isComment() const { return key.isEmpty(); }
    };

    bool load(const QString &filePath);
    bool save(const QString &filePath) const;

    // 跟 load() 一樣讀檔，但檔案不存在時（伺服器尚未啟動過一次，
    // Minecraft 還沒自動產生 server.properties）改成載入內建的預設範本，
    // 讓使用者在第一次啟動前也能先看到、先調整設定，而不是空白頁面。
    // 回傳值恆為 true（一定會有內容可顯示）。
    bool loadOrDefault(const QString &filePath);

    QString value(const QString &key, const QString &defaultValue = {}) const;
    void setValue(const QString &key, const QString &value);
    void removeKey(const QString &key);

    // server.properties 常用欄位的型別轉換輔助
    int intValue(const QString &key, int defaultValue = 0) const;
    bool boolValue(const QString &key, bool defaultValue = false) const;

    const QVector<Entry> &entries() const { return m_entries; }

    // 一鍵取得可分享的伺服器位址（server.properties 沒有存 IP，
    // 這裡回傳 "本機區網IP:port"；公網 IP 需另外呼叫外部服務查詢）
    QString serverPort() const { return value("server-port", "25565"); }

private:
    QVector<Entry> m_entries;
    QMap<QString, int> m_keyIndex; // key -> m_entries 索引，加速查找/覆寫
};
