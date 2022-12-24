#include <QAudioDecoderControl>
#include <QGuiApplication>
#include <QDebug>

#include "processor.h"

/**
 * This program takes a .wav file and runs it through the processor
 */
int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    AudioProcessor p;

    QAudioDecoder audioDecoder;
    audioDecoder.setAudioFormat(AudioProcessor::requiredFormat());
    audioDecoder.setSourceFilename("/home/david/projects/personal/audiosource/whisper.cpp/samples/jfk.wav");
    qDebug() << audioDecoder.audioFormat();

    QObject::connect(&audioDecoder, &QAudioDecoder::stateChanged, [&p, &app](QAudioDecoder::State state) {
        if (state == QAudioDecoder::StoppedState) {
            // write 8 seconds of silence so we keep processing
            QByteArray silenceBuffer(8 * 16000 * 2, 0);
            p.write(silenceBuffer.data(), silenceBuffer.count());
        }
    });

    QObject::connect(&audioDecoder, &QAudioDecoder::bufferReady, [&p, &audioDecoder]() {
        while(audioDecoder.bufferAvailable()) {

            // setAudioFomat is a lie! we need to manually change type to F32
            // don't know why.
            // If this test starts changing, get rid of it
            auto buffer = audioDecoder.read();
            uint16_t* bufferData = (uint16_t*) buffer.constData();
            for (uint i = 0 ; i < buffer.byteCount() / sizeof(int16_t); i++) {
                int16_t value = bufferData[i];
                float floatValue = 0.5 + (float)value / INT16_MAX;
                p.write((char*)&floatValue, sizeof(float));
            }
        }
    });

    QObject::connect(&p, &AudioProcessor::textChanged, [&p]() {
        qDebug() << "Text out" << p.text();
    });

    audioDecoder.start();
    app.exec();
}
