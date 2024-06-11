SUBDIRS := 1 2 3 4 5 6

export PATH := $(PATH)

.PHONY: all clean coverage

all:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

coverage:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir coverage; \
	done
