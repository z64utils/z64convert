#ifdef Z64CONVERT_GUI
#include <errno.h>
#include <stdarg.h>

#include <wow.h>
#define WOW_GUI_IMPLEMENTATION
#include <wow_gui.h>
#include <wow_clipboard.h>

#include "doc.h"

#include "z64convert.h"

static int gui_errors = 0;

#define WIDTH_1 140 /* width of first column */
#define WIDTH_2 200 /* width of second column */

enum vtype
{
	VTYPE_HEX
	, VTYPE_FLOAT
	, VTYPE_STRING
};
#define VTYPE_STRING_SZ 1024

#define STATUSBAR_H (20) /* the bar at the bottom showing filenames */
#define WINW 404
#define WINH (168 + STATUSBAR_H)//(596+STATUSBAR_H)//600

#define BPAD 4     /* image border padding */
#define BP(X) ((X)+BPAD)

#define PROG_NAME_VER_ATTRIB NAME " "VERSION " "AUTHOR

static struct
{
	char only[VTYPE_STRING_SZ];
	char except[VTYPE_STRING_SZ];
	unsigned int address;
	float scale;
	int playas;
} g =
{
	.address = 0x06000000
	, .scale = 1000
};

static char *optional = "optional";

static int empty(const char *str)
{
	return (str == optional) || !strlen(str) || !strcmp(str, optional);
}

static inline void wantValue(char *label, void *value, enum vtype type)
{
	wowGui_column_width(WIDTH_1);
	wowGui_label(label);
	wowGui_column_width(WIDTH_2);
	switch (type)
	{
	case VTYPE_HEX:
		wowGui_hex(value);
		break;
	case VTYPE_FLOAT:
		wowGui_float(value);
		break;
	case VTYPE_STRING:
		wowGui_textbox(value, VTYPE_STRING_SZ);
		break;
	}
	wowGui_column_width(8*3);
	if (type == VTYPE_STRING)
	{
		if (wowGui_button("x"))
		{
			char *str = value;
			*str = '\0';
		}
	}
	else
		wowGui_label("");
}

void gui_output(char *output_text)
{
#define OUTWINW (356 + (356 / 4))
#define OUTWINH (152 * 3)
	wowGui_bind_init(NAME " output", OUTWINW, OUTWINH);
	
	wow_windowicon(1);
	
	while (1)
	{
		/* wowGui_frame() must be called before you do any input */
		wowGui_frame();
		
		/* events */
		wowGui_bind_events();
		
		if (wowGui_bind_should_redraw())
		{
			/* draw */
			wowGui_viewport(0, 0, OUTWINW, OUTWINH);
			wowGui_padding(8, 8);
			
			static struct wowGui_window win = {
				.rect = {
					.x = 0
					, .y = 0
					, .w = OUTWINW
					, .h = OUTWINH
				}
				, .color = 0x301818FF
			};
			static struct wowGui_window copy2clipboard = {
				.rect = {
					.x = 0
					, .y = 0
					, .w = 8 * (strlen("[Copy to clipboard]")-1) + 8 * 2
					, .h = 16 + 8
				}
				, .color = 0x301818FF
				, .not_scrollable = 1
				, .scroll_valuebox = 1
			};
			copy2clipboard.rect.x = OUTWINW - copy2clipboard.rect.w - 8;
			
			if (wowGui_window(&win))
			{
				wowGui_columns(1);
				
				wowGui_column_width(OUTWINW);
				
				wowGui_label(output_text);
				
				wowGui_window_end();
			}
			if (wowGui_window(&copy2clipboard))
			{
				if (wowGui_button("Copy to clipboard"))
				{
					if (wowClipboard_set(output_text))
						wowGui_dief("fatal clipboard error");
					wowGui_infof("Log successfully copied to clipboard!");
				}
				wowGui_window_end();
			}
		} /* wowGui_bind_should_redraw */
		
		wowGui_frame_end(wowGui_bind_ms());
		
		/* display */
		wowGui_bind_result();
		
		/* loop exit condition */
		if (wowGui_bind_endmainloop())
			break;
	}
	
	wowGui_bind_quit();
}

void fnExit1(void)
{
	static int has_forked = 0;
	if (!has_forked)
	{
		has_forked = 1;
		if (gui_errors)
			exit(EXIT_SUCCESS);
	}
}

