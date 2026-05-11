#include "GameEngine.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <algorithm>
#include <QUrl>

GameEngine::GameEngine(QObject *parent) : QObject(parent) {
  physicsTimer = new QTimer(this);
  connect(physicsTimer, &QTimer::timeout, this, &GameEngine::updatePhysics);
  spawnTimer = new QTimer(this);
  connect(spawnTimer, &QTimer::timeout, this, &GameEngine::spawnNote);

  player = new QMediaPlayer(this);
  audioOutput = new QAudioOutput(this);
  player->setAudioOutput(audioOutput);

  connect(player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
    if (state == QMediaPlayer::PlayingState && isWaitingForMedia) {
      finishStartPlayback();
    }
  });

  connect(player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::EndOfMedia && isPlaying) {
      endGame();
    } else if (status == QMediaPlayer::InvalidMedia && isWaitingForMedia) {
      qWarning() << "Invalid media detected by QMediaPlayer!";
      finishStartPlayback();
    }
  });

  connect(player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &errorString) {
    qWarning() << "QMediaPlayer error:" << errorString;
    if (isWaitingForMedia) finishStartPlayback();
  });
}

void GameEngine::startGame(const QString &songName, const QString &songPath) {
  laneCount = SettingsManager::instance().getLaneCount();
  activeKeys = SettingsManager::instance().getActiveKeys(laneCount);
  Difficulty diff = SettingsManager::instance().getDifficulty();
  isExperimental = SettingsManager::instance().isExperimentalMode();
  bool noLongNotes = SettingsManager::instance().isNoLongNotes();

  if (diff == Difficulty::Easy) {
    fallSpeed = 4.0; spawnInterval = 1000;
  } else if (diff == Difficulty::Medium) {
    fallSpeed = 6.0; spawnInterval = 600;
  } else {
    fallSpeed = 8.5; spawnInterval = 350;
  }

  msToHit = ((hitZoneCenter + 50.0) / fallSpeed) * 16.0;

  if (m_currentGenerator) {
    m_currentGenerator->disconnect(this);
    m_currentGenerator->deleteLater();
    m_currentGenerator = nullptr;
  }

  currentSongName = songName;
  currentScore = 0;
  comboMultiplier = 1;
  activeNotes.clear();
  activeNotes.reserve(100);
  scheduledNotes.clear();
  scheduledNotes.reserve(5000);
  nextScheduledNoteIdx = 0;
  isPlaying = false;

  if (isExperimental && !songPath.isEmpty()) {
    isGenerating = true;
    generationProgress = 0;
    
    m_currentGenerator = new ChartGenerator(this);

    connect(m_currentGenerator, &ChartGenerator::progressChanged, this, [this](int percent) {
      generationProgress = percent;
      emit stateUpdated();
    });

    connect(m_currentGenerator, &ChartGenerator::generationFinished, this, [this, songPath](QJsonDocument json) {
      QJsonArray notes = json.object()["notes"].toArray();
      for (int i = 0; i < notes.size(); ++i) {
        QJsonObject n = notes[i].toObject();
        scheduledNotes.push_back({n["time"].toInt(), n["lane"].toInt(), n["duration"].toDouble(), false});
      }
      std::sort(scheduledNotes.begin(), scheduledNotes.end(), [](const ScheduledNote &a, const ScheduledNote &b) {
        return a.timeMs < b.timeMs;
      });
      isGenerating = false;
      startPlayback(songPath);
      
      if (m_currentGenerator) {
        m_currentGenerator->deleteLater();
        m_currentGenerator = nullptr;
      }
    });

    int diffInt = static_cast<int>(diff) + 1;
    m_currentGenerator->generate(songPath, laneCount, diffInt, !noLongNotes);
    emit stateUpdated();
  } else {
    startPlayback(songPath);
  }
}

void GameEngine::startPlayback(const QString &songPath) {
  isPlaying = true;
  isGenerating = false;
  generationProgress = 100;
  lastUpdateMs = 0;

  if (!songPath.isEmpty()) {
    isWaitingForMedia = true;
    isPlaying = true;
    isGenerating = false;
    player->setSource(QUrl::fromLocalFile(songPath));
    audioOutput->setVolume(SettingsManager::instance().getVolume() / 100.0);
    player->play();
    emit stateUpdated();
  } else {
    finishStartPlayback();
  }
}

