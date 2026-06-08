#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("OTA Update Manager");
    app.setOrganizationName("Vehicle OTA");

    MainWindow w;
    w.show();
    return app.exec();
}
