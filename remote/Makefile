
FD_FILE=remote.fd
PRAGMA_FILE=$(ROOT)$(INC)/remote_pragmas.h

SCFLAGS=$(LIBFLAGS) CPU=$(CPU) $(DEF_LOGGING) $(DEF_DEBUG) INCLUDEDIRECTORY=$(ROOT)$(INC)

LIBS=LIB:sc.lib LIB:amiga.lib

################
#
# Build Library

serialremote.library: serialremote.o $(LIBS) $(PRAGMA_FILE)
	$(LD) WITH << 
TO $(ROOT)$(OUTDIR)serialremote.library
FROM serialremote.o
LIB $(LIBS) NOICONS ADDSYM
<

$(PRAGMA_FILE): $(FD_FILE).h

$(FD_FILE).h:
	$(FD_CONV) $(FD_FILE) $(PRAGMA_FILE)

clean:
	@Delete serialremote.o
	@Delete $(PRAGMA_FILE)
	@Delete $(ROOT)$(OUTDIR)serialremote.library

