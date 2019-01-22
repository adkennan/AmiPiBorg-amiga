
EXENAME=AmiPiBorg

SCFLAGS=$(EXEFLAGS) CPU=$(CPU) $(DEF_LOGGING) $(DEF_DEBUG) INCLUDEDIRECTORY=$(ROOT)$(INC)

OBJECTS=main.o server.o config.o buffer.o packetwriter.o packetreader.o connection.o protocol.o

LIBS=$(LIBS) $(ROOT)obj/apb.lib

$(EXENAME): $(OBJECTS)
   $(LD) WITH <<
 TO $(ROOT)$(OUTDIR)$(EXENAME)
 FROM LIB:c.o $(OBJECTS)
 LIBRARY $(LIBS) NOICONS ADDSYM
<

clean:
	@Delete $(OBJECTS)
	@Delete $(ROOT)$(OUTDIR)$(EXENAME)
