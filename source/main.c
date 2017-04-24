#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include <fat.h>
#include <sdcard/gcsd.h>
#include "aram/sidestep.h"
#include "libpng/pngu/pngu.h"
#include "images.h"

#define CONFIGFILE "fat:/gbiloader/gbiloader.ini"
#define MAX_CONFIG_LINE 256

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

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
	VIDEO_WaitVSync();

	if (rmode->viTVMode&VI_NON_INTERLACE)
	{
		VIDEO_WaitVSync();
	}
}

static int initFAT()
{
	int slotagecko = 0;
	int i = 0;
	s32 ret, memsize, sectsize;

	for (i = 0; i < 10; i++)
	{
		ret = CARD_ProbeEx(CARD_SLOTA, &memsize, &sectsize);
		//printf("Ret: %d", ret);

		if (ret == CARD_ERROR_WRONGDEVICE)
		{
			slotagecko = 1;
			break;
		}
	}

	if (slotagecko)
	{
		// Memcard in SLOT B, SD gecko in SLOT A

		// This will ensure SD gecko is recognized if inserted or changed to another slot after GCMM is executed
		for (i = 0; i < 10; i++)
		{
			ret = CARD_Probe(CARD_SLOTA);

			if (ret == CARD_ERROR_WRONGDEVICE)
			{
				//printf ("SDGecko detected...\n\n");
				break;
			}
		}

		__io_gcsda.startup();

		if (!__io_gcsda.isInserted())
		{
			//printf ("No SD Gecko inserted! Using embedded config.\n\n");
			return 0;
		}

		if (!fatMountSimple("fat", &__io_gcsda))
		{
			//printf("Error Mounting SD fat! Using embedded config.\n\n");
			return 0;
		}
	}
	else
	{
		//Memcard in SLOT A, SD gecko in SLOT B

		//This will ensure SD gecko is recognized if inserted or changed to another slot after GCMM is executed
		for (i = 0; i < 10; i++)
		{
			ret = CARD_Probe(CARD_SLOTB);

			if (ret == CARD_ERROR_WRONGDEVICE)
			{
				break;
			}
		}

		__io_gcsdb.startup();

		if (!__io_gcsdb.isInserted())
		{
			//printf ("No SD Gecko inserted! Using default config.\n\n");
			return 0;
		}

		if (!fatMountSimple("fat", &__io_gcsdb))
		{
			//printf("Error Mounting SD fat! Using default config.\n\n");
			return 0;
		}
	}

	return 1;
}

//Config

char dol_gbi[] = "fat:/gbiloader/gbi.dol";
char dol_gbi_ll[] = "fat:/gbiloader/gbi-ll.dol";
char dol_gbi_ull[] = "fat:/gbiloader/gbi-ull.dol";

unsigned char *img_def = gbi_ll_png;
unsigned char *img_def_240 = gbi_ll_240_png;
char *dol_def = dol_gbi_ll;

int readparseconf(char * config)
{
	FILE * pFile;
	char mystring[MAX_CONFIG_LINE];

	pFile = fopen(config, "r");

	if (pFile == NULL)
	{
		printf("\x1b[4;6H Error opening %s\n", config);
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

			if (strncmp("DEFAULT=", mystring, 8) == 0)
			{
				if (strncmp(mystring + 8, "gbi-ull", 7) == 0)
				{
					dol_def = dol_gbi_ull;
					img_def = gbi_ull_png;
					img_def_240 = gbi_ull_240_png;
				}
				else if (strncmp(mystring + 8, "gbi-ll", 6) == 0)
				{
					dol_def = dol_gbi_ll;
					img_def = gbi_ll_png;
					img_def_240 = gbi_ll_240_png;
				}
				else if (strncmp(mystring + 8, "gbi", 3) == 0)
				{
					dol_def = dol_gbi;
					img_def = gbi_png;
					img_def_240 = gbi_240_png;
				}
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
		}

		fclose(pFile);
	}

	return 0;
}

void HaltLoop()
{
	while (1)
	{
		VIDEO_WaitVSync();
	}
}

