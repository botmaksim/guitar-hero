#include "DatabaseManager.h"
#include "GameController.h"
#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  if (!DatabaseManager::instance().init()) {
    QMessageBox::critical(nullptr, "Ошибка БД",
                          "Не удалось инициализировать базу данных SQLite.");
    return -1;
  }

  GameController window;
  window.setWindowTitle("Guitar Hero Clone Pro");
  window.show();

  return app.exec();
}