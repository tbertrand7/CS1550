#!/usr/bin/make
# Tom Bertrand
# 2/2/2017
# COE 1550
# Misurda T, Th 11:00
# Project 1
# Graphics Library Makefile

LIB = library
MAIN = square
DRIVER = driver
GCC = gcc

install:
	$(GCC) -o $(LIB).o -c $(LIB).c
	$(GCC) -o $(MAIN).o -c $(MAIN).c
	$(GCC) -o $(DRIVER).o -c $(DRIVER).c
	$(GCC) -o $(MAIN) $(MAIN).o $(LIB).o
	$(GCC) -o $(DRIVER) $(DRIVER).o $(LIB).o

clean:
	rm -f $(LIB).o
	rm -f $(MAIN).o
	rm -f $(MAIN)
	rm -f $(DRIVER).o
	rm -f $(DRIVER)
