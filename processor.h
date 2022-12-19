#pragma once

#include <QIODevice>
#include <QAudioFormat>

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

  private:
    QString m_text;
    QVector<float> m_buffer;
    struct whisper_context * ctx;
};
