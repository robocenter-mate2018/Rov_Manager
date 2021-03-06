#include "mainwidget.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <iostream>
#include <QDoubleValidator>

MainWidget::MainWidget(QWidget *parent) :
    QWidget(parent),
    _screenBtn(new QPushButton(this)),
    _searchBtn(new QPushButton(this)),
    _videoWidget(new VideoWidget(this))
{
    createLayout();
    initializeConnetions();
}

void MainWidget::createLayout()
{
    QHBoxLayout *mainLayout = new QHBoxLayout;
    QHBoxLayout* camLayout = new QHBoxLayout;
    QVBoxLayout* menuLayout = new QVBoxLayout;\

    camLayout->addWidget(_videoWidget);
    _screenBtn->setText("Скриншот");
    _searchBtn->setText("Начать распознавание");

    menuLayout->addWidget(_screenBtn);
    menuLayout->addWidget(_searchBtn);
    mainLayout->addLayout(camLayout);
    mainLayout->addLayout(menuLayout);
    this->setLayout(mainLayout);
}

void MainWidget::initializeConnetions()
{
    connect(_screenBtn, &QPushButton::pressed, this, &MainWidget::buttonScreenPress);
    connect(_searchBtn, &QPushButton::pressed, this, &MainWidget::buttonSearchPress);
}

void MainWidget::buttonScreenPress()
{
    ScreenshotWindow* w(new ScreenshotWindow(_videoWidget->GetPixmap(), this));
    w->show();
}

void MainWidget::buttonSearchPress()
{
    if (_searchBtn->text() == "Начать распознавание"){
        _searchBtn->setText("Остановить распознавание");
        _videoWidget->StartStopRecognition(true);
    }
    else {
        _searchBtn->setText("Начать распознавание");
        _videoWidget->StartStopRecognition(false);
    }
}
