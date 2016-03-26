SHELL = /bin/sh

CXX = g++
CXXFLAGS = -g -Wall 
#INCLUDE = -I/usr/include/boost-1_33_1
INCLUDE = 
CXXSRCS = Covariance.C  MAX.C  SUB.C  Normal.C  \
	RandomVariable.C  ADD.C  Util.C Gate.C \
	Parser.C Ssta.C Expression.C main.C
#CXXSRCS =  test.C Expression.C
OBJS = $(CXXSRCS:.C=.o) 
DEPS = $(CXXSRCS:.C=.d) 
TARGET = nhssta

$(TARGET) : $(OBJS) 
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $(TARGET) $(OBJS)

%.o : %.C
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<

%.d : %.C
	rm -f $@
	$(CXX) -MM $(CXXFLAGS) $(INCLUDE) $< | sed "s/\($*\)\.o[ :]*/\1.o $@ : /g" > $@

clean :
	rm -f $(OBJS) $(DEPS) $(TARGET)

tags :
	etags *.h *.C

-include $(DEPS)



