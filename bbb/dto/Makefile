FIRMWARE=/lib/firmware
SLOTS=/sys/devices/bone_capemgr.9/slots

OVERLAY_SRC=$(wildcard *.dts)
OVERLAY=$(OVERLAY_SRC:%.dts=%)
OVERLAY_BIN=$(OVERLAY_SRC:%.dts=%-00A0.dtbo)

OVERLAY_INST=$(OVERLAY_BIN:%=$(FIRMWARE)/%)

OVERLAY_DEACTIVATE_TARGETS=$(OVERLAY:%=deactivate-%)
OVERLAY_ACTIVATE_TARGETS=$(OVERLAY:%=activate-%)

all: $(OVERLAY_BIN)

install: install-overlays

.PHONY: dto all install activate deactivate \
  $(OVERLAY_ACTIVATE_TARGETS) $(OVERLAY_DEACTIVATE_TARGETS)

dto: all
	@echo "Try another target:"
	@for i in install activate deactivate; do \
	  echo "\tmake dto $$i"; \
	done

# Build the overlay blob from device tree source
$(OVERLAY_BIN): %-00A0.dtbo: %.dts
	dtc -O dtb -o $@ -b 0 -@ $<

install-overlays: $(OVERLAY_INST)

# Install blobs into lib/firmware
$(OVERLAY_INST): $(FIRMWARE)/%-00A0.dtbo: %-00A0.dtbo
	@echo "Installing $< to $(FIRMWARE)..."
	@sudo sh -c 'cp $< $@'

activate:
	@echo "Try one of these:"
	@for i in $(OVERLAY_ACTIVATE_TARGETS); do \
	  echo "\tmake dto $$i"; \
	done

deactivate:
	@echo "Try one of these:"
	@for i in $(OVERLAY_DEACTIVATE_TARGETS); do \
	  echo "\tmake dto $$i"; \
	done

$(OVERLAY_ACTIVATE_TARGETS): activate-%: $(FIRMWARE)/%-00A0.dtbo
	@OVL=$(@:activate-%=%) && \
	echo "Activating $$OVL..." && \
	sudo sh -c "echo $$OVL > $(SLOTS)" \
	  && echo "ACTIVE as $$(grep $$OVL $(SLOTS))" \
	  || echo "FAILED to activate $$OVL"

$(OVERLAY_DEACTIVATE_TARGETS):
	@OVL=$(@:deactivate-%=%) && \
	ID=$$(grep $$OVL $(SLOTS) | cut -d':' -f1 | cut -d' ' -f2) && \
	test ! -z "$$ID" && \
	echo "Deactivating [$$ID] $$OVL..." && \
	sudo sh -c "echo -$$ID > $(SLOTS)" \
	&& echo "REMOVED [$$ID] $$OVL"
