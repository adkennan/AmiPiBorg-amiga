
OUTDIR=$(ROOT)obj/

LIBFILE=$(OUTDIR)apb.lib

SCFLAGS=$(LIBFLAGS) CPU=$(CPU) $(DEF_LOGGING) $(DEF_DEBUG) INCLUDEDIRECTORY=$(ROOT)$(INC)

OBJECTS=apblib.o

################
#
# Build Library

$(LIBFILE): $(OBJECTS)
	oml $(LIBFILE) r $(OBJECTS)

clean:
	@Delete $(OBJECTS)
	@Delete $(LIBFILE)(|.info)

