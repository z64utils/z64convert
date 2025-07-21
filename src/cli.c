#ifndef Z64CONVERT_GUI
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "z64convert.h"

#include <wow.h>

static inline void showargs(void)
{
	fprintf(stderr, "possible arguments (* = optional)\n");
	fprintf(stderr, " --in      'in.objex'     input objex\n");
	fprintf(stderr, " --out     'out.zobj'     output zobj\n");
	fprintf(stderr, " --address  0x06000000  * base address\n");
	fprintf(stderr, " --scale    1000.0f     * scale\n");
	fprintf(stderr, " --playas               * embed play-as data\n");
	fprintf(stderr, " --only    'l,i,s,t'    * include or exclude\n");
	fprintf(stderr, " --except  'l,i,s,t'    * groups named in list\n");
	fprintf(stderr, " --silent               * print only errors\n");
	//fprintf(stderr, " --docs                 * print old docs format\n"); // TODO this feature
	fprintf(stderr, " --print-palettes       * output palette addresses\n");
	fprintf(stderr, " --no-prefixes          * don't write variable name prefixes\n");
	fprintf(stderr, "                          - e.g. gEnWhoopee_DlFlap -> Flap\n");
	fprintf(stderr, " --world-header 'x,y,z' * embed minimal game world headers\n");
	fprintf(stderr, "                          - xyz: player spawn coordinates\n");
	fprintf(stderr, " --binary-header  fndcs * embed binary header\n");
	fprintf(stderr, "                          - f: footer instead of header\n");
	fprintf(stderr, "                          - n: embed names for each\n");
	fprintf(stderr, "                          - d: display lists\n");
	fprintf(stderr, "                          - c: collisions\n");
	fprintf(stderr, "                          - s: skeletons\n");
	fprintf(stderr, " --header  'out.h'      * write C header (--header - for stdout)\n");
	fprintf(stderr, " --linker  'out.ld'     * write C linker (--linker - for stdout)\n");
}

int wow_main(argc, argv)
{
	wow_main_args(argc, argv);
	FILE *docs;
	const char *err;
	int silent = 0;
	
	for (int i = 1; i < argc; ++i)
		if (!strcmp(argv[i], "--silent"))
			silent = 1;
	
	if (!silent)
	{
		fprintf(stderr, "welcome to "NAME" "VERSION" "AUTHOR"\n");
		fprintf(stderr, "%s\n", z64convert_flavor());
	}
	
	if (argc < 5 /* z64convert --in in.objex --out out.zobj */)
	{
		fprintf(stderr, "not enough arguments\n");
		showargs();
		return EXIT_FAILURE;
	}
	
	/* doc file */
	docs = stdout;
	/*if (!(docs = tmpfile())) // use this setup with gui
	{
		fprintf(stderr, "failed to create log file\n");
		return EXIT_FAILURE;
	}*/
	
	if ((err = z64convert(argc, (const char**)argv, docs, wow_die)))
	{
		fprintf(stderr, "error: ");
		fwrite(err, 1, strlen(err), stderr);
		fprintf(stderr, "\n");
		return EXIT_FAILURE;
	}
	
	if (!silent)
		fprintf(stderr, "success!\n");
	
	if (docs != stdout)
		fclose(docs);
	
	return EXIT_SUCCESS;
}
#endif /* ifndef Z64CONVERT_GUI */

