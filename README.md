# WorldGenModule

Minecraft 世界預生成工具（Qt6 C++ / vcpkg / QuaZip + BZip2）。

## 架構總覽

```
使用者操作（WorldGenWizard 四步驟精靈）
   1. LoaderPage      → 選擇載入器 / 遊戲版本 / 載入器版本
                         （VersionFetcher：Python 批次抓取 + 解析官方 API）
   2. WorldTypePage   → 世界類型、極限模式、獎勵箱、結構生成、種子碼
   3. AssetsPage      → 選擇要一鍵匯入的 mods / datapack
   4. RunExportPage   → 顯示即時 console、選擇匯出路徑、觸發整條流程
        │
        ▼
WorldGenController（總控）
   ├─ ServerCoreInstaller   下載並安裝對應載入器的伺服器核心到 %TEMP%\worldgen-pregen-<時間戳>，
   │                        安裝完成後解析出 LaunchSpec（該用什麼指令啟動這個伺服器）
   ├─ AssetImporter         複製 mods → mods/，datapack → <level-name>/datapacks/
   ├─ ServerPropertiesWriter 以 PropertiesFile 為底寫出 server.properties（種子、世界類型、旗標）
   ├─ PreGenRunner           依 LaunchSpec 用 QProcess 啟動伺服器，即時轉發 console，
   │                         偵測 "Done (" 後自動送出 stop 指令
   └─ ExportPackager         以 QuaZip 的 BZIP2 method 打包整個資料夾成使用者指定的 .zip
```

## 各載入器的版本選擇模式（LoaderPage）

抓版本邏輯沿用參考專案的分類方式，UI 依載入器特性分成三種模式：

| 載入器 | 模式 | 說明 |
|---|---|---|
| Vanilla / Paper | 單一選單 | 只需選遊戲版本，無獨立載入器版本 |
| Fabric / Quilt | 獨立雙選單 | 遊戲版本、載入器版本互相獨立，任意組合皆可 |
| Forge / NeoForge | 成對選單 | 每個遊戲版本只對應特定載入器版本，選完遊戲版本會篩選載入器版本清單 |

## 各載入器安裝完成後的啟動方式（ServerCoreInstaller::resolveLaunchSpec）

| 載入器 | 啟動方式 |
|---|---|
| Vanilla | `java -jar server.jar nogui` |
| Fabric | `java -jar fabric-server-launch.jar nogui` |
| Quilt | `java -jar quilt-server-launch.jar nogui` |
| Paper | `java -jar paper-<版本>-<build>.jar nogui`（檔名動態搜尋） |
| Forge / NeoForge（MC ≥ 1.18.1） | 執行 installer 產生的 `run.bat`（Windows）/ `run.sh`（其他），並帶入 `nogui` |
| Forge（MC < 1.18.1） | `java -jar forge-*-universal.jar nogui`（動態搜尋檔名） |

## 建置方式

```bash
# 1. 安裝相依套件（manifest 模式，vcpkg.json 已宣告 quazip[qt6] / bzip2 / zlib）
vcpkg install

# 2. 設定並建置
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### 額外執行環境需求

- **Java**：預生成階段需要能執行對應版本伺服器的 JRE/JDK，並在 PATH 中找得到 `java`
  （或在 `WorldGenConfig::javaExecutable` 指定完整路徑）。
- **Python 3 + requests**：`VersionFetcher::fetchAll()` 會呼叫外部 `python3`（Windows 為 `python`）
  執行自動產生的 `requestVer.py` 腳本抓取 Vanilla/Fabric/Quilt/Forge/NeoForge 版本資料，
  需確保執行環境已安裝 Python 3 與 `requests` 套件（`pip install requests`）。
  Paper 版本清單不受此限制，直接用 Qt 的 `QNetworkAccessManager` 查詢。


   `chunky radius <n>` / `chunky start` 等指令，再監聽其完成訊息。
4. **NeoForge 新舊式判斷**：目前假設 NeoForge 支援的版本皆為新式 `run.bat/run.sh` 安裝
   （因為 NeoForge 本身只支援 1.20.1 之後的版本），若未來版本範圍改變需重新確認。
