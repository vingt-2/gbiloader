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

#define CONFIGFILE "fat:/gbiloader/gbiloader.ini"
#define MAX_CONFIG_LINE 256

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void Initialise()
{
	VIDEO_Init();
	PAD_Init();

	//rmode = VIDEO_GetPreferredMode(NULL);

	// Temporarily override with 480i (make this configurable later)
	rmode = &TVNtsc480Int;

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

char image_gbi[] = "fat:/gbiloader/gbi.png";
char image_gbi_ll[] = "fat:/gbiloader/gbi-ll.png";
char image_gbi_ull[] = "fat:/gbiloader/gbi-ull.png";
char def[256] = "fat:/gbi-ll.dol";
char vid_mode[256] = "auto";

int readparseconf(char * config)
{
	FILE * pFile;
	char mystring[MAX_CONFIG_LINE];

	pFile = fopen(config, "r");

	if (pFile == NULL)
	{
		printf("Error opening %s\n", config);
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
				strcpy(def, mystring + 8);
				def[strcspn(def, "\r\n")] = 0;
			}

			if (strncmp("VIDEO_MODE=", mystring, 11) == 0)
			{
				strcpy(vid_mode, mystring + 11);
				vid_mode[strcspn(vid_mode, "\r\n")] = 0;
			}
		}

		fclose(pFile);
	}

	return 0;
}

void waitA()
{
	printf("Press A to continue\n");

	while (1)
	{
		PAD_ScanPads();

		if (PAD_ButtonsDown(0) & PAD_BUTTON_A)
		{
			while (PAD_ButtonsHeld(0) & PAD_BUTTON_A)
			{
				PAD_ScanPads();
			}

			break;
		}

		VIDEO_WaitVSync();
	}
}


int main(int argc, char *argv[])
{
	Initialise();

	int have_sd = 0;

	while (1)
	{
		have_sd = initFAT();

		if (!have_sd)
		{
			while (1)
			{
				printf("\x1b[4;5H");
				printf("Couldn't mount SD Gecko. Insert one now and press A.\n");

				PAD_ScanPads();

				if (PAD_ButtonsDown(0) & PAD_BUTTON_A)
				{
					break;
				}

				VIDEO_WaitVSync();
			}
		}

		if (have_sd)
		{
			break;
		}
	}

	if (readparseconf(CONFIGFILE) < 0)
	{
		waitA();
	}

	char bootpath[256];
	char clipath[256];
	strcpy(bootpath, def);

	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

	PNGUPROP imgProp;
	IMGCTX ctx;

	if (!(ctx = PNGU_SelectImageFromDevice(image_gbi_ll))) // TODO: Make this dynamic
	{
		if (strlen(image_gbi_ll) > 0)
		{
			printf("PNGU_SelectFileFromDevice failed!\n");
		}
	}
	else
	{
		if (PNGU_GetImageProperties(ctx, &imgProp) != PNGU_OK)
		{
			printf("PNGU_GetImageProperties failed!\n");
		}
		else
		{
			if (PNGU_DecodeToYCbYCr(ctx, imgProp.imgWidth, imgProp.imgHeight, xfb, 640 - imgProp.imgWidth) != PNGU_OK)
			{
				printf("PNGU_DecodeToYCbYCr failed!\n");
			}
		}

		PNGU_ReleaseImageContext(ctx);
	}

	int boot;
	FILE * fp;
	int size;

	while (1)
	{
		boot = 0;
		
		//printf("\x1b[5;6H");

		//printf("\x1b[6;6H Default  : %s\n\n\n", (strlen(def_title) > 1) ? def_title : def);

		PAD_ScanPads();

		int buttonsDown = PAD_ButtonsDown(0);

		if (buttonsDown & PAD_TRIGGER_Z) {
			//strcpy(bootpath, buttonZT);
			boot = 1;
		}

		//if (timer >= 0) { if (difftime(now, boottime) >= timer) boot = 1; }

		if (boot)
		{
			fp = fopen(bootpath, "rb");

			if (fp == NULL)
			{
				printf("\x1b[22;5H                                                                   ");
				printf("\x1b[22;6H");
				printf("Can't open %s, booting default dol.", bootpath);

				strcpy(bootpath, def);

				fp = fopen(bootpath, "rb");

				if (fp == NULL)
				{
					if (strcmp(bootpath, "fat:/autoexec.dol") != 0)
					{
						printf("\x1b[23;5H                                                                   ");
						printf("\x1b[23;6H");
						printf("Can't open %s, booting autoexec.dol.", bootpath);
						sprintf(bootpath, "fat:/autoexec.dol");

						fp = fopen(bootpath, "rb");

						if (fp == NULL)
						{
							printf("\x1b[24;5H                                                                   ");
							printf("\x1b[25;5H                                                                   ");
							printf("\x1b[24;6H");
							printf("Failed to open autoexec.dol. It would be wise to have it at your\x1b[25;6Hsdcard root. Please, check your configuration file.\n\n");
							printf("\x1b[26;5H                                                                   ");
							printf("\x1b[27;5H                                                                   ");
							printf("\x1b[27;6H");
						}
					}
					else
					{
						printf("\x1b[23;5H                                                                   ");
						printf("\x1b[24;5H                                                                   ");
						printf("\x1b[23;6H");
						printf("Failed to open autoexec.dol. It would be wise to have it at your\x1b[24;6Hsdcard root. Please, check your configuration file.\n\n");
						printf("\x1b[25;5H                                                                   ");
						printf("\x1b[26;5H                                                                   ");
						printf("\x1b[26;6H");
					}
				}
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
					printf("Not a valid dol File! ");
				}

				fclose(fp);
			}
		}

		VIDEO_WaitVSync();
	}

	return 0;
}
