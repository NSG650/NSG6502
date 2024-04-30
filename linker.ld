MEMORY {
	ZP: start = $0, size = $100, type = rw;
	RAM: start = $300, size = $dd00, type = rw;
	ROM: start = $E000, size= $1fff, type = ro;
}

SEGMENTS {
	ZEROPAGE: load = "ZP", type = rw;
	CODE: load = "ROM", type = ro;
	RODATA: load = "ROM", type = ro;
}
