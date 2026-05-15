# Convenience targets for the scire-h repo.
#
# Quick reference:
#
#   make test        run the JUCE-independent DSP unit tests
#   make plugin      build the JUCE plug-in (macOS only; requires Xcode)
#   make install     build + install the AU into ~/Library/Audio/Plug-Ins
#   make clean       remove all build / binary artefacts
#   make web         open the HTML clone in the default browser

.PHONY: test plugin install clean web

NATIVE := ds4-native
BUILD  := $(NATIVE)/build

test:
	@cd $(NATIVE)/tests && ./run_all.sh

plugin:
	@command -v cmake >/dev/null || { echo "cmake not found"; exit 1; }
	@cmake -B $(BUILD) -S $(NATIVE) -G Xcode
	@cmake --build $(BUILD) --config Release

install: plugin
	@echo "AU installed to ~/Library/Audio/Plug-Ins/Components/"
	@ls -la "$(HOME)/Library/Audio/Plug-Ins/Components/" 2>/dev/null | grep -i "ult-sound\|ds-4m" || true

clean:
	@rm -rf $(BUILD) $(NATIVE)/tests/build $(NATIVE)/tests/bin
	@rm -f  $(NATIVE)/tests/test_TriangleCoreVCO \
	        $(NATIVE)/tests/test_ExpEnvelope \
	        $(NATIVE)/tests/test_OTAVCA \
	        $(NATIVE)/tests/test_LFOSchmitt \
	        $(NATIVE)/tests/test_PiezoTrigger

web:
	@command -v xdg-open >/dev/null && xdg-open index.html || \
	 command -v open     >/dev/null && open     index.html || \
	 echo "Open index.html in your browser manually"