void GameEngine::finishStartPlayback() {
  isPlaying = true;
  isGenerating = false;
  isWaitingForMedia = false;
  gameTimer.start();
  lastUpdateMs = gameTimer.elapsed();
  physicsTimer->start(16);
  if (!isExperimental) spawnTimer->start(spawnInterval);
  emit stateUpdated();
}

void GameEngine::stopGame() {
  isPlaying = false;
  physicsTimer->stop();
  spawnTimer->stop();
  player->stop();
  if (m_currentGenerator) {
    m_currentGenerator->disconnect(this);
    m_currentGenerator->deleteLater();
    m_currentGenerator = nullptr;
  }
}

void GameEngine::endGame() {
  if (!isPlaying) return;
  isPlaying = false;
  physicsTimer->stop();
  spawnTimer->stop();
  player->stop();
  if (m_currentGenerator) {
    m_currentGenerator->disconnect(this);
    m_currentGenerator->deleteLater();
    m_currentGenerator = nullptr;
  }
  emit gameOver(currentScore);
}

std::pair<bool, double> GameEngine::checkHit(int lane) {
  for (auto &note : activeNotes) {
    if (note.active && note.lane == lane && !note.isHeld) {
      if (std::abs(note.y - hitZoneCenter) <= hitTolerance) {
        currentScore += 10 * comboMultiplier;
        if (note.length > 0) note.isHeld = true;
        else { note.active = false; comboMultiplier++; }
        return {true, hitZoneCenter};
      }
    }
  }
  comboMultiplier = 1;
  return {false, 0.0};
}

void GameEngine::releaseHit(int lane) {
  for (auto &note : activeNotes) {
    if (note.active && note.lane == lane && note.isHeld) {
      note.isHeld = false;
      if (note.y - note.length < hitZoneCenter - hitTolerance) {
        note.active = false; comboMultiplier = 1;
      } else {
        note.active = false; comboMultiplier++;
      }
      break;
    }
  }
}

void GameEngine::spawnNote() {
  if (!isPlaying || isExperimental) return;
  int lane = QRandomGenerator::global()->bounded(laneCount);
  double safeDistance = 150.0;
  for (const auto &n : activeNotes) {
    if (n.lane == lane) {
      double noteEnd = n.y - n.length;
      if (noteEnd < safeDistance) return;
    }
  }

  bool noLongNotes = SettingsManager::instance().isNoLongNotes();
  bool isLong = (!noLongNotes && QRandomGenerator::global()->bounded(5) == 0);
  double length = isLong ? (300.0 + QRandomGenerator::global()->bounded(300)) : 0.0;
  activeNotes.push_back({lane, -50.0, length, true, false});
}

void GameEngine::updatePhysics() {
  qint64 currentMs = gameTimer.elapsed();
  double deltaMs = static_cast<double>(currentMs - lastUpdateMs);
  if (deltaMs < 0) deltaMs = 0;
  lastUpdateMs = currentMs;

  double currentFallStep = fallSpeed * (deltaMs / 16.0);

  if (isPlaying && isExperimental) {
    while (nextScheduledNoteIdx < scheduledNotes.size()) {
      auto &sn = scheduledNotes[nextScheduledNoteIdx];
      if (currentMs >= sn.timeMs - msToHit) {
        sn.spawned = true;
        double lagMs = currentMs - (sn.timeMs - msToHit);
        double startY = -50.0 + (lagMs / 16.0) * fallSpeed;
        double lengthDist = (sn.duration / 16.0) * fallSpeed;
        activeNotes.push_back({sn.lane, startY, lengthDist, true, false});
        nextScheduledNoteIdx++;
      } else {
        break;
      }
    }
  }

  for (auto &note : activeNotes) {
    if (!note.active) continue;

    note.y += currentFallStep;

    if (note.isHeld) {
      holdScoreAccumulator += 1.0 * (deltaMs / 16.0);
      if (holdScoreAccumulator >= 1.0) {
        int add = static_cast<int>(holdScoreAccumulator);
        currentScore += add;
        holdScoreAccumulator -= add;
      }
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
  activeNotes.erase(std::remove_if(activeNotes.begin(), activeNotes.end(), [](const Note &n) { return !n.active; }), activeNotes.end());
  emit stateUpdated();
}
