#include "audiosource.h"

#include "processor.h"
#include "whisper.cpp/whisper.h"

#include <stdlib.h>
#include <math.h>

#include <QDateTime>
#include <QDebug>
#include <QPainter>
#include <QVBoxLayout>
#include <QAudioDeviceInfo>
#include <QAudioInput>
#include <qendian.h>

InputTest::InputTest()
{
    m_textInferrer = new Processor(this);
    initializeWindow();
    initializeAudio(QAudioDeviceInfo::defaultInputDevice());
}

void InputTest::initializeWindow()
{
    QWidget *window = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout;

    m_deviceBox = new QComboBox(this);
    const QAudioDeviceInfo &defaultDeviceInfo = QAudioDeviceInfo::defaultInputDevice();
    m_deviceBox->addItem(defaultDeviceInfo.deviceName(), QVariant::fromValue(defaultDeviceInfo));
    for (auto &deviceInfo: QAudioDeviceInfo::availableDevices(QAudio::AudioInput)) {
        if (deviceInfo != defaultDeviceInfo)
            m_deviceBox->addItem(deviceInfo.deviceName(), QVariant::fromValue(deviceInfo));
    }

    connect(m_deviceBox, QOverload<int>::of(&QComboBox::activated), this, &InputTest::deviceChanged);
    layout->addWidget(m_deviceBox);

    window->setLayout(layout);

    setCentralWidget(window);
    window->show();
    qDebug() << "DAVE!";
}


void InputTest::initializeAudio(const QAudioDeviceInfo &deviceInfo)
{
    qDebug() << "DAVE222!";

    QAudioFormat format = Processor::requiredFormat();

    if (!deviceInfo.isFormatSupported(format)) {
        qWarning() << "Default format not supported - trying to use nearest";
        format = deviceInfo.nearestFormat(format);
    }

    m_audioInput.reset(new QAudioInput(deviceInfo, format));

    connect(m_audioInput.data(), &QAudioInput::stateChanged, this, [this]() {
        qDebug() << "State changed" << m_audioInput->state();
    });

    m_audioInput->start(m_textInferrer);
}

void InputTest::deviceChanged(int index)
{
    initializeAudio(m_deviceBox->itemData(index).value<QAudioDeviceInfo>());
}


#include "audiosource.moc"
