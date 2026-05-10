#pragma once
#include <QSettings>
#include <QString>
#include <vector>

enum class Difficulty { Easy, Medium, Hard };

class SettingsManager {
public:
  static SettingsManager &instance() {
    static SettingsManager instance;
    return instance;
  }

  int getVolume() const { return m_settings.value("volume", 50).toInt(); }
  void setVolume(int v) { m_settings.setValue("volume", v); }

  int getLaneCount() const { return m_settings.value("laneCount", 4).toInt(); }
  void setLaneCount(int c) { m_settings.setValue("laneCount", c); }

  Difficulty getDifficulty() const {
    return static_cast<Difficulty>(m_settings.value("difficulty", 1).toInt());
  }
  void setDifficulty(Difficulty d) {
    m_settings.setValue("difficulty", static_cast<int>(d));
  }

  QString getIntroVideoPath() const {
    return m_settings.value("introVideo", "").toString();
  }
  void setIntroVideoPath(const QString &path) {
    m_settings.setValue("introVideo", path);
  }
  QString getBgImagePath() const {
    return m_settings.value("bgImage", "").toString();
  }
  void setBgImagePath(const QString &path) {
    m_settings.setValue("bgImage", path);
  }
  void setKeys(const std::vector<Qt::Key> &keys) {
    QVariantList list;
    for (auto k : keys) {
      list.append(static_cast<int>(k));
    }
    m_settings.setValue("keys", list);
  }

  std::vector<Qt::Key> getAllKeys() const {
    std::vector<Qt::Key> defaultKeys = {Qt::Key_A, Qt::Key_S,        Qt::Key_D,
                                        Qt::Key_F, Qt::Key_J,        Qt::Key_K,
                                        Qt::Key_L, Qt::Key_Semicolon};
    QVariantList list = m_settings.value("keys").toList();
    if (list.isEmpty() || list.size() != 8)
      return defaultKeys;
    std::vector<Qt::Key> keys;
    for (const QVariant &v : list)
      keys.push_back(static_cast<Qt::Key>(v.toInt()));
    return keys;
  }

  std::vector<Qt::Key> getActiveKeys(int laneCount) const {
    auto all = getAllKeys();
    std::vector<Qt::Key> active;
    int startIdx = 4 - (laneCount / 2);
    for (int i = 0; i < laneCount; ++i)
      active.push_back(all[startIdx + i]);
    return active;
  }

private:
  SettingsManager() : m_settings("MyCompany", "GuitarHeroClonePro") {}
  QSettings m_settings;
};