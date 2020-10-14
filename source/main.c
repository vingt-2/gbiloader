#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <fat.h>
#include <sdcard/gcsd.h>
#include "aram/sidestep.h"

#define CONFIG_FILE "fat:/gbiloader/gbiloader.ini"
#define BOOTPREF_FILE "fat:/gbiloader/boot.prf"
#define MAX_CONFIG_LINE 256

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

typedef enum { GBI, GBI_LL, GBI_ULL, SWISS } dol_type;

//Config
int verbosity_level = 1;
dol_type boot_dol = GBI;

int console_line = 2;

void consolef(int verbosity, char *format, char *param)
{
	if (verbosity <= verbosity_level)
	{
		char output[128] = "\x1b[%d;6H";
		strcat(output, format),

		kprintf(output, console_line, param);

		console_line++;
	}
}

void console(int verbosity, char *format)
{
	consolef(verbosity, format, "");
}

void set_boot_pref(char *data)
{
	FILE *bootpref = fopen(BOOTPREF_FILE, "w");

	if (bootpref != NULL)
	{
		fputs(data, bootpref);
		fclose(bootpref);
	}
	else
	{
		console(0, "Can't write boot pref. Check SD lock.");
	}
}

// Note: this function only reads the first line of a file.
void get_boot_pref(char *bootpref_content)
{
	FILE *bootpref = fopen(BOOTPREF_FILE, "r");

	if (bootpref != NULL)
	{
		fgets(bootpref_content, 127, bootpref);
		fclose(bootpref);
	}
	else
	{
		strcpy(bootpref_content, "");
	}
}

bool CheckResetGamecube()
{
	if (SYS_ResetButtonDown())
	{
		SYS_ResetSystem(SYS_RESTART, 0, 0);
		return true;
	}

	return false;
}

void Initialise()
{
	VIDEO_Init();
	PAD_Init();

	if (rmode == NULL)
	{
		rmode = VIDEO_GetPreferredMode(NULL);
	}

	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb, 20, 20, rmode->fbWidth, rmode->xfbHeight, rmode->fbWidth * VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();

	// Controller will show no input on first post-init frame if we don't wait two vsyncs
	VIDEO_WaitVSync();
	VIDEO_WaitVSync();
}

static int initFAT()
{
	__io_gcsd2.startup();

	if (!__io_gcsd2.isInserted())
	{
		return 0;
	}

	if (!fatMountSimple("fat", &__io_gcsd2))
	{
		return 0;
	}

	return 1;
}

int readparseconf(char *config)
{
	FILE *pFile;
	char mystring[MAX_CONFIG_LINE];

	pFile = fopen(config, "r");

	if (pFile == NULL)
	{
		consolef(0, "Error opening %s\n", config);
		return -1;
	}
	else
	{
		while (1)
		{
			if (fgets(mystring, MAX_CONFIG_LINE - 1, pFile) == NULL)
			{
				break;
			}

			if (mystring[0] == '#' || mystring[0] == '\n' || mystring[0] == '\r')
			{
				continue;
			}

			if (strncmp("VIDEO_MODE=", mystring, 11) == 0)
			{
				if (strncmp(mystring + 11, "auto", 4) == 0)
				{
					rmode = VIDEO_GetPreferredMode(NULL);
				}
				else if (strncmp(mystring + 11, "240p", 4) == 0)
				{
					rmode = &TVNtsc240Ds;
				}
				else if (strncmp(mystring + 11, "480i", 4) == 0)
				{
					rmode = &TVNtsc480Int;
				}
				else if (strncmp(mystring + 11, "480p", 4) == 0)
				{
					rmode = &TVNtsc480Prog;
				}
				else if (strncmp(mystring + 11, "264p", 4) == 0)
				{
					rmode = &TVPal264Ds;
				}
				else if (strncmp(mystring + 11, "528i", 4) == 0)
				{
					rmode = &TVPal528Int;
				}
				else if (strncmp(mystring + 11, "576p", 4) == 0)
				{
					rmode = &TVPal576ProgScale;
				}
			}

			if (strncmp("VERBOSE=", mystring, 8) == 0)
			{
				if (strncmp(mystring + 8, "1", 1) == 0)
				{
					verbosity_level = 1;
				}
				else if (strncmp(mystring + 8, "0", 1) == 0)
				{
					verbosity_level = 0;
				}
			}
		}

		fclose(pFile);

		char bootpref[128];
		get_boot_pref(bootpref);

		if (bootpref != NULL)
		{
			if (strcmp(bootpref, "gbi") == 0)
			{
				boot_dol = GBI;
			}
			else if (strcmp(bootpref, "gbi-ll") == 0)
			{
				boot_dol = GBI_LL;
			}
			else if (strcmp(bootpref, "gbi-ull") == 0)
			{
				boot_dol = GBI_ULL;
			}
			else if (strcmp(bootpref, "swiss") == 0)
			{
				boot_dol = SWISS;
			}
		}
	}

	return 0;
}

int HaltLoop()
{
	while (1)
	{
		if (CheckResetGamecube())
		{
			return 0;
		}

		VIDEO_WaitVSync();
	}
}

