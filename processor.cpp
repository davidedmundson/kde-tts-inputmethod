#include "processor.h"

#include <QTimer>
#include <QThread>
#include <QDebug>
#include <QGuiApplication>

#include "whisper.cpp/whisper.h"

// Tuning
// from stream:
//int32_t step_ms    = 3000;
//int32_t length_ms  = 10000;
///    const int n_samples_keep = 0.2*WHISPER_SAMPLE_RATE; // amount to use between searches


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

    QTimer *t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &Processor::process);
    t->start(5000);
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

// game plan
// detect if there's any noise. Some sort of sliding window average?
// After a long pause, kick off a `process`

// copy command.cpp vad_simple

qint64 Processor::writeData(const char *data, qint64 len)
{
    // TODO resize + memcpy?
    auto floatData = (float*)data;
    for (unsigned int i = 0 ; i < len/sizeof(float) ; i++) {
        m_buffer.append(floatData[i]);
    }
    return len;
}

void Processor::process()
{
    QElapsedTimer bench;
    qDebug() << "proces start";
    bench.start();

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    wparams.print_progress   = true;
     wparams.print_special    = true;
    wparams.print_realtime   = false;
    // wparams.print_timestamps = !params.no_timestamps;
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

    qDebug() << "processing " << m_buffer.size()  / (float)WHISPER_SAMPLE_RATE << "seconds of audio";

    if (whisper_full(ctx, wparams, (const float*) m_buffer.data(), m_buffer.size()) != 0) {
        qApp->exit(6);
    }

    {
        printf("\33[2K\r");

        // print long empty line to clear the previous line
        printf("%s", std::string(100, ' ').c_str());

        printf("\33[2K\r");

        const int n_segments = whisper_full_n_segments(ctx);
        for (int i = 0; i < n_segments; ++i) {
            const char * text = whisper_full_get_segment_text(ctx, i);

            printf("%s\n", text);
            fflush(stdout);

            //                      else {
            //                        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
            //                        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);

            //                        printf ("[%s --> %s]  %s\n", to_timestamp(t0).c_str(), to_timestamp(t1).c_str(), text);

            //                        if (params.fname_out.length() > 0) {
            //                            fout << "[" << to_timestamp(t0) << " --> " << to_timestamp(t1) << "]  " << text << std::endl;
            //                        }
        }
    }
    m_buffer.clear();

    // maybe take the last N milliseconds and prepend to buffer

    qDebug() << "Done" << bench.elapsed();
}

// ++n_iter;
//
// if ((n_iter % n_new_line) == 0) {
//     printf("\n");
//
//     // keep part of the audio for next iteration to try to mitigate word boundary issues
//     pcmf32_old = std::vector<float>(pcmf32.end() - n_samples_keep, pcmf32.end());
//
//     // Add tokens of the last full length segment as the prompt
//     if (!params.no_context) {
//         prompt_tokens.clear();
//
//         const int n_segments = whisper_full_n_segments(ctx);
//         for (int i = 0; i < n_segments; ++i) {
//             const int token_count = whisper_full_n_tokens(ctx, i);
//             for (int j = 0; j < token_count; ++j) {
//                 prompt_tokens.push_back(whisper_full_get_token_id(ctx, i, j));
//             }
//         }
//     }
// }
