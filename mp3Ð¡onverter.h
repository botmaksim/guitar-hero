#pragma once
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <algorithm>
#include <cmath>

struct DifficultyParams {
  double noteProbability;
  double thresholdMult;
  int minIntervalMs;
};

class ChartGenerator : public QObject {
  Q_OBJECT
public:
  explicit ChartGenerator(QObject *parent = nullptr) : QObject(parent) {
    m_settings[1] = {0.3, 1.5, 500};
    m_settings[2] = {0.6, 1.2, 300};
    m_settings[3] = {0.9, 0.9, 150};

    connect(&m_decoder, &QAudioDecoder::bufferReady, this,
            &ChartGenerator::processBuffer);
    connect(&m_decoder, &QAudioDecoder::finished, this,
            &ChartGenerator::onDecodingFinished);
  }

  void generate(const QString &filePath, int lanes, int difficulty,
                bool longNotes) {
    m_filePath = filePath;
    m_lanes = lanes;
    m_difficulty = std::clamp(difficulty, 1, 3);
    m_useLongNotes = longNotes;
    m_pcmData.clear();

    m_decoder.setSource(QUrl::fromLocalFile(filePath));
    m_decoder.start();
  }

signals:
  void generationFinished(QJsonDocument json);

private slots:
  void processBuffer() {
    QAudioBuffer buffer = m_decoder.read();
    const float *data = buffer.constData<float>();
    int count = buffer.sampleCount();

    for (int i = 0; i < count; ++i) {
      m_pcmData.append(std::abs(data[i]));
    }
  }

  void onDecodingFinished() {
    DifficultyParams params = m_settings[m_difficulty];
    QJsonArray notesArray;

    int windowSize = 882;
    double lastNoteTime = -params.minIntervalMs;

    double avgEnergy = 0;
    for (float val : m_pcmData)
      avgEnergy += val;
    avgEnergy /= (m_pcmData.size() > 0 ? m_pcmData.size() : 1);

    for (int i = 0; i < m_pcmData.size() - windowSize; i += windowSize) {
      double windowEnergy = 0;
      for (int j = 0; j < windowSize; ++j)
        windowEnergy += m_pcmData[i + j];
      windowEnergy /= windowSize;

      double currentTimeMs = (static_cast<double>(i) / 44100.0) * 1000.0;

      if (windowEnergy > (avgEnergy * params.thresholdMult)) {
        if (currentTimeMs - lastNoteTime >= params.minIntervalMs) {

          if ((rand() % 100) / 100.0 <= params.noteProbability) {
            QJsonObject note;
            note["time"] = static_cast<int>(currentTimeMs);
            note["lane"] = rand() % m_lanes;

            if (m_useLongNotes && (rand() % 10 == 0)) {
              note["type"] = "long";
              note["duration"] = 500 + (rand() % 1000);
            } else {
              note["type"] = "short";
              note["duration"] = 0;
            }

            notesArray.append(note);
            lastNoteTime = currentTimeMs;
          }
        }
      }
    }

    QJsonObject root;
    root["song_name"] = QFileInfo(m_filePath).baseName();
    root["difficulty"] = m_difficulty;
    root["notes"] = notesArray;

    emit generationFinished(QJsonDocument(root));
  }

private:
  QAudioDecoder m_decoder;
  QVector<float> m_pcmData;
  QMap<int, DifficultyParams> m_settings;

  QString m_filePath;
  int m_lanes;
  int m_difficulty;
  bool m_useLongNotes;
};