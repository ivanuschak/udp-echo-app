#IDIR =../include
CC=gcc
#CFLAGS=-I$(IDIR)
CFLAGS = -Wall -pthread

#ODIR=obj
ODIR=.
#LDIR =../lib

LIBS=

#_DEPS = hellomake.h
#DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

#_OBJ = hellomake.o hellofunc.o 
#OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))
OBJ = echo-udp-client.o utils.o


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

echo-udp-client: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 
