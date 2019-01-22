
LIBNAME=	amipiborg
VERSION=	1
REVISION=	0

LIBFILE=	$(LIBNAME).library
FD_FILE=	$(LIBNAME).fd

PRAGMA_FILE=	$(ROOT)$(INC)/$(LIBNAME)_pragmas.h

SCFLAGS=$(LIBFLAGS) CPU=$(CPU) $(DEF_LOGGING) $(DEF_DEBUG) INCLUDEDIRECTORY=$(ROOT)$(INC)

OBJECTS=lib.o context.o log.o stats.o memory.o connection.o request.o io.o control.o objectpool.o

LIBS=LIB:sc.lib LIB:amiga.lib

################
#
# Build Library

$(LIBFILE): $(OBJECTS) $(LIBS) $(PRAGMA_FILE)
	$(LD) WITH << 
TO $(ROOT)$(OUTDIR)$(LIBFILE)
FROM $(OBJECTS)
LIBFD $(FD_FILE)
LIBVERSION $(VERSION)
LIBREVISION $(REVISION)
LIB $(LIBS) NOICONS ADDSYM
<

$(PRAGMA_FILE): $(FD_FILE).h

$(FD_FILE).h:
	$(FD_CONV) $(FD_FILE) $(PRAGMA_FILE)

clean:
	@Delete $(OBJECTS)
	@Delete $(ROOT)$(OUTDIR)$(LIBFILE)(|.info)
	@Delete $(PRAGMA_FILE)

reload:
	@version $(ROOT)$(OUTDIR)$(LIBFILE)
	@flushlibs