bin/tractron.com: tractron.c network.c win.c Makefile
	cosmocc -o bin/tractron.com win.c network.c tractron.c -I. -Wall -mtiny -s

bin/tractron.exe: tractron.c network.c Makefile
	i586-pc-msdosdjgpp-gcc -o bin/dos.exe tractron.c -I. -Wall -Ofast -s
	${EXE2COFF_PATH} bin/dos.exe
	cat ${CWSDSTUB_PATH} bin/dos > bin/tractron.exe

dos: bin/tractron.exe

debug: tractron.c network.c win.c
	cosmocc -o bin/tractron.com tractron.c network.c win.c -I. -Wall --debug

#dos: tractron.c
#	wcl386 tractron.c -i=. -za99 -bt=dos -l=dos4g -fe=tractron.com

gcc: tractron.c network.c
	gcc -g -Wall -I. -o bin/tractron tractron.c network.c

clean:
	rm bin/*
