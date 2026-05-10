#pragma once
#include "SettingsManager.h"
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>
#include <QTimer>
#include <QRandomGenerator>

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

  explicit GameEngine(QObject *parent = nullptr) : QObject(parent) {
    physicsTimer = new QTimer(this);
    connect(physicsTimer, &QTimer::timeout, this, &GameEngine::updatePhysics);
    spawnTimer = new QTimer(this);
    connect(spawnTimer, &QTimer::timeout, this, &GameEngine::spawnNote);

    player = new QMediaPlayer(this);
    audioOutput = new QAudioOutput(this);
    player->setAudioOutput(audioOutput);

    connect(player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
              if (status == QMediaPlayer::EndOfMedia && isPlaying) {
                endGame();
              }
            });
  }

  void startGame(const QString &songName, const QString &songPath) {
    laneCount = SettingsManager::instance().getLaneCount();
    activeKeys = SettingsManager::instance().getActiveKeys(laneCount);
    Difficulty diff = SettingsManager::instance().getDifficulty();

    if (diff == Difficulty::Easy) {
      fallSpeed = 4.0;
      spawnInterval = 1000;
    } else if (diff == Difficulty::Medium) {
      fallSpeed = 6.0;
      spawnInterval = 600;
    } else {
      fallSpeed = 8.5;
      spawnInterval = 350;
    }

    currentSongName = songName;
    currentScore = 0;
    comboMultiplier = 1;
    activeNotes.clear();
    isPlaying = true;

    if (!songPath.isEmpty()) {
      player->setSource(QUrl::fromLocalFile(songPath));
      audioOutput->setVolume(SettingsManager::instance().getVolume() / 100.0);
      player->play();
    }

    physicsTimer->start(16);
    spawnTimer->start(spawnInterval);
  }

  void stopGame() {
    isPlaying = false;
    physicsTimer->stop();
    spawnTimer->stop();
    player->stop();
  }

  void endGame() {
    if (!isPlaying)
      return;
    isPlaying = false;
    physicsTimer->stop();
    spawnTimer->stop();
    player->stop();
    emit gameOver(currentScore);
  }

  std::pair<bool, double> checkHit(int lane) {
    for (auto &note : activeNotes) {
      if (note.active && note.lane == lane && !note.isHeld) {
        if (std::abs(note.y - hitZoneCenter) <= hitTolerance) {
          currentScore += 10 * comboMultiplier;
          if (note.length > 0) {
            note.isHeld = true;
          } else {
            note.active = false;
            comboMultiplier++;
          }
          return {true, hitZoneCenter};
        }
      }
    }
    comboMultiplier = 1;
    return {false, 0.0};
  }

  void releaseHit(int lane) {
    for (auto &note : activeNotes) {
      if (note.active && note.lane == lane && note.isHeld) {
        note.isHeld = false;
        if (note.y - note.length < hitZoneCenter - hitTolerance) {
          note.active = false;
          comboMultiplier = 1;
        } else {
          note.active = false;
          comboMultiplier++;
        }
        break;
      }
    }
  }

signals:
  void stateUpdated();
  void gameOver(int finalScore);

private slots:
  void spawnNote() {
    if (!isPlaying)
      return;

    int lane = QRandomGenerator::global()->bounded(laneCount);

    double safeDistance = 150.0;
    for (const auto &n : activeNotes) {
      if (n.lane == lane) {
        double noteEnd = n.y - n.length;
        if (noteEnd < safeDistance)
          return;
      }
    }

    bool isLong = (QRandomGenerator::global()->bounded(5) == 0);
    double length = isLong ? (300.0 + QRandomGenerator::global()->bounded(300)) : 0.0;
    activeNotes.push_back({lane, -50.0, length, true, false});
  }

  void updatePhysics() {
    for (auto &note : activeNotes) {
      if (!note.active)
        continue;

      note.y += fallSpeed;

      if (note.isHeld) {
        currentScore += 1;
        if (note.y - note.length >= hitZoneCenter) {
          note.active = false;
          comboMultiplier++;
        }
      } else {
        double checkY = (note.length > 0) ? (note.y - note.length) : note.y;
        if (checkY > hitZoneCenter + hitTolerance) {
          note.active = false;
          comboMultiplier = 1;
        }
      }
    }
    activeNotes.erase(std::remove_if(activeNotes.begin(), activeNotes.end(),
                                     [](const Note &n) { return !n.active; }),
                      activeNotes.end());
    emit stateUpdated();
  }

private:
  QTimer *physicsTimer;
  QTimer *spawnTimer;
  QMediaPlayer *player;
  QAudioOutput *audioOutput;
  bool isPlaying = false;
  int spawnInterval = 600;
};