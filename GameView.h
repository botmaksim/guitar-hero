#pragma once
#include "GameEngine.h"
#include "SettingsManager.h"
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QWidget>

struct HitEffect {
  int lane;
  double radius;
  double alpha;
};

class GameView : public QWidget {
  Q_OBJECT
public:
  explicit GameView(GameEngine *engine, QWidget *parent = nullptr)
      : QWidget(parent), m_engine(engine) {

    QColor colors[] = {Qt::red,     Qt::green, Qt::cyan,    Qt::yellow,
                       Qt::magenta, Qt::blue,  Qt::darkRed, Qt::darkGreen};
    for (int i = 0; i < 8; ++i)
      m_noteSprites.push_back(generateNoteSprite(colors[i]));

    connect(m_engine, &GameEngine::stateUpdated, this, [this]() {
      for (auto &effect : m_effects) {
        effect.radius += 3.0;
        effect.alpha -= 15.0;
      }
      m_effects.erase(
          std::remove_if(m_effects.begin(), m_effects.end(),
                         [](const HitEffect &e) { return e.alpha <= 0; }),
          m_effects.end());
      update();
    });
  }

signals:
  void requestExit();

protected:
  QPointF getPerspectivePoint(double laneRatio, double engineY) {
    double topY = height() * 0.2;
    double bottomY = height() * 0.9;
    double topWidth = width() * 0.3;
    double bottomWidth = width() * 0.8;

    double t = engineY / 1000.0;
    double curvedT = t * t;

    double screenY = topY + curvedT * (bottomY - topY);
    double currentWidth = topWidth + curvedT * (bottomWidth - topWidth);
    double startX = (width() - currentWidth) / 2.0;

    return QPointF(startX + laneRatio * currentWidth, screenY);
  }

  void paintEvent(QPaintEvent *event) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    double uiScale = qMax(1.0, qMin(width() / 1100.0, height() / 750.0));

    QString bgPath = SettingsManager::instance().getBgImagePath();
    if (!bgPath.isEmpty()) {
      QPixmap bg(bgPath);
      painter.drawPixmap(rect(),
                         bg.scaled(size(), Qt::KeepAspectRatioByExpanding));
      painter.fillRect(rect(), QColor(10, 0, 20, 180));
    } else {
      painter.fillRect(rect(), QColor(10, 10, 15));
    }

    int lanes = m_engine->laneCount;
    const std::vector<Qt::Key>& currentKeys = m_engine->activeKeys;

    painter.setPen(QPen(QColor(0, 255, 255, 150), 2 * uiScale));
    for (int i = 0; i <= lanes; ++i) {
      double ratio = static_cast<double>(i) / lanes;
      QPointF topP = getPerspectivePoint(ratio, 0);
      QPointF botP = getPerspectivePoint(ratio, 1000);
      painter.drawLine(topP, botP);
    }

    painter.setPen(QPen(QColor(255, 0, 255, 80), 2 * uiScale));
    double timeOffset =
        fmod(m_engine->activeNotes.size() > 0 ? m_engine->activeNotes[0].y : 0,
             200.0);
    for (int i = 0; i < 6; i++) {
      double y = i * 200 - timeOffset;
      if (y < 0)
        y += 1200;
      QPointF left = getPerspectivePoint(0, y);
      QPointF right = getPerspectivePoint(1, y);
      painter.drawLine(left, right);
    }

    for (int i = 0; i < lanes; ++i) {
      double centerRatio = (i + 0.5) / lanes;
      QPointF targetPos =
          getPerspectivePoint(centerRatio, m_engine->hitZoneCenter);

      double targetWidth = 80 * uiScale;
      double targetHeight = 25 * uiScale;

      painter.setBrush(QColor(10, 10, 10, 200));
      painter.setPen(QPen(QColor(204, 255, 0, 200), 2 * uiScale));
      painter.drawRoundedRect(targetPos.x() - targetWidth / 2, targetPos.y() - targetHeight / 2, targetWidth, targetHeight, 8 * uiScale, 8 * uiScale);

      if (i < currentKeys.size()) {
        QString keyName = QKeySequence(currentKeys[i]).toString();
        painter.setPen(Qt::white);
        painter.setFont(QFont("Consolas", 14 * uiScale, QFont::Bold));
        painter.drawText(QRectF(targetPos.x() - 20 * uiScale, targetPos.y() - 15 * uiScale, 40 * uiScale, 30 * uiScale),
                         Qt::AlignCenter, keyName);
      }
    }