void RenderPNG()
{
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

	PNGUPROP imgProp;
	IMGCTX ctx;

	unsigned char *img_splash;

	if (rmode->xfbHeight >= 480)
	{
		img_splash = img_def;
	}
	else
	{
		img_splash = img_def_240;
	}
	
	if (!(ctx = PNGU_SelectImageFromBuffer(img_splash)))
	{
		printf("\x1b[4;6H PNGU_SelectImageFromBuffer failed!\n");
	}
	else
	{
		if (PNGU_GetImageProperties(ctx, &imgProp) != PNGU_OK)
		{
			printf("\x1b[4;6H PNGU_GetImageProperties failed!\n");
		}
		else
		{
			if (PNGU_DECODE_TO_COORDS_YCbYCr(ctx, (rmode->fbWidth / 2) - (imgProp.imgWidth / 2), (rmode->xfbHeight / 2) - (imgProp.imgHeight / 2), imgProp.imgWidth, imgProp.imgHeight, rmode->fbWidth, rmode->xfbHeight, xfb) != PNGU_OK)
			{
				printf("\x1b[4;6H PNGU_DecodeToYCbYCr failed!\n");
				HaltLoop();
			}

			/*if (PNGU_DecodeToYCbYCr(ctx, imgProp.imgWidth, imgProp.imgHeight, xfb, 640 - imgProp.imgWidth) != PNGU_OK)
			{
				printf("\x1b[5;6H PNGU_DecodeToYCbYCr failed!\n");
			}*/
		}

		PNGU_ReleaseImageContext(ctx);
	}
}


int main(int argc, char *argv[])
{
	if (initFAT() == 0)
	{
		Initialise();
		printf("\x1b[4;6H Unable to access SD card.");
		HaltLoop();
	}

	if (readparseconf(CONFIGFILE) < 0)
	{
		Initialise();
		printf("\x1b[4;6H Unable to read config file.");
		HaltLoop();
	}

	Initialise();

	RenderPNG();

	bool showMenu = false;
	FILE * fp;
	int size;

	// Check if the user is trying to override the GBI version
	PAD_ScanPads();

	if (PAD_ButtonsHeld(0) & PAD_BUTTON_Y)
	{
		showMenu = true;

		printf("\x1b[4;6H gbiloader r1 by Adam Zey");

		printf("\x1b[6;6H To select the version of GBI to boot, please press one");
		printf("\x1b[7;6H of the following buttons. Your preference will be saved.");

		printf("\x1b[9;6H Press A for GBI");
		printf("\x1b[10;6H Press B for GBI-LL");
		printf("\x1b[11;6H Press X for GBI-ULL\n");
		//printf("\x1b[11;6H Press L for Swiss\n");
	}

	while (true)
	{
		if (showMenu)
		{
			PAD_ScanPads();

			int buttonsDown = PAD_ButtonsDown(0);

			if (buttonsDown & PAD_BUTTON_A)
			{
				dol_def = dol_gbi;
				img_def = gbi_png;
				img_def_240 = gbi_240_png;

				showMenu = false;
				RenderPNG();
			}
			else if (buttonsDown & PAD_BUTTON_B)
			{
				dol_def = dol_gbi_ll;
				img_def = gbi_ll_png;
				img_def_240 = gbi_ll_240_png;

				showMenu = false;
				RenderPNG();
			}
			else if (buttonsDown & PAD_BUTTON_X)
			{
				dol_def = dol_gbi_ull;
				img_def = gbi_ull_png;
				img_def_240 = gbi_ull_240_png;

				showMenu = false;
				RenderPNG();
			}
		}
		else
		{
			char bootpath[256];
			char clipath[256];
			strcpy(bootpath, dol_def);

			fp = fopen(bootpath, "rb");

			if (fp == NULL)
			{
				printf("\x1b[4;6H Failed to open %s. Check config file and folder structure.\n\n", dol_def);
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
					printf("\x1b[4;6H Not a valid dol File! ");
				}

				fclose(fp);
			}
		}

		VIDEO_WaitVSync();
	}

	return 0;
}
