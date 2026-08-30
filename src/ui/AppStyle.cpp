#include "ui/AppStyle.h"

namespace AppStyle {

QString stylesheet()
{
    // 深色（黑底）主題：整體背景改為黑色，維持 Minecraft 草地綠（#5B8C3E 系列）當強調色，
    // 文字改亮色系以確保在黑底上有足夠對比。
    return QStringLiteral(R"(
        QWizard, QWidget {
            background-color: #0B0B0B;
            color: #E7ECE3;
            font-family: "Segoe UI", "Microsoft JhengHei", sans-serif;
            font-size: 14px;
        }

        QWizard QLabel#qt_wizard_title {
            font-size: 20px;
            font-weight: 600;
            color: #8FBF6E;
        }

        QLabel {
            color: #E7ECE3;
        }

        QLabel#stageLabel {
            font-weight: 600;
            color: #8FBF6E;
            padding: 2px 0;
        }

        QComboBox, QLineEdit {
            background-color: #1A1B18;
            color: #E7ECE3;
            border: 1px solid #3A3D36;
            border-radius: 6px;
            padding: 6px 10px;
            min-height: 22px;
        }

        QComboBox:hover, QLineEdit:hover {
            border: 1px solid #5B8C3E;
        }

        QComboBox QAbstractItemView {
            background-color: #1A1B18;
            color: #E7ECE3;
            selection-background-color: #3A5C28;
            selection-color: #FFFFFF;
            border: 1px solid #3A3D36;
        }

        QComboBox::drop-down {
            border: none;
            width: 24px;
        }

        QCheckBox {
            spacing: 8px;
            padding: 2px 0;
        }

        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 4px;
            border: 1px solid #3A3D36;
            background-color: #1A1B18;
        }

        QCheckBox::indicator:checked {
            background-color: #5B8C3E;
            border: 1px solid #6B9E4B;
        }

        QPushButton {
            background-color: #1A1B18;
            color: #E7ECE3;
            border: 1px solid #3A3D36;
            border-radius: 6px;
            padding: 7px 18px;
            min-width: 72px;
        }

        QPushButton:hover {
            border: 1px solid #5B8C3E;
            color: #8FBF6E;
        }

        QPushButton:pressed {
            background-color: #26281F;
        }

        QPushButton:disabled {
            color: #5A5F54;
            background-color: #16170F;
            border: 1px solid #24261D;
        }

        QPushButton#primaryButton {
            background-color: #5B8C3E;
            color: #FFFFFF;
            border: 1px solid #6B9E4B;
            font-weight: 600;
        }

        QPushButton#primaryButton:hover {
            background-color: #6B9E4B;
        }

        QPushButton#primaryButton:disabled {
            background-color: #3A4A2E;
            border: 1px solid #3A4A2E;
            color: #9AA394;
        }

        /* QWizard 內建 Next/Back/Cancel 按鈕群 */
        QDialogButtonBox QPushButton {
            min-width: 88px;
        }

        QListWidget {
            background-color: #1A1B18;
            color: #E7ECE3;
            border: 1px solid #3A3D36;
            border-radius: 6px;
            padding: 4px;
        }

        QListWidget::item {
            padding: 4px 6px;
            border-radius: 4px;
        }

        QListWidget::item:selected {
            background-color: #3A5C28;
            color: #FFFFFF;
        }

        QProgressBar {
            background-color: #1A1B18;
            border: 1px solid #3A3D36;
            border-radius: 6px;
            text-align: center;
            color: #E7ECE3;
            height: 20px;
        }

        QProgressBar::chunk {
            background-color: #5B8C3E;
            border-radius: 6px;
        }

        QProgressBar#busyBar::chunk {
            background-color: #8FBF6E;
        }

        /* console 區塊：比外層再深一階，維持終端機質感 */
        ConsoleWidget, QPlainTextEdit {
            background-color: #000000;
            color: #8FE38A;
            border: 1px solid #2A2C26;
            border-radius: 6px;
            padding: 8px;
            selection-background-color: #5B8C3E;
        }
    )");
}

} // namespace AppStyle
