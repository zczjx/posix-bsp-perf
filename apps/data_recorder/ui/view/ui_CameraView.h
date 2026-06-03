/********************************************************************************
** Form generated from reading UI file 'CameraView.ui'
**
** Created by: Qt User Interface Compiler version 5.15.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CAMERAVIEW_H
#define UI_CAMERAVIEW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "FrameWidget.h"

QT_BEGIN_NAMESPACE

class Ui_VideoFrameWidget
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *dataSourceLabel;
    QComboBox *dataSourceComboBox;
    QPushButton *toggleBtn;
    QSpacerItem *horizontalSpacer;
    QWidget *videoGridContainer;
    QGridLayout *videoGridLayout;

    void setupUi(QWidget *VideoFrameWidget)
    {
        if (VideoFrameWidget->objectName().isEmpty())
            VideoFrameWidget->setObjectName(QString::fromUtf8("VideoFrameWidget"));
        VideoFrameWidget->resize(1280, 720);
        verticalLayout = new QVBoxLayout(VideoFrameWidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        dataSourceLabel = new QLabel(VideoFrameWidget);
        dataSourceLabel->setObjectName(QString::fromUtf8("dataSourceLabel"));

        horizontalLayout->addWidget(dataSourceLabel);

        dataSourceComboBox = new QComboBox(VideoFrameWidget);
        dataSourceComboBox->addItem(QString());
        dataSourceComboBox->addItem(QString());
        dataSourceComboBox->setObjectName(QString::fromUtf8("dataSourceComboBox"));
        dataSourceComboBox->setMinimumSize(QSize(200, 0));

        horizontalLayout->addWidget(dataSourceComboBox);

        toggleBtn = new QPushButton(VideoFrameWidget);
        toggleBtn->setObjectName(QString::fromUtf8("toggleBtn"));
        toggleBtn->setStyleSheet(QString::fromUtf8("QPushButton#toggleBtn {\n"
"    min-width: 60px;\n"
"    padding: 6px 12px;\n"
"    border: 2px outset #999;\n"
"    border-radius: 12px;\n"
"    color: white;\n"
"    background: #d9534f;\n"
"    font-weight: bold;\n"
"}\n"
"QPushButton#toggleBtn:pressed {\n"
"    border-style: inset;\n"
"    padding-top: 8px;\n"
"}\n"
"QPushButton#toggleBtn:checked {\n"
"    background: #5cb85c;\n"
"}"));
        toggleBtn->setCheckable(true);

        horizontalLayout->addWidget(toggleBtn);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        verticalLayout->addLayout(horizontalLayout);

        videoGridContainer = new QWidget(VideoFrameWidget);
        videoGridContainer->setObjectName(QString::fromUtf8("videoGridContainer"));
        videoGridContainer->setMinimumSize(QSize(320, 240));
        videoGridContainer->setStyleSheet(QString::fromUtf8("background-color: black;"));
        videoGridLayout = new QGridLayout(videoGridContainer);
        videoGridLayout->setSpacing(2);
        videoGridLayout->setObjectName(QString::fromUtf8("videoGridLayout"));
        videoGridLayout->setContentsMargins(0, 0, 0, 0);

        verticalLayout->addWidget(videoGridContainer);


        retranslateUi(VideoFrameWidget);

        QMetaObject::connectSlotsByName(VideoFrameWidget);
    } // setupUi

    void retranslateUi(QWidget *VideoFrameWidget)
    {
        VideoFrameWidget->setWindowTitle(QCoreApplication::translate("VideoFrameWidget", "RGB888 \350\247\206\351\242\221\345\270\247\346\230\276\347\244\272", nullptr));
        dataSourceLabel->setText(QCoreApplication::translate("VideoFrameWidget", "Data Source", nullptr));
        dataSourceComboBox->setItemText(0, QCoreApplication::translate("VideoFrameWidget", "Raw Camera", nullptr));
        dataSourceComboBox->setItemText(1, QCoreApplication::translate("VideoFrameWidget", "Objects Detection", nullptr));

        toggleBtn->setText(QCoreApplication::translate("VideoFrameWidget", "Record OFF", nullptr));
    } // retranslateUi

};

namespace Ui {
    class VideoFrameWidget: public Ui_VideoFrameWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CAMERAVIEW_H