int wow_main(argc, argv)
{
	wow_main_args(argc, argv);
	atexit(fnExit1);
	/* pass arguments over to zzobjman */
	if (argc > 1)
	{
		for (int i = 0; i < argc; ++i)
			if (!strcasecmp(argv[i], "--gui_errors"))
				gui_errors = 1;
		
		FILE *docs;
		const char *err;
		char *docs_str;
		size_t docs_sz;
		
		if (argc < 5 /* z64convert --in in.objex --out out.zobj */)
		{
			wowGui_dief("not enough arguments");
			return EXIT_FAILURE;
		}
		
		/* doc file */
		if (!(docs = tmpfile())) // use this setup with gui
		{
			wowGui_dief("failed to create log file");
			return EXIT_FAILURE;
		}
		
		/* newline at the top of output */
		fprintf(docs, "\n");
		
		/* run z64convert, with error checking */
		if ((err = z64convert(argc, (const char**)argv, docs, wowGui_dief)))
		{
			wowGui_dief(err);
			return EXIT_FAILURE;
		}
		
		/* report success */
		wowGui_infof("success!");
		
		/* retrieve docs as 0-term'd string */
		// document_mergeDefineHeader(docs);
		document_mergeExternHeader(NULL, docs, NULL, false);
		docs_sz = ftell(docs);
		if (!(docs_str = malloc(docs_sz + 1)))
		{
			wowGui_dief("memory failure");
			return EXIT_FAILURE;
		}
		fseek(docs, 0, SEEK_SET);
		if (fread(docs_str, 1, docs_sz, docs) != docs_sz)
		{
			wowGui_dief("read failure");
			return EXIT_FAILURE;
		}
		docs_str[docs_sz] = '\0';
		
		/* can safely close docs now */
		fclose(docs);
		document_free();
		
		/* write output to stdout */
		fprintf(stdout, "/* documentation */");
		fwrite(docs_str, 1, docs_sz, stdout);
		fprintf(stdout, "\n");
		
		/* display output as window */
		gui_output(docs_str);
		
		/* cleanup */
		free(docs_str);
		return EXIT_SUCCESS;
	}
	char *args = 0;
	const char *flavor = z64convert_flavor();
	
	/* default */
	strcpy(g.only, optional);
	strcpy(g.except, optional);
	
	wowGui_bind_init(NAME, WINW, WINH);
	
	wow_windowicon(1);
	
	while (1)
	{
		/* wowGui_frame() must be called before you do any input */
		wowGui_frame();
		
		/* events */
		wowGui_bind_events();
		
		if (wowGui_bind_should_redraw())
		{
			//{static int k=0;fprintf(stderr, "redraw %d\n", k++);}
			/* draw */
			//wowGui_bind_clear(0x804040FF);
			
			wowGui_viewport(0, 0, WINW, WINH);
			wowGui_padding(8, 8);
			
			static struct wowGui_window win = {
				.rect = {
					.x = 0
					, .y = 0
					, .w = WINW
					, .h = WINH - STATUSBAR_H
				}
				, .color = 0x301818FF
				, .not_scrollable = 1
				, .scroll_valuebox = 1
			};
			
			static struct wowGui_window statusbar = {
				.rect = {
					.x = 0
					, .y = 0 + (WINH - STATUSBAR_H)
					, .w = WINW
					, .h = STATUSBAR_H
				}
				, .color = 0x1c0e0eff
				, .not_scrollable = 1
				, .scroll_valuebox = 1
			};
			
			if (wowGui_window(&win))
			{
				wowGui_row_height(20);
				wowGui_columns(1);
				
				wowGui_column_width(8 * sizeof(PROG_NAME_VER_ATTRIB));
				wowGui_italic(2);
				wowGui_label(PROG_NAME_VER_ATTRIB);
				wowGui_italic(0);
				
				wowGui_columns(3);
				wowGui_tails(1);
				
				/* file droppers */
				wowGui_columns(3);
				static struct wowGui_fileDropper in = {
					.label = "OBJEX (in)"
					, .labelWidth = WIDTH_1
					, .filenameWidth = WIDTH_2
					, .extension = "objex"
				};
				static struct wowGui_fileDropper out = {
					.label = "ZOBJ  (out)"
					, .labelWidth = WIDTH_1
					, .filenameWidth = WIDTH_2
					, .extension = "zobj"
					, .isCreateMode = 1 /* ask for save file name */
				};
				wowGui_fileDropper(&in);
				wowGui_fileDropper(&out);
				wantValue("Only", g.only, VTYPE_STRING);
				wantValue("Except", g.except, VTYPE_STRING);
				wantValue("Address", &g.address, VTYPE_HEX);
				wantValue("Scale", &g.scale, VTYPE_FLOAT);
				
				wowGui_column_width(WIDTH_2);
				wowGui_checkbox("embed play-as data", &g.playas);
				
				wowGui_column_width(WIDTH_1);
				if (wowGui_clickable("     convert"))
				{
					if (wowGui_fileDropper_filenameIsEmpty(&in))
						wowGui_warnf("no input objex specified");
					else if (wowGui_fileDropper_filenameIsEmpty(&out))
						wowGui_warnf("no output zobj specified");
					else
					{
						/* all tests passed; now construct arguments */
						if (!args)
							args = malloc(16 * 1024); /* 16kb is plenty */
						sprintf(
							args
							, "--in \"%s\" --out \"%s\" --address 0x%08X --scale %f "
							, in.filename, out.filename, g.address, g.scale
							);
						if (!empty(g.only))
							sprintf(
								args + strlen(args)
								, " --only \"%s\" "
								, g.only
								);
						if (!empty(g.except))
							sprintf(
								args + strlen(args)
								, " --except \"%s\" "
								, g.except
								);
						if (g.playas)
							sprintf(
								args + strlen(args)
								, " --playas "
								);
						strcat(args, " --gui_errors ");
						if (wow_system_gui(argv[0], args))
							wowGui_warnf("something went wrong");
					}
				}
				
				wowGui_window_end();
			}
			
			/* display flavor text */
			if (wowGui_window(&statusbar))
			{
				statusbar.scroll.y = -6;
				wowGui_label(flavor);
				
				wowGui_window_end();
			}
		} /* wowGui_bind_should_redraw */
		
		wowGui_frame_end(wowGui_bind_ms());
		
		/* display */
		wowGui_bind_result();
		
		/* loop exit condition */
		if (wowGui_bind_endmainloop())
			break;
	}
	
	wowGui_bind_quit();
	if (args)
		free(args);
	
	return 0;
}
#endif /* Z64CONVERT_GUI */

