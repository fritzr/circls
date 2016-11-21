
SUBS = $(shell ls -F | grep -- '/')

all: $(SUBS)

.PHONY: all $(SUBS)

$(SUBS):
	@$(MAKE) -C $@
