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
  GameController(QWidget *parent = nullptr) : QMainWindow(parent) {
    resize(1100, 750);
    applyDarkTheme();

    m_stack = new QStackedWidget(this);
    setCentralWidget(m_stack);
    m_engine = new GameEngine(this);

    initIntro();
    initMenu();
    initSongSelect();
    initSettings();
    initLeaderboard();
    initGame();
    initGameOver();

    connect(m_engine, &GameEngine::gameOver, this, &GameController::onGameOver);

    if (SettingsManager::instance().getIntroVideoPath().isEmpty()) {
      m_stack->setCurrentIndex(static_cast<int>(GameState::Menu));
    } else {
      m_stack->setCurrentIndex(static_cast<int>(GameState::Intro));
    }
  }

protected:
  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_F11) {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
        return;
    }

    if (m_stack->currentIndex() == static_cast<int>(GameState::Intro)) {
      m_introPressCount++;

      if (m_introPressCount >= 2) {
        skipIntro();
      }
      return;
    }

    if (event->key() == Qt::Key_Escape &&
        m_stack->currentIndex() == static_cast<int>(GameState::Gameplay)) {
      m_engine->stopGame();
      m_stack->setCurrentIndex(static_cast<int>(GameState::Menu));
    }

    QMainWindow::keyPressEvent(event);
  }

