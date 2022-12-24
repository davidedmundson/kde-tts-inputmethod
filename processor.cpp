#include "processor.h"

#include <QTimer>
#include <QThread>
#include <QDebug>
#include <QGuiApplication>

#include <QThreadPool>
#include <QFuture>
#include <QFutureWatcher>
#include <QMutexLocker>

#include <QStandardPaths>

#include "whisper.cpp/whisper.h"

#include <cmath>

#define WHISPER_SAMPLE_RATE_MS (WHISPER_SAMPLE_RATE / 1000)

class SpeechProcessor;

// size of chunk to analyze for new audio
static const int s_sampleSizeMs = 200;

// after this much silence we trigger processing
static const int s_silenceTime = 2000;

quint64 AudioBuffer::duration() const
{
    return (size() * 1000.0) / WHISPER_SAMPLE_RATE;
}


AudioProcessor::AudioProcessor(QObject *parent)
    : QIODevice(parent)
{
    // TODO move this out to the caller rather than having AudioProcessor abstract
    // then make the caller set up a chain  of AudioInput -> AudioProcessor -> SpeechProcessor -> DocumentHandler

    qRegisterMetaType<AudioBuffer>();

    auto thread = new QThread(this);
    m_speechProcessor = new SpeechProcessor(nullptr); // can't use a parent with a thread, what's the normal pattern here?
    m_speechProcessor->moveToThread(thread);

    connect(this, &AudioProcessor::audioChunkRecorded, m_speechProcessor, &SpeechProcessor::enqueue, Qt::DirectConnection);
    connect(m_speechProcessor, &SpeechProcessor::textFound, this, [this](const QString &text) {
        m_text += text;
        Q_EMIT textChanged();
    });
    connect(m_speechProcessor, &SpeechProcessor::busyChanged, this, &AudioProcessor::stateChanged);

    thread->start();

    open(QIODevice::WriteOnly);
}

QAudioFormat AudioProcessor::requiredFormat()
{
    QAudioFormat format;
    format.setSampleRate(WHISPER_SAMPLE_RATE);
    format.setChannelCount(1);
    format.setSampleSize(32);
    format.setSampleType(QAudioFormat::Float);
    format.setByteOrder(QAudioFormat::LittleEndian);
    return format;
}

bool AudioProcessor::recording() const
{
    return m_recording;
}

bool AudioProcessor::processing() const
{
    return m_speechProcessor->busy();
}

qint64 AudioProcessor::readData(char *, qint64 )
{
    return 0;
}


// copied from whisperc++
void high_pass_filter(AudioBuffer &data, float cutoff, float sample_rate) {
    const float rc = 1.0f / (2.0f * M_PI * cutoff);
    const float dt = 1.0f / sample_rate;
    const float alpha = dt / (rc + dt);

    float y = data[0];

    for (size_t i = 1; i < data.size(); i++) {
        y = alpha * (y + data[i] - data[i - 1]);
        data[i] = y;
    }
}

bool AudioProcessor::detectNoise()
{
    // go through the last duration's worth of cycles
    float energy = 0;
    for (auto it = m_sampleBuffer.constBegin() ; it  != m_sampleBuffer.constEnd() ; it++) {
        energy += fabsf(*it);
    }
    energy /= m_sampleBuffer.count();

    Q_EMIT activeVolumeChanged(energy);
    static float threshold = 0.015;
    return energy > threshold;
}


/**
 * Overall plan:
 *  - Udpate our buffer cache
 *  - For every N msecs of audio we look for noise
 *  - If there's no noise and we're not recording yet we can throw it away
 *  - Otherwise we flag ourselves as recording
 *  - If there's a new silence, we start processing
 *
 * TODO:
 *  - understand energy_all variable in vad_simple in whisper. Seems to make it relative volume compared to buffer?
 *  - add the high pass filter?
 *
 *  - port to libfvad?
 */
void AudioProcessor::handleNewData()
{
    high_pass_filter(m_sampleBuffer, 100.0, WHISPER_SAMPLE_RATE);

    const bool noiseInCurrentChunk = detectNoise();

    if (!m_recording) {
        if (noiseInCurrentChunk) {
            // start with our current chunk
            m_buffer = m_sampleBuffer;
            m_recording = true;
            Q_EMIT stateChanged();
        }
    } else {
        m_buffer.append(m_sampleBuffer);

        if (!noiseInCurrentChunk) {
            m_silenceTime += s_sampleSizeMs;
        }

        if (m_silenceTime > s_silenceTime) {
            m_recording = false;
            Q_EMIT stateChanged();
            // we could drop the silent time?
            Q_EMIT audioChunkRecorded(m_buffer);
            m_silenceTime = 0;
        }
    }
}


qint64 AudioProcessor::writeData(const char *data, qint64 len)
{
    auto floatData = (float*)data;
    for (unsigned int i = 0 ; i < len / sizeof(float) ; i++) {
        // super inneficient
        m_sampleBuffer.append(floatData[i]);
        if (m_sampleBuffer.duration() == s_sampleSizeMs) {
            handleNewData();
            m_sampleBuffer.clear();
            m_sampleBuffer.reserve(s_sampleSizeMs * WHISPER_SAMPLE_RATE_MS);
        }
    }
    return len;
}


SpeechProcessor::SpeechProcessor(QObject *parent)
    : QObject(parent)
{
    //whisper stuff
    QByteArray model = "models/ggml-small.en.bin";
    // TODO verify file exists
    // TODO language
    // TODO download the right model automatically
    m_ctx = whisper_init(model.data());
}

SpeechProcessor::~SpeechProcessor()
{
    whisper_free(m_ctx);
}

void SpeechProcessor::enqueue(const AudioBuffer &buffer)
{
    QMutexLocker lock(&m_mutex);
    if (m_pending.count()) {
        qWarning() << "New buffer arrived whilst processing the old one. Queuing";
        m_pending.append(buffer);
        return;
    } else {
        m_pending = buffer;
        QMetaObject::invokeMethod(this, &SpeechProcessor::process, Qt::QueuedConnection);
    }
}

void SpeechProcessor::process()
{
    m_processing = true;
    Q_EMIT busyChanged();

    AudioBuffer buffer;
    {
        QMutexLocker lock(&m_mutex);
        buffer = m_pending;
        m_pending.clear();
    }

    QElapsedTimer bench;
    bench.start();
    qDebug() << "proces start";

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    wparams.print_progress   = false;
    wparams.print_special    = false;
    wparams.print_realtime   = false;
    wparams.translate        = false;
    wparams.no_context       = true;
    wparams.single_segment   = true;
    wparams.max_tokens       = 0;
    wparams.language         = "en";
    wparams.n_threads        = QThread::idealThreadCount();
    // wparams.audio_ctx        = params.audio_ctx;
    wparams.speed_up         = false; //?

    // tokens from the last itteration
    // wparams.prompt_tokens    = params.no_context ? nullptr : prompt_tokens.data();
    // wparams.prompt_n_tokens  = params.no_context ? 0       : prompt_tokens.size();

    qDebug() << "processing " << buffer.duration() << "ms of audio";

    if (whisper_full(m_ctx, wparams, (const float*) buffer.data(), buffer.size()) != 0) {
        qApp->exit(-1);
    }

    QString output;
    const int n_segments = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char * text = whisper_full_get_segment_text(m_ctx, i);
        // TODO we can get blobs with timestamps here
        output += text;
    }

    qDebug() << "Done" << bench.elapsed();

    Q_EMIT textFound(output);

    m_processing = false;
    Q_EMIT busyChanged();
}

QString AudioProcessor::text() const
{
    return m_text;
}
