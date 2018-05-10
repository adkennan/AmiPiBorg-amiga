
LIBNAME=	amipiborg
VERSION=	1
REVISION=	0

INC=		/include
DIR=		/bin/


LIBFILE=	$(LIBNAME).library
LIBDIR=		$(DIR)

FD_CONV=	SC:c/fd2pragma
FD_FILE=	$(LIBNAME).fd
PRAGMA_FILE=	$(INC)/$(LIBNAME)_pragmas.h

DEF_DEBUG=opt

C_COMPILER=	SC:c/sc

CPU=		68030
C_OPTS=		CPU=$(CPU) NOICONS STREQ STRMER NOSTKCHK NOSTANDARDIO INCLUDEDIRECTORY=$(INC) $(DEF_EBUG)

LINKER=		SC:c/slink

C_SOURCES=	lib.c memory.c connection.c request.c io.c

OBJECTS=	lib.o memory.o connection.o request.o io.o

LIBS=		LIB:sc.lib LIB:amiga.lib

################
#
# Build Library

$(LIBFILE): $(OBJECTS) $(LIBS) $(PRAGMA_FILE)
	$(LINKER) WITH << 
TO $(LIBDIR)$(LIBFILE)
FROM $(OBJECTS)
LIBFD $(FD_FILE)
LIBVERSION $(VERSION)
LIBREVISION $(REVISION)
LIB $(LIBS) NOICONS ADDSYM
<


$(PRAGMA_FILE): $(FD_FILE).h

.c.o:
	$(C_COMPILER) $(C_OPTS) $*.c

$(FD_FILE).h:
	$(FD_CONV) $(FD_FILE) $(PRAGMA_FILE)

clean:
	@Delete $(OBJECTS)
	@Delete $(LIBDIR)$(LIBFILE)(|.info)
	@Delete $(PRAGMA_FILE)

reload:
	@version $(LIBFILE)
	@flushlibs