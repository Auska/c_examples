# Makefile for C++23 project

CXX ?= g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O2
LDFLAGS = 

TARGETS = folder_similarity oldsort extract_name

.PHONY: all clean $(TARGETS)

all: $(TARGETS)

folder_similarity: folder_similarity.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

oldsort: oldsort.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

extract_name: extract_name.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)