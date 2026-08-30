#include "core/ExportPackager.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

// 注意：QuaZip 標頭與確切列舉名稱依安裝版本略有差異，
// 若編譯時找不到 Z_BZIP2ED / setCompressionMethod，
// 請對照實際安裝路徑下的 quazip.h / quazipfile.h 確認正確名稱。
#include <quazip/quazip.h>
#include <quazip/quazipfile.h>
#include <quazip/quazipnewinfo.h>

namespace ExportPackager {

// zlib/minizip 定義的 BZIP2 壓縮方法代碼
static constexpr int kBzip2Method = 12; // Z_BZIP2ED

bool packDirectoryAsBzip2Zip(const QString &sourceDir, const QString &destZipPath, QString *errorMessage)
{
    QuaZip zip(destZipPath);
    if (!zip.open(QuaZip::mdCreate)) {
        if (errorMessage) *errorMessage = QStringLiteral("無法建立壓縮檔：%1").arg(destZipPath);
        return false;
    }

    const QDir baseDir(sourceDir);
    QDirIterator it(sourceDir, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        const QString absFilePath = it.next();
        const QString relPath = baseDir.relativeFilePath(absFilePath);

        QFile inFile(absFilePath);
        if (!inFile.open(QIODevice::ReadOnly)) {
            if (errorMessage) *errorMessage = QStringLiteral("無法讀取檔案：%1").arg(absFilePath);
            zip.close();
            return false;
        }

        QuaZipFile outFile(&zip);
        const QuaZipNewInfo newInfo(relPath, absFilePath);

        // 以 BZIP2 方法開啟（compression level 對 bzip2 而言採 1~9，9 為最高壓縮率）
        const bool opened = outFile.open(QIODevice::WriteOnly, newInfo,
                                          /*password*/ nullptr, /*crc*/ 0,
                                          /*method*/ kBzip2Method, /*level*/ 9);
        if (!opened) {
            if (errorMessage) *errorMessage = QStringLiteral("壓縮失敗（method=BZIP2）：%1").arg(relPath);
            inFile.close();
            zip.close();
            return false;
        }

        outFile.write(inFile.readAll());
        inFile.close();
        outFile.close();

        if (outFile.getZipError() != UNZ_OK) {
            if (errorMessage) *errorMessage = QStringLiteral("寫入壓縮項目時發生錯誤：%1").arg(relPath);
            zip.close();
            return false;
        }
    }

    zip.close();
    return zip.getZipError() == UNZ_OK;
}

} // namespace ExportPackager
