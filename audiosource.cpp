#include "audiosource.h"

#include <stdlib.h>
#include <math.h>

#include <QDateTime>
#include <QDebug>
#include <QPainter>
#include <QVBoxLayout>
#include <QAudioDeviceInfo>
#include <QAudioInput>
#include <QTimer>
#include <qendian.h>

#include <QGuiApplication>

#include "whisper.cpp/whisper.h"

// TODO
// Speech to qDebug
// Form sentences
// Populate a TextField directly, editing the contents at runtime
// wayland integration
// move TextInferrer to thread(?)
// QtQuick UI with volume indication
// Model downloader
// prepare for shipping

class TextInferrer : public QIODevice
{
    Q_OBJECT
public:
    TextInferrer();

    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    void process();
private:
//    QByteArray m_buffer;
    QVector<float> m_buffer;
    struct whisper_context * ctx;
};


InputTest::InputTest()
{
    m_textInferrer = new TextInferrer;
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

qreal InputTest::calculateLevel(const char *data, qint64 len) const
{
//    const int channelBytes = m_format.bytesPerFrame();
//    const int sampleBytes = m_format.bytesPerFrame();
//    const int numSamples = len / sampleBytes;

//    float maxValue = 0;
//    auto *ptr = reinterpret_cast<const unsigned char *>(data);

//    for (int i = 0; i < numSamples; ++i) {
//        for (int j = 0; j < m_format.channelCount(); ++j) {
//            float value = data[ptr];

//            maxValue = qMax(value, maxValue);
//            ptr += channelBytes;
//        }
//    }
//    return maxValue;
    return 0;
}


void InputTest::initializeAudio(const QAudioDeviceInfo &deviceInfo)
{
    qDebug() << "DAVE222!";

    m_format.setSampleRate(WHISPER_SAMPLE_RATE);
    m_format.setChannelCount(1);
    m_format.setSampleSize(32);
    m_format.setSampleType(QAudioFormat::Float);
    m_format.setByteOrder(QAudioFormat::LittleEndian);
//    m_format.setCodec("audio/pcm");

    if (!deviceInfo.isFormatSupported(m_format)) {
        qWarning() << "Default format not supported - trying to use nearest";
        m_format = deviceInfo.nearestFormat(m_format);
    }

    m_audioInput.reset(new QAudioInput(deviceInfo, m_format));

    qDebug() << m_audioInput->volume();

    connect(m_audioInput.data(), &QAudioInput::stateChanged, this, [this]() {
        qDebug() << "State changed" << m_audioInput->state();
    });

    m_audioInput->start(m_textInferrer);
}

void InputTest::deviceChanged(int index)
{
    initializeAudio(m_deviceBox->itemData(index).value<QAudioDeviceInfo>());
}

TextInferrer::TextInferrer()
{

    //whisper stuff
    QByteArray model = "models/ggml-small.en.bin";
    // TODO verify file exists
    // TODO language
    // TODO download the right model automatically
    ctx = whisper_init(model.data());

    open(QIODevice::WriteOnly);
    QTimer *t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &TextInferrer::process);
    t->start(5000);
    t->setSingleShot(true);
}

qint64 TextInferrer::readData(char *, qint64 )
{
    return 0;
}

qint64 TextInferrer::writeData(const char *data, qint64 len)
{
    // TODO resize + memcpy?
    auto floatData = (float*)data;
    for (int i = 0 ; i < len/sizeof(float) ; i++) {
        m_buffer.append(floatData[i]);
    }
    return len;
}

void TextInferrer::process()
{
    // pad to 30 seconds
    int emptyBufferSize = 30 * WHISPER_SAMPLE_RATE - m_buffer.size();
    if (emptyBufferSize > 0) {
        QVector<float> emptyBuffer(emptyBufferSize, 0);
        m_buffer.append(emptyBuffer);
    }

    qDebug() << "process " << m_buffer.size();
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

     wparams.print_progress   = true;
    // wparams.print_special    = params.print_special;
     wparams.print_realtime   = false;
    // wparams.print_timestamps = !params.no_timestamps;
    // wparams.translate        = params.translate;
    wparams.no_context       = true;
    wparams.single_segment   = true;
//    wparams.max_tokens       = params.max_tokens;
    // wparams.language         = params.language.c_str();
    // wparams.n_threads        = params.n_threads;
    //
    // wparams.audio_ctx        = params.audio_ctx;
    // wparams.speed_up         = params.speed_up;

    // wparams.prompt_tokens    = params.no_context ? nullptr : prompt_tokens.data();
    // wparams.prompt_n_tokens  = params.no_context ? 0       : prompt_tokens.size();

    if (whisper_full(ctx, wparams, (const float*) m_buffer.data(), m_buffer.size()) != 0) {
        qApp->exit(6);
    }

    // print result;
    {
        printf("\33[2K\r");

        // print long empty line to clear the previous line
        printf("%s", std::string(100, ' ').c_str());

        printf("\33[2K\r");

        const int n_segments = whisper_full_n_segments(ctx);
        for (int i = 0; i < n_segments; ++i) {
            const char * text = whisper_full_get_segment_text(ctx, i);

            //                    if (params.no_timestamps) {
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
    qDebug() << "Done";
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


#include "audiosource.moc"
