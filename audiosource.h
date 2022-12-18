#ifndef AUDIOINPUT_H
#define AUDIOINPUT_H

#include <QAudioInput>
#include <QByteArray>
#include <QComboBox>
#include <QMainWindow>
#include <QObject>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QWidget>
#include <QScopedPointer>

class TextInferrer;

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

    QScopedPointer<QAudioInput> m_audioInput;
    TextInferrer* m_textInferrer;
    QAudioFormat m_format;
};

#endif // AUDIOINPUT_H