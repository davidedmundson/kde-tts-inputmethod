#pragma once

#include <QIODevice>
#include <QAudioFormat>

class AudioBuffer : public QVector<float>
{
public:
    quint64 duration();
};

class Processor : public QIODevice
{
    Q_OBJECT
public:
    Processor(QObject *parent = nullptr);
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    void process();

    QString text() const;

    static QAudioFormat requiredFormat();

Q_SIGNALS :
    void textChanged();
    void activeVolumeChanged(qreal value);

private:
    bool detectNoise(uint);
    void handleNewData();

    QString m_text;
    AudioBuffer m_buffer;
    bool m_recording = false;
    struct whisper_context * ctx;
};
