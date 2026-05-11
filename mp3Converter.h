#pragma once
#include <QAudioBuffer>
#include <QAudioDecoder>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QMap>
#include <QVector>
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
  explicit ChartGenerator(QObject *parent = nullptr);

  Q_INVOKABLE void generate(const QString &filePath, int lanes, int difficulty,
                bool longNotes);

signals:
  void generationFinished(QJsonDocument json);
  void progressChanged(int percent);

private slots:
  void processBuffer();
  void onDecodingFinished();

private:
  QAudioDecoder *m_decoder;
  QVector<float> m_pcmData;
  QMap<int, DifficultyParams> m_settings;

  int m_sampleRate = 44100;
  int m_channels = 2;

  QString m_filePath;
  int m_lanes;
  int m_difficulty;
  bool m_useLongNotes;
  bool m_isFinished = false;
};