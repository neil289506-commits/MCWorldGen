#pragma once
#include <QString>

// 提供整個應用程式共用的 QSS 樣式表。
// 注意：QWizard 的 ModernStyle 本身不畫毛玻璃特效，QSS 可以正常套用在
// 裡面的一般 QWidget（按鈕、下拉選單、輸入框、進度條等）上。
namespace AppStyle {
    QString stylesheet();
}
