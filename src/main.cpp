#include <QApplication>
#include "ui/WorldGenWizard.h"
#include "ui/AppStyle.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyleSheet(AppStyle::stylesheet());

    WorldGenWizard wizard;
    wizard.show();

    return app.exec();
}
