#!/bin/bash
# AI Training Job Scheduler — Build & Run Script
set -e

cd "$(dirname "$0")"

echo ""
echo "  ╔══════════════════════════════════════╗"
echo "  ║   Neural Ops — AI Training Scheduler ║"
echo "  ║   OS Course Project — FAST-NU 2026   ║"
echo "  ╚══════════════════════════════════════╝"
echo ""

echo "  Building C++ backend..."
cd backend && make -s && cd ..
echo "  Build successful."
echo ""
echo "  Starting server → http://localhost:8080"
echo "  Press Ctrl+C to stop."
echo ""

./backend/scheduler
