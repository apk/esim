#include <stdlib.h>
#include <stdio.h>

char *lines [100000];

main () {
	char buf [1024];
	int nlines = 0;
	srand48 (time (0) + getpid ());
	while (fgets (buf, sizeof (buf), stdin)) {
		lines [nlines ++] = strdup (buf);
	}
	while (nlines > 0) {
		int i;
		int p = lrand48 () % nlines;
		fputs (lines [p], stdout);
		nlines --;
		for (i = p; i < nlines; i ++) {
			lines [i] = lines [i + 1];
		}
	}
	return 0;
}
