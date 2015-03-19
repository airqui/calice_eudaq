all: caliceProducer caliceDataCollector caliceMonitor

CPPFLAGS = -g -Wall -Wno-deprecated -I../eudaq/main/include -I../lcio/v02-04-03/include -std=c++11 -I$(shell root-config --incdir)
LIBS = -lpthread -lstdc++ -L../eudaq/lib -lEUDAQ -L ../lcio/v02-04-03/lib -llcio $(shell root-config --libs)

caliceProducer: CaliceReceiver.o SiReader.o ScReader.o
	gcc -g -Wall $(LIBS) -o caliceProducer CaliceReceiver.o SiReader.o ScReader.o

caliceDataCollector: CaliceDataCollector.o CaliceGenericConverterPlugin.o FileWriterLCIOC.o
	gcc -g -Wall $(LIBS) -o caliceDataCollector CaliceDataCollector.o CaliceGenericConverterPlugin.o FileWriterLCIOC.o
#caliceDataCollector: CaliceDataCollector.o CaliceGenericConverterPlugin.o
#	gcc -g -Wall -lpthread -lstdc++ -L../eudaq/lib -lEUDAQ -L ../lcio/v02-04-03/lib -llcio -o caliceDataCollector CaliceDataCollector.o CaliceGenericConverterPlugin.o
#caliceMonitor: moniq/CaliceMonitor.o CaliceGenericConverterPlugin.o
#	gcc -g -Wall $(LIBS) -o caliceMonitor moniq/CaliceMonitor.o CaliceGenericConverterPlugin.o

.cc.o	:
	g++ $(CPPFLAGS) -c $< -o $@

%.o : %.h

clean :
	rm -f *.o *~ \#*  caliceProducer caliceDataCollector caliceMonitor
