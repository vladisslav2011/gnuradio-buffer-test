LD_FLAGS=-lgnuradio-runtime -lgnuradio-blocks -lgnuradio-pmt -lfmt -llog4cpp

main: main.cpp
	g++ -DGNURADIO_VER=`gnuradio-config-info --version|sed -e 's/[^0-9]//g'|cut -b1-2` -o main main.cpp $(LD_FLAGS)
clean:
	rm -f main
