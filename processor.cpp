#include "processor.h"

#include <QTimer>
#include <QThread>
#include <QDebug>
#include <QGuiApplication>

#include "whisper.cpp/whisper.h"

#include <cmath>

#define WHISPER_SAMPLE_RATE_MS WHISPER_SAMPLE_RATE / 1000;

// Tuning
// from stream:
//int32_t step_ms    = 3000;
//int32_t length_ms  = 10000;
///    const int n_samples_keep = 0.2*WHISPER_SAMPLE_RATE; // amount to use between searches

quint64 AudioBuffer::duration()
{
    return (size() * 1000.0) / WHISPER_SAMPLE_RATE;
}

Processor::Processor(QObject *parent)
    : QIODevice(parent)
{
    //whisper stuff
    QByteArray model = "models/ggml-small.en.bin";
    // TODO verify file exists
    // TODO language
    // TODO download the right model automatically
    ctx = whisper_init(model.data());

    open(QIODevice::WriteOnly);
}

QAudioFormat Processor::requiredFormat()
{
    QAudioFormat format;
    format.setSampleRate(WHISPER_SAMPLE_RATE);
    format.setChannelCount(1);
    format.setSampleSize(32);
    format.setSampleType(QAudioFormat::Float);
    format.setByteOrder(QAudioFormat::LittleEndian);
    return format;
}

qint64 Processor::readData(char *, qint64 )
{
    return 0;
}


// copied from whisperc++
void high_pass_filter(std::vector<float> & data, float cutoff, float sample_rate) {
    const float rc = 1.0f / (2.0f * M_PI * cutoff);
    const float dt = 1.0f / sample_rate;
    const float alpha = dt / (rc + dt);

    float y = data[0];

    for (size_t i = 1; i < data.size(); i++) {
        y = alpha * (y + data[i] - data[i - 1]);
        data[i] = y;
    }
}

// copied from whisperc++ examples
// TODO port to vector start and end

bool Processor::detectNoise(uint duration)
{
    if (m_buffer.duration() < duration) {
        return false;
    }

    uint nSamples = duration * WHISPER_SAMPLE_RATE_MS;

    // go through the last duration's worth of cycles
    // take a mean

    float energy = 0;
    for (auto it = m_buffer.constEnd() - (nSamples) ; it  != m_buffer.constEnd() ; it++) {
        energy += fabsf(*it);
    }
    energy /= nSamples;

    Q_EMIT activeVolumeChanged(energy);
    static float threshold = 0.3;
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
 */
void Processor::handleNewData()
{
    if (m_buffer.duration() < 1000) {
        return;
    }

    // size of sliding window to use to detect audio start / end respectively
    static const int startVoiceTimer = 1000;
    static const int endVoicePauseTimer = 2000;

    // DAVE, new plan. Not done yet

    // When not recording
        // front buffer is less than sample size
        // front buffer and back buffer. Each take a sample
        // If low energy - swap buffers and continue
        // If high energy append backBuffer, frontBuffer as m_buffer and flag as recording
    // When recording:
        // still build front buffers, but in handleData we append them to m_buffer now
        // after N seconds of silence, stop recording
    // Optimise afterwards

    if (!m_recording) {
        if (detectNoise(startVoiceTimer)) {
            qDebug() << "start voice";
            m_recording = true;
        } else {
            // TODO, keep the last 100ms or so?
            m_buffer.clear();
            return;
        }
    } else {
        if (!detectNoise(endVoicePauseTimer)) {
            qDebug() << "end voice " << m_buffer.duration();
            m_recording = false;
            if (m_buffer.size() <= 2000) {
                // was probably just noise. There's scope for being much cleverer in a lot of places here
                m_buffer.clear();
                return;
            }
            process();
        }
    }
}


qint64 Processor::writeData(const char *data, qint64 len)
{
    // do stuff with every 1 second of audio
    static int checkIntervalCount = WHISPER_SAMPLE_RATE;

    auto floatData = (float*)data;
    for (unsigned int i = 0 ; i < len/sizeof(float) ; i++) {
        m_buffer.append(floatData[i]);
        if (m_buffer.size() > 1 && (m_buffer.size() % checkIntervalCount) == 0) {
            handleNewData();
        }
    }

    return len;
}


// Future action plan
// run this in another thread
// create a thread pool of 1 so we queue in order automagically
// copying the buffer into the thread

void Processor::process()
{
    QElapsedTimer bench;
    qDebug() << "proces start";
    bench.start();

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    wparams.print_progress   = false;
    wparams.print_special    = false;
    wparams.print_realtime   = false;
    wparams.translate        = false;
    wparams.no_context       = true;
    wparams.single_segment   = true;
    wparams.max_tokens       = 32;
    wparams.language         = "en";
    wparams.n_threads        = QThread::idealThreadCount();
    // wparams.audio_ctx        = params.audio_ctx;
    wparams.speed_up         = true;

    // tokens from the last itteration
    // wparams.prompt_tokens    = params.no_context ? nullptr : prompt_tokens.data();
    // wparams.prompt_n_tokens  = params.no_context ? 0       : prompt_tokens.size();

    qDebug() << "processing " << m_buffer.duration() << "ms of audio";

    if (whisper_full(ctx, wparams, (const float*) m_buffer.data(), m_buffer.size()) != 0) {
        qApp->exit(6);
    }

    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
        const char * text = whisper_full_get_segment_text(ctx, i);

        // TODO be cleverer with newlines and such
        // maybe Processor should emit blobs of text + timestamps
        // then another class is responsible for building a text document
        m_text += text;

        Q_EMIT textChanged();
        //                      else {
        //                        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
        //                        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);

        //                        printf ("[%s --> %s]  %s\n", to_timestamp(t0).c_str(), to_timestamp(t1).c_str(), text);

        //                        if (params.fname_out.length() > 0) {
        //                            fout << "[" << to_timestamp(t0) << " --> " << to_timestamp(t1) << "]  " << text << std::endl;
        //                        }
    }

    m_buffer.clear();

    // maybe take the last N milliseconds and prepend to buffer

    qDebug() << "Done" << bench.elapsed();
}

QString Processor::text() const
{
    return m_text;
}
