PROG = http_upload
SOURCES = $(PROG).c ../external/mongoose/mongoose.c ../external/inih/ini.c
CFLAGS = -g -W -Wall  -I../external/mongoose -I../external/inih -Wno-unused-function $(CFLAGS_EXTRA) -DMG_ENABLE_HTTP_STREAMING_MULTIPART

CC = gcc

$(PROG): $(SOURCES)
	$(CC) $(SOURCES) -o out/$@ $(CFLAGS)

clean:
	rm -rf *.gc* *.dSYM *.exe *.obj *.o a.out $(PROG)