private:
  void skipIntro() {
    if (m_introPlayer) {
      m_introPlayer->stop();
    }
    m_introPressCount = 0;
    m_stack->setCurrentIndex(static_cast<int>(GameState::Menu));
  }
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

  void applyDarkTheme() {
    QApplication::setStyle("Fusion");
    setStyleSheet(R"(
        QMainWindow, QWidget { 
            background-color: #121217; 
            color: #CCFF00; 
            font-family: 'Consolas', 'Courier New', monospace; 
        }
        QPushButton { 
            background-color: #242430; 
            color: #CCFF00; 
            border: 1px solid #333344;
            padding: 10px; 
            font-weight: bold; 
            font-size: 14px; 
        }
        QPushButton:hover { 
            background-color: #333344; 
            border: 1px solid #CCFF00;
        }
        QTableWidget {
            background-color: #0A0A0C;
            color: #FFFFFF;
            gridline-color: #333344;
            border: 1px solid #333344;
        }
        QHeaderView::section {
            background-color: #121217;
            color: #CCFF00;
            padding: 5px;
            border: none;
            border-bottom: 2px solid #CCFF00;
        }
        QListWidget {
            background-color: #0A0A0C;
            color: #FFFFFF;
            border: 1px solid #333344;
        }
        QListWidget::item {
            padding: 10px;
        }
        QListWidget::item:selected {
            background-color: #CCFF00;
            color: #000000;
        }
        QLineEdit, QComboBox, QSpinBox, QSlider {
            background-color: #242430;
            color: #CCFF00;
            border: 1px solid #333344;
            padding: 5px;
        }
    )");
  }

  void initIntro() {
    QWidget *w = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);

    m_videoWidget = new QVideoWidget();

    m_introPlayer = new QMediaPlayer(this);
    m_introAudioOutput = new QAudioOutput(this);

    m_introPlayer->setAudioOutput(m_introAudioOutput);
    m_introPlayer->setVideoOutput(m_videoWidget);

    connect(m_introPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            skipIntro();
        }
    });

    m_introAudioOutput->setVolume(SettingsManager::instance().getVolume() / 100.0);

    QString videoPath = SettingsManager::instance().getIntroVideoPath();
    if (!videoPath.isEmpty()) {
      m_introPlayer->setSource(QUrl::fromLocalFile(videoPath));
      m_introPlayer->play();
    }

    QLabel *skipHint =
        new QLabel("Нажмите 2 раза, чтобы пропустить");
    skipHint->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    skipHint->setStyleSheet("color: rgba(255, 255, 255, 180); font-size: 16px; background: transparent;");
    
    QVBoxLayout *hintLayout = new QVBoxLayout(m_videoWidget);
    hintLayout->addStretch();
    hintLayout->addWidget(skipHint);
    hintLayout->setContentsMargins(0, 0, 0, 20);

    m_stack->insertWidget(static_cast<int>(GameState::Intro), m_videoWidget);
  }

  void initMenu() {
    QWidget *w = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(w);
    mainLayout->setAlignment(Qt::AlignCenter);

    QWidget *box = new QWidget();
    box->setFixedSize(500, 400);
    box->setStyleSheet("background-color: #0A0A0C; border-radius: 4px; border: 1px solid #111;");
    QVBoxLayout *boxLayout = new QVBoxLayout(box);
    boxLayout->setAlignment(Qt::AlignCenter);

    QLabel *title = new QLabel("GUITAR HERO // CORE");
    title->setFont(QFont("Consolas", 28, QFont::Bold));
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("color: #CCFF00; background: transparent; border: none; margin-bottom: 5px;");

    QLabel *subTitle = new QLabel("MADE BY mr.Business");
    subTitle->setFont(QFont("Consolas", 10));
    subTitle->setAlignment(Qt::AlignCenter);
    subTitle->setStyleSheet("color: #555566; background: transparent; border: none; letter-spacing: 3px; margin-bottom: 30px;");

    QPushButton *btnPlay = new QPushButton("> START_PROTOCOL");
    QPushButton *btnLeaderboard = new QPushButton("> ACCESS_RECORDS");
    QPushButton *btnSettings = new QPushButton("> CONFIG_SYSTEM");
    QPushButton *btnQuit = new QPushButton("> TERMINATE");

    auto setupMenuBtn = [](QPushButton* btn) {
        btn->setStyleSheet(R"(
            QPushButton {
                background-color: #242430;
                color: #CCFF00;
                border: none;
                border-radius: 2px;
                padding: 15px;
                margin: 5px 30px;
                font-weight: bold;
                text-align: left;
                padding-left: 20px;
            }
            QPushButton:hover {
                background-color: #333344;
                border-left: 4px solid #CCFF00;
            }
        )");
        btn->setCursor(Qt::PointingHandCursor);
    };
    setupMenuBtn(btnPlay);
    setupMenuBtn(btnLeaderboard);
    setupMenuBtn(btnSettings);
    setupMenuBtn(btnQuit);

    boxLayout->addWidget(title);
    boxLayout->addWidget(subTitle);
    boxLayout->addWidget(btnPlay);
    boxLayout->addWidget(btnLeaderboard);
    boxLayout->addWidget(btnSettings);
    boxLayout->addWidget(btnQuit);

    mainLayout->addWidget(box);

    connect(btnPlay, &QPushButton::clicked, [this]() {
      refreshSongList();
      m_stack->setCurrentIndex(static_cast<int>(GameState::SongSelect));
    });
    connect(btnSettings, &QPushButton::clicked, [this]() {
      m_stack->setCurrentIndex(static_cast<int>(GameState::Settings));
    });
    connect(btnLeaderboard, &QPushButton::clicked, [this]() {
      refreshLeaderboard();
      m_stack->setCurrentIndex(static_cast<int>(GameState::Leaderboard));
    });
    connect(btnQuit, &QPushButton::clicked, qApp, &QCoreApplication::quit);

    m_stack->insertWidget(static_cast<int>(GameState::Menu), w);
  }

  void initSongSelect() {
    QWidget *w = new QWidget();
    QVBoxLayout *l = new QVBoxLayout(w);
    l->setContentsMargins(40, 40, 40, 40);

    QLabel *title = new QLabel("AUDIO // SELECTION");
    title->setStyleSheet("color: #CCFF00; font-weight: bold; font-size: 20px; margin-bottom: 10px;");

    searchSongs = new QLineEdit();
    searchSongs->setPlaceholderText("SEARCH TRACKS...");
    searchSongs->setStyleSheet("padding: 10px; font-size: 14px;");
    connect(searchSongs, &QLineEdit::textChanged, this, &GameController::filterSongs);

    listSongs = new QListWidget();
    
    QPushButton *btnAdd = new QPushButton("> ADD NEW TRACK");
    QPushButton *btnPlay = new QPushButton("> INITIALIZE PROTOCOL (PLAY)");
    btnPlay->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            color: #CCFF00;
            border: 1px solid #CCFF00;
            font-weight: bold;
            padding: 15px;
            font-size: 16px;
            margin-top: 10px;
        }
        QPushButton:hover {
            background-color: #CCFF00;
            color: #121217;
        }
    )");
    QPushButton *btnBack = new QPushButton("> GO BACK");

    l->addWidget(title);
    l->addWidget(searchSongs);
    l->addWidget(listSongs);
    l->addWidget(btnAdd);
    l->addWidget(btnPlay);
    l->addWidget(btnBack);

    connect(btnAdd, &QPushButton::clicked, [this]() {
      QString path = QFileDialog::getOpenFileName(this, "Выберите аудио", "",
                                                  "Audio (*.mp3 *.wav)");
      if (!path.isEmpty()) {
        bool ok;
        QString name = QInputDialog::getText(
            this, "Название трека",
            "Введите название песни (Исполнитель - Трек):", QLineEdit::Normal,
            QFileInfo(path).baseName(), &ok);
        if (ok && !name.isEmpty()) {
          DatabaseManager::instance().addSong(name, path);
          refreshSongList();
        }
      }
    });

    connect(btnPlay, &QPushButton::clicked, [this]() {
      auto item = listSongs->currentItem();
      if (!item)
        return;
      selectedSongName = item->text();
      selectedSongPath = item->data(Qt::UserRole + 1).toString();
      m_stack->setCurrentIndex(static_cast<int>(GameState::Gameplay));
      m_gameView->setFocus();
      m_engine->startGame(selectedSongName, selectedSongPath);
    });

    connect(btnBack, &QPushButton::clicked, [this]() {
      m_stack->setCurrentIndex(static_cast<int>(GameState::Menu));
    });
    m_stack->insertWidget(static_cast<int>(GameState::SongSelect), w);
  }

  void filterSongs(const QString &query) {
    for (int i = 0; i < listSongs->count(); ++i) {
        auto item = listSongs->item(i);
        item->setHidden(!item->text().contains(query, Qt::CaseInsensitive));
    }
  }

  void refreshSongList() {
    listSongs->clear();
    if (searchSongs) searchSongs->clear();
    auto songs = DatabaseManager::instance().getSongs();
    for (const auto &s : songs) {
      QListWidgetItem *item = new QListWidgetItem(s.name);
      item->setData(Qt::UserRole, s.id);
      item->setData(Qt::UserRole + 1, s.filepath);
      listSongs->addItem(item);
    }
  }

  std::vector<KeyBindButton *> keyButtons;

  void initSettings() {
    QWidget *w = new QWidget();
    QVBoxLayout *l = new QVBoxLayout(w);
    l->setContentsMargins(40, 40, 40, 40);

    QLabel *title = new QLabel("SYSTEM // CONFIGURATION");
    title->setStyleSheet("color: #CCFF00; font-weight: bold; font-size: 20px; margin-bottom: 20px;");
    l->addWidget(title);

    QSlider *sliderVol = new QSlider(Qt::Horizontal);
    sliderVol->setRange(0, 100);
    sliderVol->setValue(SettingsManager::instance().getVolume());
    connect(sliderVol, &QSlider::valueChanged,
            [](int v) { SettingsManager::instance().setVolume(v); });

    QSpinBox *spinLanes = new QSpinBox();
    spinLanes->setRange(1, 8);
    spinLanes->setValue(SettingsManager::instance().getLaneCount());
    connect(spinLanes, QOverload<int>::of(&QSpinBox::valueChanged),
            [](int v) { SettingsManager::instance().setLaneCount(v); });

    l->addWidget(new QLabel("MASTER_VOLUME_LEVEL:"));
    l->addWidget(sliderVol);
    l->addSpacing(15);
    l->addWidget(new QLabel("ACTIVE_LANES_COUNT (1-8):"));
    l->addWidget(spinLanes);
    l->addSpacing(15);

    l->addWidget(new QLabel("DIFFICULTY_MATRIX:"));
    QComboBox *comboDiff = new QComboBox();
    comboDiff->addItem("NOVICE (Easy)", static_cast<int>(Difficulty::Easy));
    comboDiff->addItem("OPERATIVE (Medium)", static_cast<int>(Difficulty::Medium));
    comboDiff->addItem("EXPERT (Hard)", static_cast<int>(Difficulty::Hard));

    int currentDiff = static_cast<int>(SettingsManager::instance().getDifficulty());
    comboDiff->setCurrentIndex(currentDiff);

    connect(comboDiff, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [](int index) {
              SettingsManager::instance().setDifficulty(
                  static_cast<Difficulty>(index));
            });

    l->addWidget(comboDiff);
    l->addSpacing(30);

    l->addWidget(new QLabel("KEY_BINDINGS (CLICK TO REASSIGN):"));
    QHBoxLayout *keysLayout = new QHBoxLayout();
    auto keys = SettingsManager::instance().getAllKeys();
    for (int i = 0; i < 8; ++i) {
      KeyBindButton *btn = new KeyBindButton(i, keys[i]);
      keyButtons.push_back(btn);
      keysLayout->addWidget(btn);
    }
    l->addLayout(keysLayout);
    l->addSpacing(30);

    QPushButton *btnVideo = new QPushButton("> SELECT BOOT VIDEO");
    connect(btnVideo, &QPushButton::clicked, [this]() {
      QString path = QFileDialog::getOpenFileName(this, "Видео", "",
                                                  "Video (*.mp4 *.avi)");
      if (!path.isEmpty())
        SettingsManager::instance().setIntroVideoPath(path);
    });

    QPushButton *btnBg = new QPushButton("> SELECT ENVIRONMENT BACKGROUND");
    connect(btnBg, &QPushButton::clicked, [this]() {
      QString path = QFileDialog::getOpenFileName(
          this, "Фон", "", "Images (*.png *.jpg *.jpeg)");
      if (!path.isEmpty())
        SettingsManager::instance().setBgImagePath(path);
    });

    QPushButton *btnFullscreen = new QPushButton("> TOGGLE FULLSCREEN (F11)");
    connect(btnFullscreen, &QPushButton::clicked, [this]() {
        if (isFullScreen()) showNormal();
        else showFullScreen();
    });

    l->addWidget(btnVideo);
    l->addWidget(btnBg);
    l->addWidget(btnFullscreen);

    QPushButton *btnBack = new QPushButton("> APPLY AND RETURN");
    btnBack->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            color: #CCFF00;
            border: 1px solid #CCFF00;
            font-weight: bold;
            padding: 15px;
            margin-top: 20px;
        }
        QPushButton:hover {
            background-color: #CCFF00;
            color: #121217;
        }
    )");
    l->addStretch();
    l->addWidget(btnBack);

    connect(btnBack, &QPushButton::clicked, [this]() {
      std::vector<Qt::Key> newKeys;
      for (auto btn : keyButtons)
        newKeys.push_back(btn->currentKey);
      SettingsManager::instance().setKeys(newKeys);
      m_stack->setCurrentIndex(static_cast<int>(GameState::Menu));
    });

    m_stack->insertWidget(static_cast<int>(GameState::Settings), w);
  }

  void initLeaderboard() {
    QWidget *w = new QWidget();
    QVBoxLayout *l = new QVBoxLayout(w);
    l->setContentsMargins(40, 40, 40, 40);

    QLabel *title = new QLabel("ARCHIVE // LEADERBOARD");
    title->setStyleSheet("color: #CCFF00; font-weight: bold; font-size: 20px; margin-bottom: 20px;");

    tableLeaderboard = new QTableWidget();
    tableLeaderboard->setColumnCount(5);
    tableLeaderboard->setHorizontalHeaderLabels({"PLAYER", "SCORE", "SONG", "DIFFICULTY", "LOG_DATE"});
    tableLeaderboard->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    tableLeaderboard->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableLeaderboard->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableLeaderboard->setSortingEnabled(true);
    tableLeaderboard->verticalHeader()->setVisible(false);

    QPushButton *btnBack = new QPushButton("> RETURN TO MAIN MENU");
    btnBack->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            color: #CCFF00;
            border: 1px solid #CCFF00;
            padding: 15px;
            margin-top: 15px;
        }
        QPushButton:hover {
            background-color: #CCFF00;
            color: #121217;
        }
    )");

    l->addWidget(title);
    l->addWidget(tableLeaderboard);
    l->addWidget(btnBack);

    connect(btnBack, &QPushButton::clicked, [this]() {
      m_stack->setCurrentIndex(static_cast<int>(GameState::Menu));
    });
    m_stack->insertWidget(static_cast<int>(GameState::Leaderboard), w);
  }

  void refreshLeaderboard() {
    tableLeaderboard->setSortingEnabled(false);
    tableLeaderboard->setRowCount(0);
    auto scores = DatabaseManager::instance().getTopScores();
    for (int i = 0; i < scores.size(); ++i) {
      const auto &s = scores[i];
      tableLeaderboard->insertRow(i);
      
      QTableWidgetItem *itemPlayer = new QTableWidgetItem(s.name);
      QTableWidgetItem *itemScore = new QTableWidgetItem();
      itemScore->setData(Qt::EditRole, s.score);
      QTableWidgetItem *itemSong = new QTableWidgetItem(s.song);
      QTableWidgetItem *itemDiff = new QTableWidgetItem(s.difficulty);
      QTableWidgetItem *itemDate = new QTableWidgetItem(s.date);

      itemPlayer->setTextAlignment(Qt::AlignCenter);
      itemScore->setTextAlignment(Qt::AlignCenter);
      itemSong->setTextAlignment(Qt::AlignCenter);
      itemDiff->setTextAlignment(Qt::AlignCenter);
      itemDate->setTextAlignment(Qt::AlignCenter);

      tableLeaderboard->setItem(i, 0, itemPlayer);
      tableLeaderboard->setItem(i, 1, itemScore);
      tableLeaderboard->setItem(i, 2, itemSong);
      tableLeaderboard->setItem(i, 3, itemDiff);
      tableLeaderboard->setItem(i, 4, itemDate);
    }
    tableLeaderboard->setSortingEnabled(true);
    tableLeaderboard->sortByColumn(1, Qt::DescendingOrder);
  }

  void initGame() {
    m_gameView = new GameView(m_engine, this);
    connect(m_gameView, &GameView::requestExit, [this]() {
      m_engine->stopGame();
      m_stack->setCurrentIndex(static_cast<int>(GameState::Menu));
    });
    m_stack->insertWidget(static_cast<int>(GameState::Gameplay), m_gameView);
  }

  void initGameOver() {
    QWidget *w = new QWidget();
    QVBoxLayout *l = new QVBoxLayout(w);
    l->setAlignment(Qt::AlignCenter);

    QWidget *box = new QWidget();
    box->setStyleSheet("background-color: #0A0A0C; border-radius: 4px; border: 1px solid #333;");
    box->setFixedSize(500, 350);
    QVBoxLayout *boxLayout = new QVBoxLayout(box);
    boxLayout->setAlignment(Qt::AlignCenter);

    QLabel *title = new QLabel("PROTOCOL TERMINATED");
    title->setFont(QFont("Consolas", 24, QFont::Bold));
    title->setStyleSheet("color: #CCFF00; background: transparent; border: none;");
    title->setAlignment(Qt::AlignCenter);

    lblFinalScore = new QLabel("SCORE: 0");
    lblFinalScore->setFont(QFont("Consolas", 20));
    lblFinalScore->setStyleSheet("color: #FFF; background: transparent; border: none;");
    lblFinalScore->setAlignment(Qt::AlignCenter);

    editPlayerName = new QLineEdit();
    editPlayerName->setPlaceholderText("ENTER_OPERATIVE_ID");
    editPlayerName->setAlignment(Qt::AlignCenter);
    editPlayerName->setStyleSheet("padding: 10px; font-size: 16px; margin: 10px 40px;");

    QPushButton *btnSave = new QPushButton("> SUBMIT RECORD");
    btnSave->setStyleSheet(R"(
        QPushButton {
            background-color: transparent;
            color: #CCFF00;
            border: 1px solid #CCFF00;
            font-weight: bold;
            padding: 15px;
            margin: 10px 40px;
        }
        QPushButton:hover {
            background-color: #CCFF00;
            color: #121217;
        }
    )");

    boxLayout->addWidget(title);
    boxLayout->addSpacing(10);
    boxLayout->addWidget(lblFinalScore);
    boxLayout->addSpacing(20);
    boxLayout->addWidget(editPlayerName);
    boxLayout->addWidget(btnSave);

    l->addWidget(box);

    connect(btnSave, &QPushButton::clicked, [this]() {
      QString diffStr = "NOVICE";
      Difficulty d = SettingsManager::instance().getDifficulty();
      if (d == Difficulty::Medium) diffStr = "OPERATIVE";
      else if (d == Difficulty::Hard) diffStr = "EXPERT";

      DatabaseManager::instance().addScore(editPlayerName->text(), selectedSongName, diffStr, m_engine->currentScore);
      m_stack->setCurrentIndex(static_cast<int>(GameState::Menu));
    });

    m_stack->insertWidget(static_cast<int>(GameState::GameOver), w);
  }

  void onGameOver(int finalScore) {
    lblFinalScore->setText(QString("SCORE: %1").arg(finalScore));
    editPlayerName->clear();
    m_stack->setCurrentIndex(static_cast<int>(GameState::GameOver));
  }
};
