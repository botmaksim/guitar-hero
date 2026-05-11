#include "mp3Converter.h"
#include <QCoreApplication>
#include <QtConcurrent>
#include <QFutureWatcher>

ChartGenerator::ChartGenerator(QObject *parent) : QObject(parent) {
  m_settings[1] = {1.0, 1.6, 650};
  m_settings[2] = {1.0, 1.4, 450};
  m_settings[3] = {1.0, 1.2, 250};

  m_decoder = new QAudioDecoder(this);

  connect(m_decoder, &QAudioDecoder::bufferReady, this,
          &ChartGenerator::processBuffer);
  connect(m_decoder, &QAudioDecoder::finished, this,
          &ChartGenerator::onDecodingFinished);
  connect(m_decoder,
          QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error), this,
          [this](QAudioDecoder::Error err) {
            qWarning() << "Audio decoder error:" << err;
            onDecodingFinished();
          });
  connect(m_decoder, &QAudioDecoder::positionChanged, this,
          [this](qint64 pos) {
            qint64 dur = m_decoder->duration();
            if (dur > 0) {
              int percent = std::min(99, static_cast<int>((pos * 100) / dur));
              emit progressChanged(percent);
            } else {
              int percent = std::min(99, static_cast<int>((pos / 3000) % 100));
              emit progressChanged(percent);
            }
          });
}

void ChartGenerator::generate(const QString &filePath, int lanes,
                              int difficulty, bool longNotes) {
  m_isFinished = false;
  m_filePath = filePath;
  m_lanes = lanes;
  m_difficulty = std::clamp(difficulty, 1, 3);
  m_useLongNotes = longNotes;
  m_pcmData.clear();
  m_sampleRate = 44100;
  m_channels = 2;
  // Estimate for a 5-minute song at 44100Hz, 2 channels to avoid reallocations
  m_pcmData.reserve(44100 * 2 * 60 * 5);

  m_decoder->stop();
  m_decoder->setAudioFormat(QAudioFormat());
  m_decoder->setSource(QUrl::fromLocalFile(filePath));
  m_decoder->start();
}

void ChartGenerator::processBuffer() {
  while (true) {
    QAudioBuffer buffer = m_decoder->read();
    if (!buffer.isValid())
      break;

    if (buffer.format().sampleRate() > 0) {
      m_sampleRate = buffer.format().sampleRate();
      m_channels = buffer.format().channelCount();
    }

    int count = buffer.sampleCount();
    auto format = buffer.format().sampleFormat();

    int currentSize = m_pcmData.size();
    m_pcmData.resize(currentSize + count);
    float* dest = m_pcmData.data() + currentSize;

    if (format == QAudioFormat::Float) {
      const float *data = buffer.constData<float>();
      for (int i = 0; i < count; ++i)
        dest[i] = std::abs(data[i]);
    } else if (format == QAudioFormat::Int16) {
      const qint16 *data = buffer.constData<qint16>();
      for (int i = 0; i < count; ++i)
        dest[i] = std::abs(data[i]) / 32768.0f;
    } else if (format == QAudioFormat::Int32) {
      const qint32 *data = buffer.constData<qint32>();
      for (int i = 0; i < count; ++i)
        dest[i] = std::abs(data[i]) / 2147483648.0f;
    } else if (format == QAudioFormat::UInt8) {
      const quint8 *data = buffer.constData<quint8>();
      for (int i = 0; i < count; ++i)
        dest[i] = std::abs(data[i] - 128) / 128.0f;
    }
  }
}

void ChartGenerator::onDecodingFinished() {
  if (m_isFinished)
    return;
  m_isFinished = true;
  m_decoder->stop();
  
  // Create copies of the necessary data for the background thread
  QVector<float> pcmData = m_pcmData;
  DifficultyParams params = m_settings[m_difficulty];
  int sampleRate = m_sampleRate;
  int channels = m_channels;
  int lanes = m_lanes;
  bool useLongNotes = m_useLongNotes;
  QString filePath = m_filePath;
  int difficulty = m_difficulty;

  // Run the heavy processing in a separate thread
  // Using QTimer::singleShot to decouple if needed but QtConcurrent is better
  QFutureWatcher<QJsonDocument>* watcher = new QFutureWatcher<QJsonDocument>(this);
  connect(watcher, &QFutureWatcher<QJsonDocument>::finished, this, [this, watcher]() {
      emit progressChanged(100);
      emit generationFinished(watcher->result());
      watcher->deleteLater();
  });
  
  QFuture<QJsonDocument> future = QtConcurrent::run([=]() -> QJsonDocument {
      QJsonArray notesArray;

      int windowSize = static_cast<int>(sampleRate * channels * 0.02);
      if (windowSize <= 0)
        windowSize = 882;

      double lastNoteTime = -params.minIntervalMs;
      std::vector<double> laneFreeTime(lanes, -params.minIntervalMs);

      double avgEnergy = 0;
      for (float val : pcmData)
        avgEnergy += val;
      avgEnergy /= (pcmData.size() > 0 ? pcmData.size() : 1);

      std::vector<double> windowEnergies;
      int totalSteps = (pcmData.size() - windowSize > 0)
                           ? (pcmData.size() - windowSize) / windowSize
                           : 1;

      windowEnergies.reserve(totalSteps);

      for (int i = 0; i < pcmData.size() - windowSize; i += windowSize) {
        double windowEnergy = 0;
        for (int j = 0; j < windowSize; ++j)
          windowEnergy += pcmData[i + j];
        windowEnergy /= windowSize;
        windowEnergies.push_back(windowEnergy);
      }

      int totalWindows = windowEnergies.size();
      for (size_t k = 1; k < totalWindows - 1; ++k) {
        double currentTimeMs =
            ((k * windowSize) / (double)(sampleRate * channels)) * 1000.0;

        bool isLocalPeak = (windowEnergies[k] > windowEnergies[k - 1]) &&
                           (windowEnergies[k] > windowEnergies[k + 1]);

        if (isLocalPeak && windowEnergies[k] > (avgEnergy * params.thresholdMult)) {
          if (currentTimeMs - lastNoteTime >= params.minIntervalMs) {

            if ((rand() % 100) / 100.0 <= params.noteProbability) {
              int lane = rand() % lanes;

              if (currentTimeMs < laneFreeTime[lane]) {
                int originalLane = lane;
                bool found = false;
                for (int l = 1; l < lanes; ++l) {
                  int testLane = (originalLane + l) % lanes;
                  if (currentTimeMs >= laneFreeTime[testLane]) {
                    lane = testLane;
                    found = true;
                    break;
                  }
                }
                if (!found)
                  continue;
              }

              QJsonObject note;
              note["time"] = static_cast<int>(currentTimeMs);
              note["lane"] = lane;

              if (useLongNotes && (rand() % 10 == 0)) {
                note["type"] = "long";
                int duration = 200 + (rand() % 400);
                note["duration"] = duration;
                laneFreeTime[lane] =
                    currentTimeMs + duration + params.minIntervalMs;
              } else {
                note["type"] = "short";
                note["duration"] = 0;
                laneFreeTime[lane] = currentTimeMs + params.minIntervalMs;
              }

              notesArray.append(note);
              lastNoteTime = currentTimeMs;
            }
          }
        }
      }

      QJsonObject root;
      root["song_name"] = QFileInfo(filePath).baseName();
      root["difficulty"] = difficulty;
      root["notes"] = notesArray;

      return QJsonDocument(root);
  });
  
  watcher->setFuture(future);
}
