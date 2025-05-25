#include <QApplication>
#include "VideoFrameWidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    VideoFrameWidget w;
    w.show();
    return app.exec();
}