// stdio.h
"feof",
  "fflush",
  "fgets",
  "fputs",
  "fopen",
  "fprintf",
  "fputc",
  "fscanf",
  "fseek",
  "ftell",
  "fwrite",
  "fread",
  "printf",
  "putchar",
  "puts",
  "remove",
  "rewind",
  "vfprintf",

  "getc",
  "getchar",
  "_IO_getc",

// intrinsics
  "llvm.lifetime.start",
  "llvm.lifetime.start.p0i8",
  "llvm.lifetime.end",
  "llvm.lifetime.end.p0i8",

// stdlib.h
  "abort",
  "exit",
  "rand",
  "realloc",
  "srand",

// unistd.h
  "close",
  "lseek",
  "pread",
  "read",
  "write",

// string.h
  "strtok",
  "strtod",
  "strtol",

// cuda.h
  "cuCtxSynchronize",

// autocuda
  "lcuCtxForceSynchronize",

// From some old, odd standard of C.
// Observed in olden_em3d
  "lrand48",
  "drand48",
