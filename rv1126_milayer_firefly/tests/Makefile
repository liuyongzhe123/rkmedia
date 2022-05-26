CC = /home/lyz/fouraibox/rv1126_rv1109_linux_release_20211022/prebuilts/gcc/linux-x86/arm/gcc-arm-8.3-2019.03-x86_64-arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++
INCFLAGS = -I../src
LIBS := -L../milayer_dev
CPPFLAGS=-lmilayer -leasymedia -lpthread

# SRCS := $(wildcard *.cpp) 
# #OBJS := $(patsubst %.cpp,%.o,$(SRCS))
# OBJS :=$(SRCS:.cpp=.o)                                                      

#api测试
ceshi: ceshi.o
	$(CC) -o $@ $^ $(INCFLAGS) $(LIBS) $(CPPFLAGS)
ceshi.o:ceshi.cpp
	$(CC) -o $@ -c $^ $(INCFLAGS) $(LIBS) $(CPPFLAGS)

#4路解码+4路编码
ceshi_mul: ceshi_mul.o
	$(CC) -o $@ $^ $(INCFLAGS) $(LIBS) $(CPPFLAGS)
 .o:ceshi_mul.cpp
	$(CC) -o $@ -c $^ $(INCFLAGS) $(LIBS) $(CPPFLAGS)

#4路解码
ceshi_savejpg: ceshi_savejpg.o
	$(CC) -o $@ $^ $(INCFLAGS) $(LIBS) $(CPPFLAGS)
ceshi_savejpg.o:ceshi_savejpg.cpp
	$(CC) -o $@ -c $^ $(INCFLAGS) $(LIBS) $(CPPFLAGS)	

clean:
	rm -rf *.o ceshi_savejpg ceshi_mul ceshi
		