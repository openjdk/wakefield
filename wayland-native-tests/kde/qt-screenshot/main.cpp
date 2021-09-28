#include "mainwindow.h"

#include <iostream>

#include <QApplication>
#include <QImage>
#include <QScreen>
#include <QPixmap>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    MainWindow w;
    w.show();

    QScreen *screen = application.screens().at(0);
    QImage image = screen->grabWindow().toImage();

    QString imagePath = QString("/home/neugens/qt-test.png");
    bool didSave = image.save(imagePath, "PNG");

    std::cout << "Image saved... " << didSave << std::endl;

    return application.exec();
}
