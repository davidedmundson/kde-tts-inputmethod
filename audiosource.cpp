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
#include <QLabel>
#include <qendian.h>


class RenderArea : public QWidget
{
    Q_OBJECT

public:
    explicit RenderArea(QWidget *parent = nullptr);

    void setLevel(qreal value);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_level = 0;
};

RenderArea::RenderArea(QWidget *parent)
    : QWidget(parent)
{
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);

    setMinimumHeight(30);
    setMinimumWidth(200);
}

void RenderArea::paintEvent(QPaintEvent * /* event */)
{
    QPainter painter(this);

    painter.setPen(Qt::black);

    const QRect frame = painter.viewport() - QMargins(10, 10, 10, 10);
    painter.drawRect(frame);
    if (m_level == 0.0)
        return;

    const int pos = qRound(qreal(frame.width() - 1) * m_level);
    painter.fillRect(frame.left() + 1, frame.top() + 1,
                     pos, frame.height() - 1, Qt::red);
}

void RenderArea::setLevel(qreal value)
{
    m_level = value;
    update();
}

InputTest::InputTest()
{
    m_audioProcessor = new AudioProcessor(this);
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


    auto vuMeter = new RenderArea(this);
    connect(m_audioProcessor, &AudioProcessor::activeVolumeChanged, this, [vuMeter](qreal level) {
        vuMeter->setLevel(level);
    });
    layout->addWidget(vuMeter);

    auto stateLabel = new QLabel(this);
    connect(m_audioProcessor, &AudioProcessor::stateChanged, this, [stateLabel, this]() {
        QString text;
        if (m_audioProcessor->recording()) {
            text += "Recording ";
        }
        if (m_audioProcessor->processing()) {
            text += "Processing";
        }
        stateLabel->setText(text);
    });
    layout->addWidget(stateLabel);

    connect(m_deviceBox, QOverload<int>::of(&QComboBox::activated), this, &InputTest::deviceChanged);
    layout->addWidget(m_deviceBox);

    m_textEdit = new QTextEdit(this);
    layout->addWidget(m_textEdit);
    connect(m_audioProcessor, &AudioProcessor::textChanged, this, [this]() {
        m_textEdit->setText(m_audioProcessor->text());
    });

    window->setLayout(layout);

    setCentralWidget(window);
    window->show();
}


void InputTest::initializeAudio(const QAudioDeviceInfo &deviceInfo)
{
    QAudioFormat format = AudioProcessor::requiredFormat();

    if (!deviceInfo.isFormatSupported(format)) {
        qWarning() << "Default format not supported - trying to use nearest";
        format = deviceInfo.nearestFormat(format);
    }

    m_audioInput.reset(new QAudioInput(deviceInfo, format));

    connect(m_audioInput.data(), &QAudioInput::stateChanged, this, [this]() {
        qDebug() << "State changed" << m_audioInput->state();
    });

    m_audioInput->start(m_audioProcessor);
}

void InputTest::deviceChanged(int index)
{
    initializeAudio(m_deviceBox->itemData(index).value<QAudioDeviceInfo>());
}


#include "audiosource.moc"
