@echo off
rmdir /s /q build
set ONLOOKER_POLL_INTERVAL=10
Onlooker cmake -S llvm -B build --trace --trace-format=json-v1 --trace-redirect=trace.json