
DIR=		//bin/

EXENAME=	ApbDate
EXEPATH=	$(DIR)

INC=		INCLUDEDIRECTORY=//include

COMPILER=	SC:c/sc
LINKER=		SC:c/slink

LIBS=		LIB:sc.lib LIB:amiga.lib

CPU=		68030

C_OPTS=		noicons cpu=$(CPU) $(INC)

C_SOURCES=	apbdate.c 

OBJECTS=	apbdate.o

$(EXENAME): $(OBJECTS)
   $(LINKER) WITH <<
 TO $(EXEPATH)$(EXENAME)
 FROM LIB:c.o $(OBJECTS)
 LIBRARY $(LIBS) NOICONS ADDSYM
<
.c.o:
	$(COMPILER) $(C_OPTS) $*.c

clean:
	@Delete $(OBJECTS)
	@Delete $(EXEPATH)$(EXENAME) 
