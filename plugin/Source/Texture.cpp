#include "Texture.h"
#include "BinaryData.h"

void Texture::loadEmbeddedSample()
{
    juce::AudioFormatManager fmt;
    fmt.registerBasicFormats();

    auto* stream = new juce::MemoryInputStream (BinaryData::dulcimer_wav,
                                                BinaryData::dulcimer_wavSize, false);
    std::unique_ptr<juce::AudioFormatReader> reader (fmt.createReaderFor (
        std::unique_ptr<juce::InputStream> (stream)));
    if (reader == nullptr) return;

    sampleLength      = (int) juce::jmin ((juce::int64) kMaxSampleLen, reader->lengthInSamples);
    sourceSampleRate  = reader->sampleRate;
    sampleData.assign ((size_t) sampleLength, 0.0f);

    juce::AudioBuffer<float> tmp ((int) reader->numChannels, sampleLength);
    reader->read (&tmp, 0, sampleLength, 0, true, reader->numChannels > 1);

    // Mono-mix if stereo (shouldn't be — embedded sample is mono).
    const float* ch0 = tmp.getReadPointer (0);
    const float* ch1 = (reader->numChannels > 1) ? tmp.getReadPointer (1) : ch0;
    for (int i = 0; i < sampleLength; ++i)
        sampleData[(size_t) i] = (ch0[i] + ch1[i]) * 0.5f;

    sampleLoaded = true;
}
