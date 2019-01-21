
LIBNAME=	apb
VERSION=	1
REVISION=	0

INC=		/include
DIR=		/obj/


LIBFILE=	$(LIBNAME).lib
LIBDIR=		$(DIR)

DEF_DEBUG=debug=ff

C_COMPILER=	SC:c/sc

CPU=		68000
C_OPTS=		CPU=$(CPU) NOICONS STREQ STRMER NOSTKCHK NOSTANDARDIO INCLUDEDIRECTORY=$(INC) $(DEF_EBUG) 

LINKER=		SC:c/slink

C_SOURCES=	apblib.c

OBJECTS=	apblib.o

#LIBS=		LIB:sc.lib LIB:amiga.lib

################
#
# Build Library

$(LIBFILE): $(OBJECTS)
	oml $(LIBDIR)$(LIBFILE) r $(OBJECTS)

.c.o:
	$(C_COMPILER) $(C_OPTS) DISASM=$*.a  $*.c

$(FD_FILE).h:
	$(FD_CONV) $(FD_FILE) $(PRAGMA_FILE)

clean:
	@Delete $(OBJECTS)
	@Delete $(LIBDIR)$(LIBFILE)(|.info)

