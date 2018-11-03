# CSE531_Project
Combination of Lab 2 (simple file locking service) and Raft to create a fault tolerant simple file locking service

## Build/Run Environment
Tested on 64bit Debian 9 (stretch)

## Debian Packages (similarly named for other distros)
- gcc 6.3.0-4
- scons 2.5.1-1
- protobuf-compiler 1.2.1-2
- libprotobuf-dev 3.0.0-9
- libcrypto++-dev 5.6.4-7

## Installation
```
git clone git://github.com/SPARCpenguin/CSE531_Project
cd CSE531_Project
git submodule update --recursive --init
```
## Build

### Logcabin
```
cd logcabin
scons
```
### simpleFileLockService
```
cd simpleFileLockService
make clean
make all
```

## Test
