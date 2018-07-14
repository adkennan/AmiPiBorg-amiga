
INCDIR=	/include
INC=	INCLUDEDIRECTORY $(INCDIR)
DIR=	/bin

FD_CONV=	SC:c/fd2pragma
FD_FILE=	remote.fd
PRAGMA_FILE=	$(INCDIR)/remote_pragmas.h

DEF_DEBUG=opt

C_COMPILER=	SC:c/sc

CPU=		68030
C_OPTS=		CPU=$(CPU) SAVEDS STREQ STRMER NOSTKCHK NOSTANDARDIO NOICONS $(INC)

LINKER=		SC:c/slink

LIBS=		LIB:sc.lib LIB:amiga.lib

################
#
# Build Library

serialremote.library: serialremote.o $(LIBS) $(PRAGMA_FILE)
	$(LINKER) WITH << 
TO $(DIR)/serialremote.library
FROM serialremote.o
LIB $(LIBS) NOICONS ADDSYM
<

$(PRAGMA_FILE): $(FD_FILE).h

.c.o:
	$(C_COMPILER) $(C_OPTS) $*.c

$(FD_FILE).h:
	$(FD_CONV) $(FD_FILE) $(PRAGMA_FILE)

clean:
	@Delete serialremote.o
	@Delete $(PRAGMA_FILE)
	@Delete $(DIR)/serialremote.library

