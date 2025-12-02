CC ?= gcc
CFLAGS = -Wall -O3
CFLAGS_DEBUG = -Wall -g -DDEBUG
LIBS = `pkg-config uuid fuse3 libical --cflags --libs`
OUTDIR=build
PREFIX?=/usr/local
BINDIR?=$(PREFIX)/bin
MANDIR?=$(PREFIX)/share/man
.DEFAULT_GOAL=all


agendafs: main.c path.c util.c hashmap.c agenda_entry.c tree.c
	mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) *.c -o $(OUTDIR)/mount_agendafs.o $(LIBS)

debug: main.c path.c util.c hashmap.c agenda_entry.c tree.c
	mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS_DEBUG) *.c -o $(OUTDIR)/mount_agendafs_debug.o $(LIBS)

clean:
	rm -rf $(OUTDIR)

manpages:
	scdoc < agendafs.8.scd > $(OUTDIR)/agendafs.8
	scdoc < agendafs-examples.7.scd > $(OUTDIR)/agendafs-examples.7

all: agendafs manpages

install:
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man8
	install -m755 $(OUTDIR)/mount_agendafs.o $(DESTDIR)$(BINDIR)/mount.agendafs
	install -m644 $(OUTDIR)/agendafs.8 $(DESTDIR)$(MANDIR)/man8/agendafs.8
	ln -fs $(DESTDIR)$(MANDIR)/man8/agendafs.8 $(DESTDIR)$(MANDIR)/man8/mount.agendafs.8
	install -m644 $(OUTDIR)/agendafs-examples.7 $(DESTDIR)$(MANDIR)/man7/agendafs-examples.7

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/mount.agendafs
	rm -r $(DESTDIR)$(MANDIR)/man8/mount.agendafs.8
	rm -f $(DESTDIR)$(MANDIR)/man8/agendafs.8
	rm -r $(DESTDIR)$(MANDIR)/man7/agendafs-examples.8