int main(int argc, char *argv[])
{
	if (initFAT() == 0)
	{
		Initialise();
		console(0, "Unable to access SD card.");
		return HaltLoop();
	}

	if (readparseconf(CONFIG_FILE) < 0)
	{
		Initialise();
		console(0, "Unable to read config file.");
		return HaltLoop();
	}

	Initialise();

	console(1, "gbiloader r2 by Adam Zey");
	console(1, "");

	bool showMenu = false;
	FILE * fp;
	int size;

	// Check if the user is trying to override the GBI version
	PAD_ScanPads();
	
	//// kprintf("\x1b[%d;6HButton held state: %d", console_line, PAD_ButtonsHeld(0));
	//// console_line++;

	if (PAD_ButtonsHeld(0) & PAD_BUTTON_Y)
	{
		showMenu = true;

		console(1, "To select the version of GBI to boot, please press one");
		console(1, "of the following buttons. Your preference will be saved.");
		console(1, "");
		console(1, "Press A for GBI");
		console(1, "Press B for GBI-LL");
		console(1, "Press X for GBI-ULL");
		console(1, "Press L for Swiss");
	}

	while (true)
	{
		if (showMenu)
		{
			if (CheckResetGamecube())
			{
				return 0;
			}

			PAD_ScanPads();

			int buttonsDown = PAD_ButtonsDown(0);

			if (buttonsDown & PAD_BUTTON_A)
			{
				boot_dol = GBI;
				set_boot_pref("gbi");

				showMenu = false;
			}
			else if (buttonsDown & PAD_BUTTON_B)
			{
				boot_dol = GBI_LL;
				set_boot_pref("gbi-ll");

				showMenu = false;
			}
			else if (buttonsDown & PAD_BUTTON_X)
			{
				boot_dol = GBI_ULL;
				set_boot_pref("gbi-ull");

				showMenu = false;
			}
			else if (buttonsDown & PAD_TRIGGER_L)
			{
				boot_dol = SWISS;
				set_boot_pref("swiss");

				showMenu = false;
			}
		}
		else
		{
			char bootpath[256];
			char clipath[256];

			switch (boot_dol)
			{
				case GBI:
					strcpy(bootpath, "fat:/gbiloader/gbi.dol");
					break;
				case GBI_LL:
					strcpy(bootpath, "fat:/gbiloader/gbi-ll.dol");
					break;
				case GBI_ULL:
					strcpy(bootpath, "fat:/gbiloader/gbi-ull.dol");
					break;
				case SWISS:
					strcpy(bootpath, "fat:/gbiloader/swiss.dol");
					break;
			}

			consolef(1, "Loading %s ...", bootpath);

			fp = fopen(bootpath, "rb");

			if (fp == NULL)
			{
				consolef(0, "Failed to open %s.", bootpath);
				console(0, "Check config file and folder structure.\n\n");
				return HaltLoop();
			}

			if (fp != NULL)
			{
				fseek(fp, 0, SEEK_END);
				size = ftell(fp);
				fseek(fp, 0, SEEK_SET);

				if ((size > 0) && (size < (AR_GetSize() - (64 * 1024))))
				{
					u8 *dol = (u8*)memalign(32, size);

					if (dol)
					{
						fread(dol, 1, size, fp);

						//CLI support
						strcpy(clipath, bootpath);
						clipath[strlen(clipath) - 3] = 'c';
						clipath[strlen(clipath) - 2] = 'l';
						clipath[strlen(clipath) - 1] = 'i';

						FILE * fp2;
						int size2;

						fp2 = fopen(clipath, "rb");

						if (fp2 != NULL)
						{
							fseek(fp2, 0, SEEK_END);
							size2 = ftell(fp2);
							fseek(fp2, 0, SEEK_SET);

							// Build a command line to pass to the DOL
							int argc2 = 0;
							char *argv2[1024];
							char *cli_buffer = memalign(32, size2 + 1); // +1 to append null character if needed

							if (cli_buffer)
							{
								fread(cli_buffer, 1, size2, fp2);
								fclose(fp2);

								//add a terminating null character for last argument if needed
								if (cli_buffer[size2 - 1] != '\0')
								{
									cli_buffer[size2] = '\0';
									size2 += 1;
								}

								// CLI parse
								argv2[argc2] = bootpath;
								argc2++;

								// First argument is at the beginning of the file
								if (cli_buffer[0] != '\r' && cli_buffer[0] != '\n')
								{
									argv2[argc2] = cli_buffer;
									argc2++;
								}

								// Search for the others after each newline
								int i;

								for (i = 0; i < size2; i++)
								{
									if (cli_buffer[i] == '\r' || cli_buffer[i] == '\n')
									{
										cli_buffer[i] = '\0';
									}
									else if (cli_buffer[i - 1] == '\0')
									{
										argv2[argc2] = cli_buffer + i;
										argc2++;

										if (argc2 >= 1024)
										{
											break;
										}
									}
								}
							}

							DOLtoARAM(dol, argc2, argc2 == 0 ? NULL : argv2);
						}
						else
						{
							DOLtoARAM(dol, 0, NULL);
						}
					}

					//We shouldn't reach this point
					if (dol != NULL) free(dol);
					console(0, "Not a valid dol File! ");
					return HaltLoop();
				}

				fclose(fp);
			}
		}

		VIDEO_WaitVSync();
	}

	return 0;
}