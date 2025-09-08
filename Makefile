# borra cualquier Makefile extraño y créalo limpio
rm -f makefile GNUmakefile Makefile
cat > Makefile <<'EOF'
CXX      := g++
CXXFLAGS := -Wall -O2 -pthread -Iinclude

SRC_DIR  := src
BIN_DIR  := bin

# Binarios a generar
BINS := $(BIN_DIR)/p1_counter $(BIN_DIR)/p2_ring $(BIN_DIR)/p3_rw $(BIN_DIR)/p4_deadlock $(BIN_DIR)/p5_pipeline

COMMON := $(SRC_DIR)/utils.cpp include/utils.h

.PHONY: all clean run_p2
all: $(BINS)

$(BIN_DIR)/p1_counter: $(SRC_DIR)/p1_counter.cpp $(COMMON) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(SRC_DIR)/utils.cpp -o $@

$(BIN_DIR)/p2_ring: $(SRC_DIR)/p2_ring.cpp $(COMMON) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(SRC_DIR)/utils.cpp -o $@

$(BIN_DIR)/p3_rw: $(SRC_DIR)/p3_rw.cpp $(SRC_DIR)/utils.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(SRC_DIR)/utils.cpp -o $@

$(BIN_DIR)/p4_deadlock: $(SRC_DIR)/p4_deadlock.cpp $(SRC_DIR)/utils.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(SRC_DIR)/utils.cpp -o $@

$(BIN_DIR)/p5_pipeline: $(SRC_DIR)/p5_pipeline.cpp $(SRC_DIR)/utils.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< $(SRC_DIR)/utils.cpp -o $@


$(BIN_DIR):
	mkdir -p $(BIN_DIR)

run_p2: $(BIN_DIR)/p2_ring
	$(BIN_DIR)/p2_ring 4 5

clean:
	rm -f $(BIN_DIR)/*
EOF

# Asegura fin de línea UNIX por si acaso
sed -i 's/\r$//' Makefile

# Muestra las primeras 60 líneas para confirmar que existe y tiene reglas
nl -ba Makefile | sed -n '1,60p'
