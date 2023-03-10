#ifndef AUDIOINPUT_H
#define AUDIOINPUT_H

#include "processor.h"
#include <QAudioInput>
#include <QByteArray>
#include <QComboBox>
#include <QMainWindow>
#include <QObject>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QWidget>
#include <QTextEdit>
#include <QScopedPointer>

class AudioProcessor;

class InputTest : public QMainWindow
{
    Q_OBJECT

public:
    InputTest();

private:
    void initializeWindow();
    void initializeAudio(const QAudioDeviceInfo &deviceInfo);

private slots:
    void deviceChanged(int index);

private:
    qreal calculateLevel(const char*, qint64 len) const;
    QComboBox *m_deviceBox = nullptr;
    QTextEdit *m_textEdit = nullptr;

    QScopedPointer<QAudioInput> m_audioInput;
    AudioProcessor *m_audioProcessor;
};

#endif // AUDIOINPUT_H
