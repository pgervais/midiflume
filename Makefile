CC=gcc

SRCS=dag.cc event_processors.cc lua_config.cc lua_util.cc
HDRS=dag.h event_processors.h lua_config.h lua_util.h

midiflume: midiflume.cc $(SRCS) $(HDRS)
	$(CC) -frtti -Wall -g -o midiflume midiflume.cc $(SRCS) -lasound -llua5.3 -lstdc++

dag_test: dag_test.cc $(SRCS) $(HDRS)
	$(CC) -frtti -Wall -g -o dag_test dag_test.cc $(SRCS) -lasound -lstdc++ -lm

clean:
	rm -f midiflume dag_test
