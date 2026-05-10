Set up
```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev libqt6sql6-sqlite
```

Build
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

Start
```bash
./GuitarHeroClone
```