#pragma once
#include "DatabaseManager.h"
#include "GameEngine.h"
#include "GameView.h"
#include "SettingsManager.h"
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>

class KeyBindButton : public QPushButton {
public:
  int index;
  Qt::Key currentKey;
  KeyBindButton(int idx, Qt::Key k, QWidget *parent = nullptr)
      : QPushButton(QKeySequence(k).toString(), parent), index(idx),
        currentKey(k) {}

protected:
  void keyPressEvent(QKeyEvent *e) override {
    currentKey = static_cast<Qt::Key>(e->key());
    setText(QKeySequence(currentKey).toString());
    clearFocus();
  }
};

enum class GameState {
  Intro = 0,
  Menu,
  SongSelect,
  Settings,
  Leaderboard,
  Gameplay,
  GameOver
};

class GameController : public QMainWindow {
  Q_OBJECT
public:
  GameController(QWidget *parent = nullptr);

protected:
  void keyPressEvent(QKeyEvent *event) override;

private:
  void skipIntro();
  int m_introPressCount = 0;

  QMediaPlayer *m_introPlayer;
  QAudioOutput *m_introAudioOutput;
  QVideoWidget *m_videoWidget;

  QStackedWidget *m_stack;
  GameEngine *m_engine;
  GameView *m_gameView;

  QListWidget *listSongs;
  QTableWidget *tableLeaderboard;
  QLineEdit *searchSongs;
  QLabel *lblFinalScore;
  QLineEdit *editPlayerName;

  QString selectedSongPath;
  QString selectedSongName;
  std::vector<KeyBindButton *> keyButtons;

  void applyDarkTheme();
  void initIntro();
  void initMenu();
  void initSongSelect();
  void filterSongs(const QString &query);
  void refreshSongList();
  void initSettings();
  void initLeaderboard();
  void refreshLeaderboard();
  void initGame();
  void initGameOver();
  void onGameOver(int finalScore);
};
