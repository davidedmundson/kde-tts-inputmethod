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
    Processor p;

    QAudioDecoder audioDecoder;
    audioDecoder.setAudioFormat(Processor::requiredFormat());
    audioDecoder.setSourceFilename("/home/david/projects/personal/audiosource/whisper.cpp/samples/jfk.wav");
    qDebug() << audioDecoder.audioFormat();

    QObject::connect(&audioDecoder, &QAudioDecoder::stateChanged, [&app](QAudioDecoder::State state) {
        qDebug() << state;
    });

    // TODO read snippet at a time to make it realtime
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

    audioDecoder.start();
    app.exec();
}
