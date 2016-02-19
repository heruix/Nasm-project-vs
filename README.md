# Nasm-project-vs
Visual Studio solution to build Netwide Assembler

Thanks to: shaynox - http://forum.nasm.us/index.php?topic=2064.0

to build the same solution with future version you have to:

modify nasm.c so it contains:
-  the copyright info
-  #include "common.h"
-  and the main procedure/function
-  all the rest of the code goes into common.h in the same directory as nasm.c

modify ndisasm.c so it contains:
- the copyright info
- followed by nex includes
  #include "compiler.h"
  #include <stdio.h>
  #include <stdarg.h>
  #include <stdlib.h>
  #include <string.h>
  #include <ctype.h>
  #include <errno.h>
  #include <inttypes.h>
  #include "insns.h"
- mark #include "nasm.h" as comment
- add #include "common.h" in place followed by
  #include "nasmlib.h"
  #include "sync.h"
  #include "disasm.h"

add a file common.h in the same directory as nasm.c and ndisasm.c (root directory mostly) with the contents of nasm.c without the main procedure/function.  Also add the copyright notice on top of it.
right below and just above #include "compiler.h" add a line #include "nasm.h" file.
I'm no C/C++ expert but the solution now works to build ndisasm and nasm in the same solution with different projects. The builds go in the same directory.

If there should be any error in my method please let me know. (admin@agguro.org)
