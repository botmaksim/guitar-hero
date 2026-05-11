cmake_minimum_required(VERSION 3.16)
project(GuitarHeroClone VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Добавлен MultimediaWidgets
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Multimedia MultimediaWidgets Concurrent)

add_executable(GuitarHeroClone
    main.cpp
    DatabaseManager.h
    SettingsManager.h
    GameEngine.h
    GameView.h
    GameController.h
    GameController.cpp
    DatabaseManager.cpp
    SettingsManager.cpp
    GameEngine.cpp
    GameView.cpp
    mp3Converter.h
    mp3Converter.cpp
)

# force cmake rebuild

target_link_libraries(GuitarHeroClone PRIVATE 
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Multimedia Qt6::MultimediaWidgets Qt6::Concurrent
)