CXX      = g++
CXXFLAGS = -std=c++17 -pthread -O2 -Wall -Wextra -I.
TARGET   = scheduler

HEADERS  = include/Globals.h      \
           include/Semaphore.h    \
           include/Logger.h       \
           include/Job.h          \
           include/ResourceTable.h \
           include/Scheduler.h    \
           include/JsonHelpers.h  \
           include/ApiHandlers.h  \
           include/HttpServer.h

$(TARGET): main.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) main.cpp

clean:
	rm -f $(TARGET) scheduler.log

.PHONY: clean
