#pragma once

#include <QIODevice>
#include <QAudioFormat>
#include <QMutex>

class SpeechProcessor;

// Dave, drop this and use QAudioBuffer
class AudioBuffer : public QVector<float>
{
public:
    quint64 duration() const;
};

Q_DECLARE_METATYPE(AudioBuffer);

class AudioProcessor : public QIODevice
{
    Q_OBJECT
public:
    AudioProcessor(QObject *parent = nullptr);
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;

    QString text() const;

    static QAudioFormat requiredFormat();

    bool recording() const;
    bool processing() const;

Q_SIGNALS :
    void textChanged();
    void stateChanged();
    void activeVolumeChanged(qreal value);
    void audioChunkRecorded(const AudioBuffer &buffer);

private:
    bool detectNoise();
    void handleNewData();

    QString m_text;
    AudioBuffer m_sampleBuffer;
    AudioBuffer m_buffer;
    bool m_recording = false;
    int m_silenceTime = 0;

    SpeechProcessor *m_speechProcessor;
};

class SpeechProcessor : public QObject
{
    Q_OBJECT
public:
    SpeechProcessor(QObject *parent);
    ~SpeechProcessor();
    bool busy() const {return m_processing;}

    void enqueue(const AudioBuffer &buffer);
Q_SIGNALS:
    void busyChanged();
    void textFound(const QString &text);
private:
    void process();
    struct whisper_context * m_ctx;
    bool m_processing = false;
    AudioBuffer m_pending;
    QMutex m_mutex; //guard m_buffer
};
