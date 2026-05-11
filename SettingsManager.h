#pragma once
#include <QSettings>
#include <QString>
#include <vector>

enum class Difficulty { Easy, Medium, Hard };

class SettingsManager {
public:
  static SettingsManager &instance();

  int getVolume() const;
  void setVolume(int v);

  int getLaneCount() const;
  void setLaneCount(int c);

  Difficulty getDifficulty() const;
  void setDifficulty(Difficulty d);

  QString getIntroVideoPath() const;
  void setIntroVideoPath(const QString &path);
  QString getBgImagePath() const;
  void setBgImagePath(const QString &path);
  void setKeys(const std::vector<Qt::Key> &keys);

  bool isNoLongNotes() const;
  void setNoLongNotes(bool v);

  bool isExperimentalMode() const;
  void setExperimentalMode(bool v);

  std::vector<Qt::Key> getAllKeys() const;
  std::vector<Qt::Key> getActiveKeys(int laneCount) const;

private:
  SettingsManager();
  QSettings m_settings;
};
