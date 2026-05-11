#pragma once
#include "SettingsManager.h"
#include "mp3Converter.h"
#include <QAudioOutput>
#include <QElapsedTimer>
#include <QMediaPlayer>
#include <QObject>
#include <QTimer>
#include <vector>
#include <utility>

struct Note {
  int lane;
  double y;
  double length;
  bool active;
  bool isHeld;
};

class GameEngine : public QObject {
  Q_OBJECT
public:
  int currentScore = 0;
  int comboMultiplier = 1;
  int laneCount = 4;
  double fallSpeed = 5.0;
  double hitZoneCenter = 900.0;
  double hitTolerance = 80.0;
  QString currentSongName;
  std::vector<Note> activeNotes;
  std::vector<Qt::Key> activeKeys;
  bool isExperimental = false;
  QElapsedTimer gameTimer;
  bool isGenerating = false;
  int generationProgress = 0;
  qint64 lastUpdateMs = 0;
  double holdScoreAccumulator = 0.0;

  struct ScheduledNote {
    int timeMs;
    int lane;
    double duration;
    bool spawned;
  };
  std::vector<ScheduledNote> scheduledNotes;
  double msToHit = 0.0;
  bool isWaitingForMedia = false;
  size_t nextScheduledNoteIdx = 0;
  ChartGenerator* m_currentGenerator = nullptr;

  explicit GameEngine(QObject *parent = nullptr);
  void startGame(const QString &songName, const QString &songPath);
  void startPlayback(const QString &songPath);
  void finishStartPlayback();
  void stopGame();
  void endGame();
  std::pair<bool, double> checkHit(int lane);
  void releaseHit(int lane);

signals:
  void stateUpdated();
  void gameOver(int finalScore);

private slots:
  void spawnNote();
  void updatePhysics();

private:
  QTimer *physicsTimer;
  QTimer *spawnTimer;
  QMediaPlayer *player;
  QAudioOutput *audioOutput;
  bool isPlaying = false;
  int spawnInterval = 600;
};
