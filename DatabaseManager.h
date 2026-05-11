#pragma once
#include <QString>
#include <vector>

struct ScoreRecord {
  int id;
  QString name;
  QString song;
  QString difficulty;
  int score;
  QString date;
};

struct SongRecord {
  int id;
  QString name;
  QString filepath;
};

class DatabaseManager {
public:
  static DatabaseManager &instance();
  bool init();
  void addScore(const QString &name, const QString &song, const QString &difficulty, int score);
  std::vector<ScoreRecord> getTopScores(int limit = 50);
  void deleteScore(int id);
  void addSong(const QString &name, const QString &filepath);
  std::vector<SongRecord> getSongs();
  void deleteSong(int id);

private:
  DatabaseManager() = default;
};
