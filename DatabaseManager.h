#pragma once
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>
#include <vector>
#include <QDateTime>

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
  static DatabaseManager &instance() {
    static DatabaseManager instance;
    return instance;
  }

  bool init() {
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
      return true;
    }
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("guitar_hero.db");

    if (!db.open()) {
      qWarning() << "Database error:" << db.lastError().text();
      return false;
    }

    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS leaderboard_v3 ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "player_name TEXT, song TEXT, difficulty TEXT, score INTEGER, date TEXT)");

    query.exec("CREATE TABLE IF NOT EXISTS songs ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "name TEXT, filepath TEXT)");
    return true;
  }

  void addScore(const QString &name, const QString &song, const QString &difficulty, int score) {
    QSqlQuery query;
    query.prepare(
        "INSERT INTO leaderboard_v3 (player_name, song, difficulty, score, date) VALUES (:name, :song, :diff, :score, :date)");
    query.bindValue(":name", name.isEmpty() ? "NoName" : name);
    query.bindValue(":song", song.isEmpty() ? "Unknown" : song);
    query.bindValue(":diff", difficulty);
    query.bindValue(":score", score);
    
    QDateTime now = QDateTime::currentDateTimeUtc().addSecs(3 * 3600);
    query.bindValue(":date", now.toString("yyyy-MM-dd HH:mm:ss"));
    query.exec();
  }

  std::vector<ScoreRecord> getTopScores(int limit = 50) {
    std::vector<ScoreRecord> records;
    QSqlQuery query;
    query.prepare("SELECT id, player_name, song, difficulty, score, date FROM leaderboard_v3 ORDER "
                  "BY score DESC LIMIT :limit");
    query.bindValue(":limit", limit);
    if (query.exec()) {
      while (query.next()) {
        records.push_back({query.value(0).toInt(), query.value(1).toString(),
                           query.value(2).toString(), query.value(3).toString(), 
                           query.value(4).toInt(), query.value(5).toString()});
      }
    }
    return records;
  }

  void deleteScore(int id) {
    QSqlQuery query;
    query.prepare("DELETE FROM leaderboard_v3 WHERE id = :id");
    query.bindValue(":id", id);
    query.exec();
  }

  void addSong(const QString &name, const QString &filepath) {
    QSqlQuery query;
    query.prepare(
        "INSERT INTO songs (name, filepath) VALUES (:name, :filepath)");
    query.bindValue(":name", name);
    query.bindValue(":filepath", filepath);
    query.exec();
  }

  std::vector<SongRecord> getSongs() {
    std::vector<SongRecord> songs;
    QSqlQuery query;
    if (query.exec("SELECT id, name, filepath FROM songs")) {
      while (query.next()) {
        songs.push_back({query.value(0).toInt(), query.value(1).toString(),
                         query.value(2).toString()});
      }
    }
    return songs;
  }

  void deleteSong(int id) {
    QSqlQuery query;
    query.prepare("DELETE FROM songs WHERE id = :id");
    query.bindValue(":id", id);
    query.exec();
  }

private:
  DatabaseManager() = default;
};