#pragma once
#include <QString>

namespace ExportPackager {
    // 將 sourceDir 底下所有內容以 BZip2 壓縮方式打包成單一 zip，輸出到 destZipPath
    // 回傳是否成功；失敗時 errorMessage 會填入原因
    bool packDirectoryAsBzip2Zip(const QString &sourceDir, const QString &destZipPath,
                                  QString *errorMessage = nullptr);
}