    for (const auto &note : m_engine->activeNotes) {
      if (!note.active)
        continue;
      double centerRatio = (note.lane + 0.5) / lanes;
      double leftRatio = static_cast<double>(note.lane) / lanes + 0.05;
      double rightRatio = static_cast<double>(note.lane + 1) / lanes - 0.05;

      if (note.length > 0) {
        double tailY = std::max(0.0, note.y - note.length);
        QPointF botLeft = getPerspectivePoint(leftRatio, note.y);
        QPointF botRight = getPerspectivePoint(rightRatio, note.y);
        QPointF topLeft = getPerspectivePoint(leftRatio, tailY);
        QPointF topRight = getPerspectivePoint(rightRatio, tailY);

        QPainterPath tailPath;
        tailPath.moveTo(botLeft);
        tailPath.lineTo(botRight);
        tailPath.lineTo(topRight);
        tailPath.lineTo(topLeft);
        tailPath.closeSubpath();

        QColor tailColor = m_noteSprites[note.lane % m_noteSprites.size()]
                               .toImage()
                               .pixelColor(m_noteSprites[0].width() / 2, m_noteSprites[0].height() / 2);
        tailColor.setAlpha(note.isHeld ? 220 : 120); 
        painter.fillPath(tailPath, tailColor);
      }

      QPointF notePos = getPerspectivePoint(centerRatio, note.y);
      double depthScale = 0.4 + (note.y / 1000.0) * 0.8;
      double finalScale = depthScale * uiScale;
      QPixmap scaledSprite =
          m_noteSprites[note.lane % m_noteSprites.size()].scaled(
              100 * finalScale, 40 * finalScale, Qt::IgnoreAspectRatio,
              Qt::SmoothTransformation);

      painter.drawPixmap(notePos.x() - scaledSprite.width() / 2.0,
                         notePos.y() - scaledSprite.height() / 2.0,
                         scaledSprite);
    }

    for (const auto &effect : m_effects) {
      double centerRatio = (effect.lane + 0.5) / lanes;
      QPointF targetPos =
          getPerspectivePoint(centerRatio, m_engine->hitZoneCenter);
      painter.setPen(QPen(QColor(204, 255, 0, effect.alpha), 4 * uiScale));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(targetPos, effect.radius * uiScale,
                          effect.radius * 0.6 * uiScale);
    }

    painter.setFont(QFont("Consolas", 18 * uiScale, QFont::Bold));

    painter.setBrush(QColor(10, 10, 12, 220));
    painter.setPen(QPen(QColor(204, 255, 0), 2 * uiScale));
    painter.drawRect(10 * uiScale, 10 * uiScale, 240 * uiScale, 90 * uiScale);

    painter.setPen(QColor(255, 255, 255));
    painter.drawText(20 * uiScale, 40 * uiScale, QString("SCORE"));

    painter.setFont(QFont("Consolas", 26 * uiScale, QFont::Bold));
    painter.setPen(QColor(204, 255, 0));
    painter.drawText(
        20 * uiScale, 80 * uiScale, QString("%1").arg(m_engine->currentScore, 7, 10, QChar('0')));

    QFont titleFont("Consolas", 16 * uiScale, QFont::Bold);
    painter.setFont(titleFont);
    painter.setPen(QColor(204, 255, 0));
    
    QRect titleRect(width() - 620 * uiScale, 20 * uiScale, 600 * uiScale, 60 * uiScale);
    QString title = m_engine->currentSongName.toUpper();
    QFontMetrics fm(titleFont);
    QString elidedTitle = fm.elidedText(title, Qt::ElideRight, titleRect.width());
    
    painter.drawText(titleRect, Qt::AlignRight | Qt::AlignTop | Qt::TextDontClip, elidedTitle);

    if (m_engine->comboMultiplier > 1) {
      painter.setFont(QFont("Consolas", 28 * uiScale, QFont::Bold));
      painter.setPen(QColor(0, 255, 255));
      painter.drawText(20 * uiScale, 150 * uiScale, QString("x%1").arg(m_engine->comboMultiplier));
    }
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->isAutoRepeat())
      return;
    if (event->key() == Qt::Key_Escape) {
      emit requestExit();
      return;
    }

    const std::vector<Qt::Key>& currentKeys = m_engine->activeKeys;
    for (int i = 0; i < currentKeys.size(); ++i) {
      if (event->key() == currentKeys[i]) {
        auto [hit, y] = m_engine->checkHit(i);
        if (hit) {
          m_effects.push_back({i, 35.0, 255.0});
        }
        break;
      }
    }
  }

  void keyReleaseEvent(QKeyEvent *event) override {
    if (event->isAutoRepeat())
      return;
    const std::vector<Qt::Key>& currentKeys = m_engine->activeKeys;
    for (int i = 0; i < currentKeys.size(); ++i) {
      if (event->key() == currentKeys[i]) {
        m_engine->releaseHit(i);
        break;
      }
    }
  }

private:
  GameEngine *m_engine;
  std::vector<QPixmap> m_noteSprites;
  std::vector<HitEffect> m_effects;

  QPixmap generateNoteSprite(QColor color) {
    int w = 100;
    int h = 40;
    QPixmap pix(w, h);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    QColor glowColor = color;
    glowColor.setAlpha(80);
    p.setBrush(glowColor);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, w, h, 20, 20);

    QLinearGradient grad(0, 0, w, 0);
    grad.setColorAt(0, color.darker(150));
    grad.setColorAt(0.5, color.lighter(120));
    grad.setColorAt(1, color.darker(150));
    p.setBrush(grad);
    p.setPen(QPen(Qt::white, 2));
    p.drawRoundedRect(10, 5, w - 20, h - 10, 10, 10);

    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(20, 12, w - 40, 6, 3, 3);

    return pix;
  }
};