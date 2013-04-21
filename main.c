/**** LICENSE INFORMATION ****
Snap Detector
Snap finger detection freeware
Copyright (C) 2013  Quoc-Nam Dessoulles

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/


//////////////////////////////////////////////
/* ----------- Choose mode here ----------- */

#define EXPERIMENTAL_MODE 1
#define FINAL_MODE 2

#define CURRENT_MODE FINAL_MODE

//////////////////////////////////////////////


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <conio.h>
#include <FMOD.h>
#include <SDL.h>
#include <fftw3.h>

#define MAX_STRING              512
#define TIMESPACEMIN            300

#define BAND1                   350
#define BAND2                   1750
#define BAND3                   3000
#define BAND4                   5000

static FMOD_SYSTEM *mainFMODSystem = NULL;

static FMOD_SOUND* CreateSoundBuffer(unsigned int length, unsigned int samplingFreq);
static double* ProcessDFT(Sint8* pcmData, unsigned int sampleLength_PCM);
static int IsSnapshotEx(double *modules, unsigned int sampleLength_PCM, unsigned int samplingFreq, double *sum1_out, double *sum2_out, double *sum3_out, double *sum4_out, double *sum_out);
static int IsSnapshot(double *modules, unsigned int sampleLength_PCM, unsigned int samplingFreq);
static int WriteOutputFile(double *modules, const char fileName[], unsigned int sampleLength_PCM, unsigned int samplingRate);

//////////////////////////////////////////////
/* ---------- Experimental mode ----------- */
#if (CURRENT_MODE==EXPERIMENTAL_MODE)

#define SAMPLERATE              44100

#define SAMPLELENGTH            250
#define SAMPLELENGTH_PCM        (SAMPLERATE*SAMPLELENGTH/1000)

#define SCREENW                 640
#define SCREENH                 480

#define Exit(n) do { FMOD_System_Close(mainFMODSystem); FMOD_System_Release(mainFMODSystem); return n; } while (0)

static int ChooseInt(int min, int max);
static unsigned int ChooseDriver(void);
static int DisplayResults(double *modules, unsigned int screenW, unsigned int screenH);
static int RecAndWait(FMOD_SOUND *soundBuffer, unsigned int driverId);


int main(int argc, char *argv[])
{
    FMOD_SOUND *soundBuffer = NULL;
    int time;
    unsigned int driverId, soundLength = 0;

    Sint8 *pcmData1, *pcmData2;
    unsigned int len1, len2;

    double *modules = NULL;
    double sum, sum1, sum2, sum3, sum4;
    int isSnapshot;

    freopen("CON", "w", stdout);
    freopen("con", "w", stderr);

    printf("Initializing FMOD...\n");
    FMOD_System_Create(&mainFMODSystem);
    FMOD_System_Init(mainFMODSystem, 1, FMOD_INIT_NORMAL, NULL);

    printf("Creating sound buffer...\n");
    soundBuffer = CreateSoundBuffer(SAMPLELENGTH_PCM, SAMPLERATE);
    FMOD_Sound_GetLength(soundBuffer, &soundLength, FMOD_TIMEUNIT_MS);
    printf("Sound created successfully, length: %d ms.\n", soundLength);

    printf("Listing recording drivers...\n");
    driverId = ChooseDriver();
    printf("\rDriver #%d will be used.     \n", driverId);

    printf("Press <Enter> to start recording.\n");
    while (getch() != '\r');

    printf("Starting record...\n");
    RecAndWait(soundBuffer, driverId);
    printf("\rRecord ok. Extracting PCM data...       \n");
    FMOD_Sound_Lock(soundBuffer, 0, SAMPLELENGTH_PCM, (void**)&pcmData1, (void**)&pcmData2, &len1, &len2);

    time = SDL_GetTicks();
    printf("Analysing data...\n");
    modules = ProcessDFT(pcmData1, SAMPLELENGTH_PCM);
    FMOD_Sound_Unlock(soundBuffer, (void*)pcmData1, (void*)pcmData2, len1, len2);
    FMOD_Sound_Release(soundBuffer);

    printf("Summary:\n");
    isSnapshot = IsSnapshotEx(modules, SAMPLELENGTH_PCM, SAMPLERATE, &sum1, &sum2, &sum3, &sum4, &sum);
    printf("\t0.0-%.2f kHz: %.2f\n", BAND1/1000.0, sum1);
    printf("\t%.2f-%.2f kHz: %.2f\n", BAND1/1000.0, BAND2/1000.0, sum2);
    printf("\t%.2f-%.2f kHz: %.2f\n", BAND2/1000.0, BAND3/1000.0, sum3);
    printf("\t%.2f-%.2f kHz: %.2f\n", BAND3/1000.0, BAND4/1000.0, sum4);
    if (isSnapshot)
        printf("\t-> Finger snap detected!\n");
    else printf("\t-> No finger snap.\n");
    printf("Calculation time: %d ms.\n", SDL_GetTicks()-time);

    printf("Saving results...\n");
    if (!WriteOutputFile(modules, "out.txt", SAMPLELENGTH_PCM, SAMPLERATE))
    {
        free(modules);
        Exit(0);
    }
    printf("Saved in 'out.txt'.\n");

    DisplayResults(modules, SCREENW, SCREENH);
    free(modules);
    Exit(0);
}

static int ChooseInt(int min, int max)
{
    int done = 0, i = 0, number, n;
    char buffer[MAX_STRING] = "";

    while(!done)
    {
        do
        {
            buffer[i] = getch();
            buffer[i+1] = '\0';
        } while ((buffer[i] < '0' || buffer[i] > '9') && (buffer[i] != '-' || i > 0 || min >= 0) && buffer[i] != '\r' && buffer[i] != '\b');
        if (buffer[i] == '\r')
        {
            if (i > 0)
            {
                done = 1;
                number = strtol(buffer, NULL, 10);
            }
        }
        else if (buffer[i] == '\b')
        {
            if (i > 0)
            {
                printf("\b \b");
                i--;
            }
        }
        else
        {
            n = strtol(buffer, NULL, 10);
            if (n > max || n < min)
                buffer[i] = '\0';
            else
            {
                printf("%c", buffer[i]);
                i++;
            }
        }
    }

    return number;
}

static unsigned int ChooseDriver(void)
{
    int numDrivers, i;
    unsigned int driverId;
    char buffer[MAX_STRING] = "";

    FMOD_System_GetRecordNumDrivers(mainFMODSystem, &numDrivers);

    if (numDrivers < 0)
    {
        printf("\tNo recording driver found, exiting.\n");
        return 0;
    }
    else if (numDrivers == 1)
    {
        FMOD_System_GetRecordDriverInfo(mainFMODSystem, 0, buffer, MAX_STRING-1, NULL);
        printf("\t1 recording driver found :\n\t%s\n", buffer);
        driverId = 0;
    }
    else
    {
        printf("\t%d recording drivers found:\n", numDrivers);
        for (i=0 ; i < numDrivers ; i++)
        {
            FMOD_System_GetRecordDriverInfo(mainFMODSystem, i, buffer, MAX_STRING-1, NULL);
            printf("\t#%d - %s\n", i, buffer);
        }

        printf("Please choose a driver to use: ");
        driverId = ChooseInt(0, numDrivers-1);
    }

    return driverId;
}

static int DisplayResults(double *modules, unsigned int screenW, unsigned int screenH)
{
    int i, j, interval = (SAMPLELENGTH_PCM/2+1) / screenW;
    double sum, modMax=0;
    SDL_Surface *screen = NULL, *surf = NULL;
    SDL_Rect pos;
    SDL_Event event;

    printf("Creating display...\n");
    if (SDL_Init( SDL_INIT_VIDEO ) < 0)
    {
        printf( "Unable to init SDL: %s. Exiting.\n", SDL_GetError() );
        return 0;
    }
    if ( !(screen = SDL_SetVideoMode(screenW, screenH, 16, SDL_HWSURFACE|SDL_DOUBLEBUF)) )
    {
        printf("Unable to set %dx%d video: %s\n", screenW, screenH, SDL_GetError());
        SDL_Quit();
        return 0;
    }
    for (j=0 ; j < SAMPLELENGTH_PCM/2+1 ; j+=interval)
    {
        sum = 0;
        for (i=0 ; i < interval ; i++)
            sum += modules[i+j];
        if (sum/interval > modMax)
            modMax = sum/interval;
    }
    for (j=0 ; j < SAMPLELENGTH_PCM/2+1 ; j+=interval)
    {
        sum = 0;

        for (i=0 ; i < interval ; i++)
            sum += modules[i+j] / modMax;

        surf = SDL_CreateRGBSurface(SDL_HWSURFACE, 1, sum/interval * screenH, 32, 0,0,0,0);
        SDL_FillRect(surf, NULL, SDL_MapRGB(surf->format, 255,255,255));
        pos.x = j/interval;
        pos.y = screenH - surf->h;
        SDL_BlitSurface(surf, NULL, screen, &pos);
        SDL_FreeSurface(surf);
    }
    SDL_Flip(screen);

    printf("Press <Enter> (in the SDL window) to quit.\n");
    do
    {
        SDL_WaitEvent(&event);
    } while (event.type != SDL_QUIT &&
             (event.type != SDL_KEYDOWN || (event.key.keysym.sym != SDLK_RETURN && event.key.keysym.sym != SDLK_KP_ENTER)));

    SDL_Quit();
    return 1;
}

static int RecAndWait(FMOD_SOUND *soundBuffer, unsigned int driverId)
{
    unsigned int recPos, length;
    FMOD_Sound_GetLength(soundBuffer, &length, FMOD_TIMEUNIT_PCM);

    FMOD_System_RecordStart(mainFMODSystem, driverId, soundBuffer, 0);
    do
    {
        FMOD_System_GetRecordPosition(mainFMODSystem, driverId, &recPos);
        printf("\rPosition: %d / %d   ", recPos, length);
        SDL_Delay(10);
    } while (recPos < length);

    return 1;
}

#endif
//////////////////////////////////////////////



//////////////////////////////////////////////
/* ------------- Final mode ----------------*/
#if (CURRENT_MODE==FINAL_MODE)

#define _WIN32_IE 0x0600
#define _WIN32_WINNT 0x0502
#define WINVER 0x0502

#include <windows.h>
#include <commctrl.h>
#include <Windowsx.h>
#include <Shlobj.h>
#include "resource.h"

#ifndef BIF_NONEWFOLDERBUTTON
#define BIF_NONEWFOLDERBUTTON 0x00000200
#endif

#define SOUNDBUFFERLENGTH_FACTOR 10
#define SAMPLELENGTH_MIN 100
#define SAMPLELENGTH_MAX 1000
#define THRESHOLD_MIN 0.1
#define THRESHOLD_MAX 1.0


typedef struct
{
    char description[50];
    int (*function)(void* param);
} Action;

typedef struct
{
    unsigned int samplingFreq,
                 sampleLength,
                 driverId,
                 snapAction;
    double detectionThreshold;
    char file[MAX_PATH+1],
         launchDir[MAX_PATH+1],
         args[MAX_STRING];
} Settings;

static HINSTANCE mainInstance;
static HWND mainDlgWnd, runDlgWnd, optionsDlgWnd, aboutDlgWnd;
static Settings mainSettings;
static FMOD_SOUND *soundBuffer = NULL;
static double *modulesTab[10] = {NULL};
static int nbCurrentThreads = 0;
static BOOL isAnalysing = FALSE;
static SDL_TimerID mainTimerID = 0;
static int isBufferReady = -1;

static void CenterWindow(HWND hwnd1, HWND hwnd2);
static int CreateWndClass(WNDPROC wndProc, const char name[]);
static int LoadSettings(void);
static int SaveSettings(void);
int StopAnalysis(void);
int StartAnalysis(void);
int IsFileExecutable(const char *fileName);
int ToggleControlStatus(void);
int ToggleTaskBarIcon(int off);
int PrintTaskbarIconMenu(void);
Uint32 timerFunction(Uint32 interval, void *param);
int threadFunction(void *param);

int DblClickDesktop(void *param);

BOOL CALLBACK MinimizeWnd(HWND hwnd, LPARAM lParam);
int DisplayDesktop(void *param);

BOOL CALLBACK CountWnd(HWND hwnd, LPARAM lParam);
BOOL CALLBACK ChooseWnd(HWND hwnd, LPARAM lParam);
int AltTab(void *param);

int CtrlAltDel(void *param);
int Exec(void *param);
int WindowsTab(void *param);
int DoNothing(void *param);

static int IsNoisySnapshot(double *modules, double *noiseModules, unsigned int sampleLength_PCM, unsigned int samplingFreq);
Uint32 DecreaseNbSnapshots(Uint32 interval, void *param);


static Action tabActions[] =
{
    { "Double click on Desktop", DblClickDesktop },
    { "Back to desktop", DisplayDesktop },
    { "Switch windows", AltTab },
    { "Task Manager", CtrlAltDel },
    { "Execute...", Exec },
    { "Windows + Tab", WindowsTab },
    { "(no action)", DoNothing },
    { "", NULL }
};

static unsigned int tabFreq[] = {11025, 22050, 44100, 48000};

BOOL CALLBACK MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK OptionsDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK RunDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK AboutDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DFTWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    int numDrivers;

    FMOD_System_Create(&mainFMODSystem);
    FMOD_System_Init(mainFMODSystem, 1, FMOD_INIT_NORMAL, NULL);
    SDL_Init(SDL_INIT_TIMER);

    FMOD_System_GetRecordNumDrivers(mainFMODSystem, &numDrivers);
    if (numDrivers < 0)
        MessageBox(NULL, "Error: No recording driver was found on your computer!\nAs Snap Detector is totally useless without a recording system, it will now exit.", "Snap Detector", MB_OK | MB_ICONERROR);
    else
    {
        LoadSettings();

        mainInstance = hInstance;
        DialogBox(hInstance, "mainDlg", NULL, (DLGPROC)MainDlgProc);

        SaveSettings();
    }

    SDL_Quit();
    FMOD_System_Close(mainFMODSystem);
    FMOD_System_Release(mainFMODSystem);
    return 0;
}

BOOL CALLBACK MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg)
    {
        case WM_INITDIALOG:
        {
            RECT size;
            int width = 150;
            HWND listWnd = NULL;
            LVCOLUMN lvcol;
            LVITEM lvi;
            char buffer[MAX_STRING] = "";

            mainDlgWnd = hwndDlg;
            CenterWindow(mainDlgWnd, NULL);
            GetClientRect(mainDlgWnd, &size);

            listWnd = CreateWindowEx(
                 WS_EX_CLIENTEDGE,
                 WC_LISTVIEW,
                 "",
                 WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS | LVS_NOCOLUMNHEADER | LVS_SINGLESEL,
                 15,
                 15,
                 width,
                 size.bottom-30,
                 mainDlgWnd,
                 (HMENU) IDLV_OPTLIST,
                 GetModuleHandle(NULL),
                 NULL
            );
            ListView_SetExtendedListViewStyle(listWnd, LVS_EX_FULLROWSELECT);

            lvcol.mask = LVCF_WIDTH;
            lvcol.cx = width-6;
            ListView_InsertColumn(listWnd, 0, &lvcol);

            lvi.mask = LVIF_TEXT;
            lvi.iItem = 0;
            lvi.iSubItem = 0;
            lvi.pszText = buffer;
            strcpy(buffer, "Run");
            ListView_InsertItem(listWnd, &lvi);

            strcpy(buffer, "Options");
            lvi.iItem++;
            ListView_InsertItem(listWnd, &lvi);

            strcpy(buffer, "About");
            lvi.iItem++;
            ListView_InsertItem(listWnd, &lvi);

            CreateDialog(GetModuleHandle(NULL), "runDlg", mainDlgWnd, (DLGPROC)RunDlgProc);
            SetWindowPos(runDlgWnd, NULL, width+30, 15, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER);

            CreateDialog(GetModuleHandle(NULL), "optionsDlg", mainDlgWnd, (DLGPROC)OptionsDlgProc);
            SetWindowPos(optionsDlgWnd, NULL, width+30, 15, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

            CreateDialog(GetModuleHandle(NULL), "aboutDlg", mainDlgWnd, (DLGPROC)AboutDlgProc);
            SetWindowPos(aboutDlgWnd, NULL, width+30, 15, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

            SendMessage(hwndDlg, WM_SETICON, ICON_BIG, (LPARAM)LoadImage(mainInstance, "iconProject", IMAGE_ICON, 32, 32, 0));
            SendMessage(hwndDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadImage(mainInstance, "iconProject", IMAGE_ICON, 16, 16, 0));

            return FALSE;
        }

        case WM_NOTIFY:
        {
            NMHDR *nmhdr = (NMHDR*)lParam;
            switch (nmhdr->idFrom)
            {
                case IDLV_OPTLIST:
                {
                    if (nmhdr->code != NM_CLICK)
                       return FALSE;

                    NMITEMACTIVATE *nmia = (NMITEMACTIVATE*)lParam;
                    static int lastSelection = 0;

                    if (nmia->iItem < 0 || lastSelection == nmia->iItem)
                       return FALSE;

                    ShowWindow(runDlgWnd, SW_HIDE);
                    ShowWindow(optionsDlgWnd, SW_HIDE);
                    ShowWindow(aboutDlgWnd, SW_HIDE);

                    switch (nmia->iItem)
                    {
                           case 0:
                                ShowWindow(runDlgWnd, SW_SHOW);
                                break;
                           case 1:
                                ShowWindow(optionsDlgWnd, SW_SHOW);
                                break;
                           case 2:
                                ShowWindow(aboutDlgWnd, SW_SHOW);
                                break;
                           default:
                                break;
                    }

                    lastSelection = nmia->iItem;
                    return FALSE;
                }
            }

            return FALSE;
        }

        case WM_CLOSE:
            ShowWindow(hwndDlg, SW_MINIMIZE);
            Sleep(500);
            ShowWindow(hwndDlg, SW_HIDE);
            ToggleTaskBarIcon(0);
            return TRUE;

        case WM_DESTROY:
            if (!IsWindowVisible(hwndDlg))
                ToggleTaskBarIcon(1);
            return TRUE;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDP_APPLY:
                case IDDP_OK:
                {
                    char buffer[MAX_STRING] = "";
                    int sampleLength = 0,
                        wasAnalysing;
                    double threshold = 0;

                    if (HIWORD(wParam) != BN_CLICKED)
                        return FALSE;

                    if ( (wasAnalysing = isAnalysing) );
                        StopAnalysis();

                    mainSettings.driverId = ComboBox_GetCurSel(GetDlgItem(optionsDlgWnd, IDCB_DRIVER));
                    mainSettings.samplingFreq = ComboBox_GetCurSel(GetDlgItem(optionsDlgWnd, IDCB_SAMPLEFREQ));
                    mainSettings.snapAction = ComboBox_GetCurSel(GetDlgItem(optionsDlgWnd, IDCB_ACTION));

                    Edit_GetText(GetDlgItem(optionsDlgWnd, IDET_SAMPLELENGTH), buffer, MAX_STRING-1);
                    sampleLength = strtol(buffer, NULL, 10);
                    if (sampleLength < SAMPLELENGTH_MIN || sampleLength > SAMPLELENGTH_MAX)
                    {
                        char buffer[MAX_STRING];
                        sprintf(buffer, "Please choose a sample length between %d and %d ms.", SAMPLELENGTH_MIN, SAMPLELENGTH_MAX);
                        MessageBox(hwndDlg, buffer, "Warning", MB_OK | MB_ICONWARNING);
                    }
                    else mainSettings.sampleLength = sampleLength;
                    sprintf(buffer, "%d", mainSettings.sampleLength);
                    Edit_SetText(GetDlgItem(optionsDlgWnd, IDET_SAMPLELENGTH), buffer);
                    SendMessage(GetDlgItem(optionsDlgWnd, IDTB_SAMPLELENGTH), TBM_SETPOS, TRUE, mainSettings.sampleLength);

                    Edit_GetText(GetDlgItem(optionsDlgWnd, IDET_THRESHOLD), buffer, MAX_STRING-1);
                    threshold = strtod(buffer, NULL);
                    if (threshold < THRESHOLD_MIN || threshold > THRESHOLD_MAX)
                    {
                        char buffer[MAX_STRING];
                        sprintf(buffer, "Please choose a detection threshold between %.2f and %.2f.", THRESHOLD_MIN, THRESHOLD_MAX);
                        MessageBox(hwndDlg, buffer, "Warning", MB_OK | MB_ICONWARNING);
                    }
                    else mainSettings.detectionThreshold = threshold;
                    sprintf(buffer, "%.2f", mainSettings.detectionThreshold);
                    Edit_SetText(GetDlgItem(optionsDlgWnd, IDET_THRESHOLD), buffer);
                    SendMessage(GetDlgItem(optionsDlgWnd, IDTB_THRESHOLD), TBM_SETPOS, TRUE, 100 * (mainSettings.detectionThreshold - THRESHOLD_MIN) / (THRESHOLD_MAX - THRESHOLD_MIN));

                    Edit_GetText(GetDlgItem(optionsDlgWnd, IDET_FILE), mainSettings.file, MAX_PATH);
                    Edit_GetText(GetDlgItem(optionsDlgWnd, IDET_LAUNCHDIR), mainSettings.launchDir, MAX_PATH);
                    Edit_GetText(GetDlgItem(optionsDlgWnd, IDET_ARGS), mainSettings.args, MAX_STRING-1);

                    if (wasAnalysing)
                        StartAnalysis();

                    if (LOWORD(wParam) == IDDP_OK)
                        SendMessage(hwndDlg, WM_CLOSE, 0, 0);

                    return TRUE;
                }
                case IDP_CANCEL:
                    if (HIWORD(wParam) != BN_CLICKED)
                        return FALSE;

                    SendMessage(hwndDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                case IDM_OPEN:
                case IDTBI_ICON:
                    if (lParam == WM_LBUTTONDBLCLK || wParam == IDM_OPEN)
                    {
                        ToggleTaskBarIcon(1);
                        ShowWindow(hwndDlg, SW_SHOW);
                        ShowWindow(hwndDlg, SW_RESTORE);
                    }
                    else if (lParam == WM_RBUTTONDOWN)
                        PrintTaskbarIconMenu();

                    return TRUE;
                case IDM_TOGGLESTATUS:
                    SendMessage(runDlgWnd, WM_COMMAND, MAKEWPARAM(IDP_TOGGLESTATUS, BN_CLICKED), (LPARAM)GetDlgItem(runDlgWnd, IDP_TOGGLESTATUS));
                    return TRUE;
                case IDM_EXIT:
                    SendMessage(runDlgWnd, WM_COMMAND, MAKEWPARAM(IDP_EXIT, BN_CLICKED), (LPARAM)GetDlgItem(runDlgWnd, IDP_EXIT));
                    return TRUE;
            }
    }

    return FALSE;
}

BOOL CALLBACK OptionsDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            int numDrivers, i;
            char buffer[MAX_STRING] = "";
            HWND comboBoxWnd = GetDlgItem(hwndDlg, IDCB_DRIVER);
            optionsDlgWnd = hwndDlg;

            FMOD_System_GetRecordNumDrivers(mainFMODSystem, &numDrivers);
            for (i=0 ; i < numDrivers ; i++)
            {
                FMOD_System_GetRecordDriverInfo(mainFMODSystem, i, buffer, MAX_STRING-1, NULL);
                ComboBox_AddString(comboBoxWnd, buffer);
            }
            ComboBox_SetCurSel(comboBoxWnd, 0);

            comboBoxWnd = GetDlgItem(hwndDlg, IDCB_SAMPLEFREQ);
            for (i=0 ; tabFreq[i] > 0; i++)
            {
                sprintf(buffer, "%.3f kHz", tabFreq[i] / 1000.0);
                ComboBox_AddString(comboBoxWnd, buffer);
            }
            ComboBox_SetCurSel(comboBoxWnd, mainSettings.samplingFreq);

            comboBoxWnd = GetDlgItem(hwndDlg, IDCB_ACTION);
            for (i=0 ; tabActions[i].function ; i++)
                ComboBox_AddString(comboBoxWnd, tabActions[i].description);
            ComboBox_SetCurSel(comboBoxWnd, mainSettings.snapAction);

            sprintf(buffer, "%d", mainSettings.sampleLength);
            Edit_SetText(GetDlgItem(hwndDlg, IDET_SAMPLELENGTH), buffer);
            SendMessage(GetDlgItem(hwndDlg, IDTB_SAMPLELENGTH), TBM_SETRANGE, FALSE, MAKELPARAM(SAMPLELENGTH_MIN, SAMPLELENGTH_MAX));
            SendMessage(GetDlgItem(hwndDlg, IDTB_SAMPLELENGTH), TBM_SETPOS, TRUE, mainSettings.sampleLength);

            sprintf(buffer, "%.2f", mainSettings.detectionThreshold);
            Edit_SetText(GetDlgItem(hwndDlg, IDET_THRESHOLD), buffer);
            SendMessage(GetDlgItem(hwndDlg, IDTB_THRESHOLD), TBM_SETRANGE, FALSE, MAKELPARAM(0, 100));
            SendMessage(GetDlgItem(hwndDlg, IDTB_THRESHOLD), TBM_SETPOS, TRUE, 100 * (mainSettings.detectionThreshold - THRESHOLD_MIN) / (THRESHOLD_MAX - THRESHOLD_MIN));

            Edit_SetText(GetDlgItem(hwndDlg, IDET_FILE), mainSettings.file);
            Edit_SetText(GetDlgItem(hwndDlg, IDET_LAUNCHDIR), mainSettings.launchDir);
            Edit_SetText(GetDlgItem(hwndDlg, IDET_ARGS), mainSettings.args);

            ToggleControlStatus();

            return TRUE;
        }

        case WM_HSCROLL:
        {
            char buffer[MAX_STRING] = "";

            if (LOWORD(wParam) == TB_THUMBTRACK)
            {
                if (lParam == (LPARAM)GetDlgItem(hwndDlg, IDTB_SAMPLELENGTH))
                {
                    sprintf(buffer, "%d", HIWORD(wParam));
                    Edit_SetText(GetDlgItem(hwndDlg, IDET_SAMPLELENGTH), buffer);
                }
                else if (lParam == (LPARAM)GetDlgItem(hwndDlg, IDTB_THRESHOLD))
                {
                    sprintf(buffer, "%.2f", (HIWORD(wParam) / 100.0 + THRESHOLD_MIN) * (THRESHOLD_MAX - THRESHOLD_MIN));
                    Edit_SetText(GetDlgItem(hwndDlg, IDET_THRESHOLD), buffer);
                }
            }

            return TRUE;
        }

        case WM_COMMAND:
        {
            char buffer[MAX_STRING] = "";

            switch (LOWORD(wParam))
            {
                case IDCB_ACTION:
                    if (HIWORD(wParam) == CBN_SELCHANGE)
                        ToggleControlStatus();
                    return TRUE;
                case IDET_FILE:
                    if (HIWORD(wParam) == EN_CHANGE)
                        ToggleControlStatus();
                    return TRUE;
                case IDET_SAMPLELENGTH:
                    if (HIWORD(wParam) == EN_KILLFOCUS)
                    {
                        Edit_GetText((HWND)lParam, buffer, MAX_STRING-1);
                        SendMessage(GetDlgItem(hwndDlg, IDTB_SAMPLELENGTH), TBM_SETPOS, TRUE, strtol(buffer, NULL, 10));
                    }
                    return TRUE;
                case IDET_THRESHOLD:
                    if (HIWORD(wParam) == EN_KILLFOCUS)
                    {
                        Edit_GetText((HWND)lParam, buffer, MAX_STRING-1);
                        SendMessage(GetDlgItem(hwndDlg, IDTB_THRESHOLD), TBM_SETPOS, TRUE, 100 * (strtod(buffer, NULL) - THRESHOLD_MIN) / (THRESHOLD_MAX - THRESHOLD_MIN));
                    }
                    return TRUE;
                case IDP_FILE:
                {
                    OPENFILENAME ofn;
                    char buffer[MAX_PATH+1] = "",
                         curDir[MAX_PATH+1] = "";

                    if (HIWORD(wParam) != BN_CLICKED)
                        return FALSE;

                    memset(&ofn, 0, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = mainDlgWnd;
                    ofn.lpstrFile = buffer;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.lpstrFilter = "All files\0*.*\0Executable files (*.exe)\0*.exe\0\0";
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    GetCurrentDirectory(MAX_PATH, curDir);
                    if (GetOpenFileName(&ofn))
                        Edit_SetText(GetDlgItem(hwndDlg, IDET_FILE), buffer);
                    SetCurrentDirectory(curDir);

                    return TRUE;
                }
                case IDP_LAUNCHDIR:
                {
                    BROWSEINFO bwi;
                    char buffer[MAX_PATH+1] = "",
                         curDir[MAX_PATH+1] = "";
                    ITEMIDLIST *pid;

                    if (HIWORD(wParam) != BN_CLICKED)
                        return FALSE;

                    memset(&bwi, 0, sizeof(BROWSEINFO));
                    bwi.hwndOwner = mainDlgWnd;
                    bwi.pszDisplayName = buffer;
                    bwi.lpszTitle = "Please choose a folder.";
                    bwi.ulFlags = BIF_NONEWFOLDERBUTTON;

                    GetCurrentDirectory(MAX_PATH, curDir);
                    if ( (pid = SHBrowseForFolder(&bwi)) )
                    {
                        SHGetPathFromIDList(pid, buffer);
                        Edit_SetText(GetDlgItem(hwndDlg, IDET_LAUNCHDIR), buffer);
                    }
                    SetCurrentDirectory(curDir);

                    return TRUE;
                }
            }
        }
        case WM_DESTROY:
            return TRUE;
    }

    return FALSE;
}

BOOL CALLBACK RunDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            HWND hwnd;
            runDlgWnd = hwndDlg;

            CreateWndClass(DFTWndProc, "DFTWindow");
            hwnd = CreateWindowEx(
                  0,
                  "DFTWindow",
                  "",
                  WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                  20, 140, 380, 130,
                  hwndDlg,
                  (HMENU) ID_DFTWND,
                  mainInstance,
                  NULL
               );
            SetWindowPos(hwnd, HWND_TOP, 0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);

            StartAnalysis();
            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDP_TOGGLESTATUS:
                    if (HIWORD(wParam) != BN_CLICKED)
                        return FALSE;

                    if (isAnalysing)
                        StopAnalysis();
                    else StartAnalysis();

                    Button_SetText(GetDlgItem(hwndDlg, IDP_TOGGLESTATUS), isAnalysing ? "Stop" : "Start");
                    return TRUE;
                case IDP_EXIT:
                    if (HIWORD(wParam) != BN_CLICKED)
                        return FALSE;

                    DestroyWindow(mainDlgWnd);
                    return TRUE;
            }

        case WM_DESTROY:
            StopAnalysis();
            return TRUE;
    }

    return FALSE;
}

BOOL CALLBACK AboutDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            HICON iconAuthor;
            aboutDlgWnd = hwndDlg;

            iconAuthor = LoadImage(mainInstance, "iconAuthor", IMAGE_ICON, 64, 64, 0);
            Static_SetIcon(GetDlgItem(hwndDlg, IDI_AUTHOR), iconAuthor);

            return TRUE;
        }

        case WM_DESTROY:
            return TRUE;
    }

    return FALSE;
}

int ToggleTaskBarIcon(int off)
{
    NOTIFYICONDATA nid;
    static HICON iconProject = NULL;

    if (!iconProject)
        iconProject = LoadImage(mainInstance, "iconProject", IMAGE_ICON, 16, 16, 0);

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = mainDlgWnd;
    nid.uID = IDTBI_ICON;
    nid.uCallbackMessage = WM_COMMAND;
    nid.hIcon = iconProject;
    strcpy(nid.szTip, "Snap Detector");

    if (!off)
    {
        nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        Shell_NotifyIcon(NIM_ADD, &nid);
    }
    else
    {
        nid.uFlags = 0;
        Shell_NotifyIcon(NIM_DELETE, &nid);
    }

    return 1;
}

int PrintTaskbarIconMenu(void)
{
    HMENU hMenu;
    POINT pt;
    GetCursorPos(&pt);

    hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, IDM_OPEN, "Open settings window");
    AppendMenu(hMenu, MF_STRING, IDM_TOGGLESTATUS, isAnalysing ? "Stop" : "Start");
    AppendMenu(hMenu, MF_STRING, IDM_EXIT, "Exit");

    SetMenuDefaultItem(hMenu, IDM_OPEN, FALSE);

    SetForegroundWindow(mainDlgWnd);
    int result = TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, mainDlgWnd, NULL);
    PostMessage(mainDlgWnd, WM_NULL, 0, 0);

    if (result)
        return 1;
    else return 0;
}

Uint32 timerFunction(Uint32 interval, void *param)
{
    SDL_CreateThread(threadFunction, param);

    return interval;
}
int threadFunction(void *param)
{
    HWND hwnd = (HWND)param;
    Sint8 *pcmData1, *pcmData2, *pcmData;
    unsigned int len1, len2, recPos;
    unsigned int sampleLength_PCM = mainSettings.sampleLength * tabFreq[mainSettings.samplingFreq] / 1000;
    unsigned int soundBufferLength_PCM;
    int i;
    double *modules = NULL,
           *noiseModules = NULL;

    nbCurrentThreads++;

    FMOD_Sound_GetLength(soundBuffer, &soundBufferLength_PCM, FMOD_TIMEUNIT_PCM);
    FMOD_System_GetRecordPosition(mainFMODSystem, mainSettings.driverId, &recPos);

    pcmData = malloc(soundBufferLength_PCM);
    memset(pcmData, 0, soundBufferLength_PCM);
    FMOD_Sound_Lock(soundBuffer, recPos+1, soundBufferLength_PCM - (recPos+1), (void**)&pcmData1, (void**)&pcmData2, &len1, &len2);
    memcpy(pcmData, pcmData1, len1);
    FMOD_Sound_Unlock(soundBuffer, (void*)pcmData1, (void*)pcmData2, len1, len2);
    if (recPos >= sampleLength_PCM)
    {
        FMOD_Sound_Lock(soundBuffer, 0, recPos - sampleLength_PCM, (void**)&pcmData1, (void**)&pcmData2, &len1, &len2);
        memcpy(pcmData + soundBufferLength_PCM - (recPos+1), pcmData1, len1);
        FMOD_Sound_Unlock(soundBuffer, (void*)pcmData1, (void*)pcmData2, len1, len2);
    }
    noiseModules = ProcessDFT(pcmData, soundBufferLength_PCM);

    memset(pcmData, 0, soundBufferLength_PCM);
    if (recPos < sampleLength_PCM)
    {
        recPos += soundBufferLength_PCM - sampleLength_PCM;
        FMOD_Sound_Lock(soundBuffer, recPos, soundBufferLength_PCM - recPos, (void**)&pcmData1, (void**)&pcmData2, &len1, &len2);
        memcpy(pcmData, pcmData1, len1);
        FMOD_Sound_Unlock(soundBuffer, (void*)pcmData1, (void*)pcmData2, len1, len2);
        FMOD_Sound_Lock(soundBuffer, 0, sampleLength_PCM - (soundBufferLength_PCM - recPos), (void**)&pcmData1, (void**)&pcmData2, &len1, &len2);
        memcpy(pcmData + soundBufferLength_PCM - recPos, pcmData1, len1);
        FMOD_Sound_Unlock(soundBuffer, (void*)pcmData1, (void*)pcmData2, len1, len2);
    }
    else
    {
        recPos -= sampleLength_PCM;
        FMOD_Sound_Lock(soundBuffer, recPos, sampleLength_PCM, (void**)&pcmData1, (void**)&pcmData2, &len1, &len2);
        memcpy(pcmData, pcmData1, len1);
        FMOD_Sound_Unlock(soundBuffer, (void*)pcmData1, (void*)pcmData2, len1, len2);
    }
    modules = ProcessDFT(pcmData, soundBufferLength_PCM);
    free(pcmData);

    if (IsNoisySnapshot(modules, noiseModules, soundBufferLength_PCM, tabFreq[mainSettings.samplingFreq]))
        tabActions[mainSettings.snapAction].function(NULL);

    for (i=0 ; i < 10 && modulesTab[i] ; i++);
    if (i < 10 && IsWindowVisible(GetParent(hwnd)))
        modulesTab[i] = modules;
    else free(modules);
    free(noiseModules);

    if (IsWindowVisible(GetParent(hwnd)))
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE);

    nbCurrentThreads--;
    return 1;
}
LRESULT CALLBACK DFTWndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    unsigned int soundBufferLength_PCM;
    FMOD_Sound_GetLength(soundBuffer, &soundBufferLength_PCM, FMOD_TIMEUNIT_PCM);

    switch (msg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT paintst;
            HDC hdc;
            RECT wndSize, rect;
            LOGBRUSH lb;
            HGDIOBJ hPen = NULL, hPenOld;
            HBRUSH greenBrush = NULL, yellowBrush = NULL;
            int i, j, interval;
            double sum, modMax=0;
            double sum1 = 0, sum2 = 0,
                   sum3 = 0, sum4 = 0;
            int freq1 = BAND1*soundBufferLength_PCM/tabFreq[mainSettings.samplingFreq],
                freq2 = BAND2*soundBufferLength_PCM/tabFreq[mainSettings.samplingFreq],
                freq3 = BAND3*soundBufferLength_PCM/tabFreq[mainSettings.samplingFreq],
                freq4 = BAND4*soundBufferLength_PCM/tabFreq[mainSettings.samplingFreq],
                maxFreq = tabFreq[mainSettings.samplingFreq]/2 + 1;

            GetClientRect(hwnd, &wndSize);
            interval = (soundBufferLength_PCM/2+1) / wndSize.right;
            hdc = BeginPaint(hwnd, &paintst);

            lb.lbStyle = BS_SOLID;
            lb.lbColor = RGB(0,0,0);
            lb.lbHatch = 0;
            hPen = ExtCreatePen(PS_COSMETIC | PS_SOLID, 1, &lb, 0, NULL);
            hPenOld = SelectObject(hdc, hPen);

            if (modulesTab[0])
            {
                for (j=0 ; j < soundBufferLength_PCM/2+1 ; j+=interval)
                {
                    sum = 0;
                    for (i=0 ; i < interval ; i++)
                        sum += modulesTab[0][i+j] / interval;
                    if (sum > modMax)
                        modMax = sum;
                }

                for (i=0 ; i < freq1 ; i++)
                    sum1 += modulesTab[0][i]/freq1;
                for (; i < freq2 ; i++)
                    sum2 += modulesTab[0][i]/(freq2-freq1);
                for (; i < freq3 ; i++)
                    sum3 += modulesTab[0][i]/(freq3-freq2);
                for (; i < freq4 ; i++)
                    sum4 += modulesTab[0][i]/(freq4-freq3);

                FillRect(hdc, &wndSize, GetStockObject(LTGRAY_BRUSH));
                greenBrush = CreateSolidBrush(RGB(0,127,0));
                yellowBrush = CreateSolidBrush(RGB(127,127,0));

                rect.left = 0; rect.top = wndSize.bottom * (1-sum1/modMax);
                rect.right = wndSize.right * BAND1/maxFreq; rect.bottom = wndSize.bottom;
                FillRect(hdc, &rect, greenBrush);

                rect.left = wndSize.right * BAND1/maxFreq; rect.top = wndSize.bottom * (1-sum2/modMax);
                rect.right = wndSize.right * BAND2/maxFreq; rect.bottom = wndSize.bottom;
                FillRect(hdc, &rect, greenBrush);

                rect.left = wndSize.right * BAND2/maxFreq; rect.top = wndSize.bottom * (1-sum3/modMax);
                rect.right = wndSize.right * BAND3/maxFreq; rect.bottom = wndSize.bottom;
                FillRect(hdc, &rect, yellowBrush);

                rect.left = wndSize.right * BAND3/maxFreq; rect.top = wndSize.bottom * (1-sum4/modMax);
                rect.right = wndSize.right * BAND4/maxFreq; rect.bottom = wndSize.bottom;
                FillRect(hdc, &rect, greenBrush);

                DeleteObject(greenBrush);
                DeleteObject(yellowBrush);

                for (j=0 ; j < soundBufferLength_PCM/2+1 ; j+=interval)
                {
                    sum = 0;
                    for (i=0 ; i < interval ; i++)
                        sum += modulesTab[0][i+j] / interval;

                    MoveToEx(hdc, j/interval, wndSize.bottom, NULL);
                    LineTo(hdc, j/interval, wndSize.bottom * (1 - sum/modMax));
                }

                free(modulesTab[0]);
                for (j=1 ; j < 10 ; j++)
                    modulesTab[j-1] = modulesTab[j];
                modulesTab[9] = NULL;

                SelectObject(hdc, hPenOld);
                DeleteObject(hPen);
                lb.lbColor = RGB(255,0,0);
                hPen = ExtCreatePen(PS_COSMETIC | PS_SOLID, 1, &lb, 0, NULL);
                SelectObject(hdc, hPen);

                MoveToEx(hdc, 0, wndSize.bottom * (1-mainSettings.detectionThreshold/modMax), NULL);
                LineTo(hdc, wndSize.right, wndSize.bottom * (1-mainSettings.detectionThreshold/modMax));
            }

            SelectObject(hdc, hPenOld);
            DeleteObject(hPen);

            EndPaint(hwnd, &paintst);
            return TRUE;
        }
        default:
            return DefWindowProc (hwnd, msg, wParam, lParam);
    }

    return FALSE;
}


static void CenterWindow(HWND hwnd1, HWND hwnd2)
{
     RECT screen;
     screen.right = GetSystemMetrics(SM_CXFULLSCREEN);
     screen.bottom = GetSystemMetrics(SM_CYFULLSCREEN);
     screen.top = 0;
     screen.left = 0;

     if (hwnd2 != NULL)
     {
          RECT rect;
          GetWindowRect(hwnd2, &rect);
          GetClientRect(hwnd2, &screen);
          screen.top = rect.top;
          screen.left = rect.left;
     }

     RECT wndRect;
     GetWindowRect(hwnd1, &wndRect);

     SetWindowPos(hwnd1, NULL, (screen.right - (wndRect.right - wndRect.left)) / 2 + screen.left, (screen.bottom - (wndRect.bottom - wndRect.top)) / 2 + screen.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

     return;
}

static int CreateWndClass(WNDPROC wndProc, const char name[])
{
    WNDCLASSEX wincl;
    wincl.cbSize = sizeof(WNDCLASSEX);
    wincl.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wincl.lpfnWndProc = wndProc;
    wincl.cbClsExtra = 0;
    wincl.cbWndExtra = 0;
    wincl.hInstance = mainInstance;
    wincl.hIcon = LoadIcon(NULL,IDI_APPLICATION);
    wincl.hCursor = LoadCursor(NULL, IDC_ARROW);
    wincl.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    wincl.lpszMenuName = NULL;
    wincl.lpszClassName = (LPSTR)name;
    wincl.hIconSm = LoadIcon(NULL,IDI_APPLICATION);

    return RegisterClassEx (&wincl);
}

static int LoadSettings(void)
{
    FILE *settingsFile;

    if ( (settingsFile = fopen("param.cf", "rb")) )
    {
        if (fread(&mainSettings, 1, sizeof(Settings),settingsFile) == sizeof(Settings))
        {
            fclose(settingsFile);
            return 1;
        }

        fclose(settingsFile);
    }

    mainSettings.driverId = 0;
    mainSettings.sampleLength = 250;
    mainSettings.samplingFreq = 2;
    mainSettings.snapAction = 0;

    mainSettings.detectionThreshold = 0.5;

    mainSettings.file[0] = '\0';
    mainSettings.launchDir[0] = '\0';
    mainSettings.args[0] = '\0';

    return 0;
}

static int SaveSettings(void)
{
    FILE *settingsFile;

    if ( (settingsFile = fopen("param.cf", "wb")) )
    {
        if (fwrite(&mainSettings, 1, sizeof(Settings), settingsFile) == sizeof(Settings))
        {
            fclose(settingsFile);
            return 1;
        }

        fclose(settingsFile);
    }

    return 0;
}

int StartAnalysis(void)
{
    unsigned int soundBufferLength_PCM = mainSettings.sampleLength*SOUNDBUFFERLENGTH_FACTOR * tabFreq[mainSettings.samplingFreq] / 1000;
    HWND dftDisplayWnd = GetDlgItem(runDlgWnd, ID_DFTWND);
    static HICON iconOK = NULL;
    HWND buttonWnd = GetDlgItem(runDlgWnd, IDP_TOGGLESTATUS);

    if (isAnalysing)
        return 0;

    if (!iconOK)
        iconOK = LoadImage(mainInstance, "iconOK", IMAGE_ICON, 16, 16, 0);

    Button_Enable(buttonWnd, FALSE);

    isBufferReady = -1;
    soundBuffer = CreateSoundBuffer(soundBufferLength_PCM, tabFreq[mainSettings.samplingFreq]);
    FMOD_System_RecordStart(mainFMODSystem, mainSettings.driverId, soundBuffer, 1);
    Sleep(mainSettings.sampleLength - 100);

    mainTimerID = SDL_AddTimer(100, timerFunction, (void*)dftDisplayWnd);

    Static_SetIcon(GetDlgItem(runDlgWnd, IDI_STATUS), iconOK);
    Static_SetText(GetDlgItem(runDlgWnd, IDT_STATUS), "Snap Detector is working well!");

    Button_Enable(buttonWnd, TRUE);
    Button_SetText(buttonWnd, "Stop");

    isAnalysing = TRUE;
    return 1;
}

int StopAnalysis(void)
{
    static HICON iconStop = NULL;
    HWND buttonWnd = GetDlgItem(runDlgWnd, IDP_TOGGLESTATUS);

    if (!isAnalysing)
        return 0;

    if (!iconStop)
        iconStop = LoadImage(mainInstance, "iconStop", IMAGE_ICON, 16, 16, 0);

    Button_Enable(buttonWnd, FALSE);

    if (mainTimerID > 0)
    {
        SDL_RemoveTimer(mainTimerID);
        mainTimerID = 0;
        while (nbCurrentThreads > 0)
            Sleep(10);
    }

    FMOD_System_RecordStop(mainFMODSystem, mainSettings.driverId);
    FMOD_Sound_Release(soundBuffer);

    Static_SetIcon(GetDlgItem(runDlgWnd, IDI_STATUS), iconStop);
    Static_SetText(GetDlgItem(runDlgWnd, IDT_STATUS), "Snap Detector is sleeping...");

    Button_Enable(buttonWnd, TRUE);
    Button_SetText(buttonWnd, "Start");

    isAnalysing = FALSE;
    return 1;
}

int IsFileExecutable(const char *fileName)
{
    int l = strlen(fileName);
    return l > 4 && !strcmp(fileName + l - 4, ".exe");
}

int ToggleControlStatus(void)
{
    int isExec,
        curSel = ComboBox_GetCurSel(GetDlgItem(optionsDlgWnd, IDCB_ACTION));
    char buffer[MAX_STRING] = "";

    Edit_GetText(GetDlgItem(optionsDlgWnd, IDET_FILE), buffer, MAX_STRING-1);

    Edit_Enable(GetDlgItem(optionsDlgWnd, IDET_FILE), curSel == 4);
    Button_Enable(GetDlgItem(optionsDlgWnd, IDP_FILE), curSel == 4);
    Static_Enable(GetDlgItem(optionsDlgWnd, IDT_FILE), curSel == 4);

    isExec = IsFileExecutable(buffer);
    Edit_Enable(GetDlgItem(optionsDlgWnd, IDET_LAUNCHDIR), isExec && curSel == 4);
    Static_Enable(GetDlgItem(optionsDlgWnd, IDT_LAUNCHDIR), isExec && curSel == 4);
    Button_Enable(GetDlgItem(optionsDlgWnd, IDP_LAUNCHDIR), isExec && curSel == 4);
    Edit_Enable(GetDlgItem(optionsDlgWnd, IDET_ARGS), isExec && curSel == 4);
    Static_Enable(GetDlgItem(optionsDlgWnd, IDT_ARGS), isExec && curSel == 4);

    return 1;
}


int DblClickDesktop(void *param)
{
    HWND desktopWnd = GetShellWindow(),
         hDefView = FindWindowEx(desktopWnd, NULL, "SHELLDLL_DefView", NULL),
         folderView = FindWindowEx(hDefView, NULL, "SysListView32", NULL);
    SendMessage(folderView, WM_LBUTTONDBLCLK, 0, 0);
    return 1;
}

BOOL CALLBACK MinimizeWnd(HWND hwnd, LPARAM lParam)
{
    WINDOWINFO wndInfo;
    wndInfo.cbSize = sizeof(wndInfo);
    GetWindowInfo(hwnd, &wndInfo);
    if ((wndInfo.dwStyle & WS_CAPTION) == WS_CAPTION)
        ShowWindow(hwnd, SW_FORCEMINIMIZE);
    return TRUE;
}
int DisplayDesktop(void *param)
{
    EnumWindows(MinimizeWnd, 0);
    return 1;
}

static int curWndNumber,
           nbWindows = 0;
BOOL CALLBACK CountWnd(HWND hwnd, LPARAM lParam)
{
    WINDOWINFO wndInfo;
    wndInfo.cbSize = sizeof(wndInfo);
    GetWindowInfo(hwnd, &wndInfo);
    if (IsWindowVisible(hwnd) && (wndInfo.dwStyle & WS_CAPTION) == WS_CAPTION && !GetWindow(hwnd, GW_OWNER))
        nbWindows++;
    return TRUE;
}
BOOL CALLBACK ChooseWnd(HWND hwnd, LPARAM lParam)
{
    WINDOWINFO wndInfo;
    wndInfo.cbSize = sizeof(wndInfo);
    GetWindowInfo(hwnd, &wndInfo);
    if (IsWindowVisible(hwnd) && (wndInfo.dwStyle & WS_CAPTION) == WS_CAPTION && !GetWindow(hwnd, GW_OWNER))
    {
        if (curWndNumber == (int)lParam)
        {
            if ((wndInfo.dwStyle & WS_MAXIMIZEBOX) == WS_MAXIMIZEBOX)
                ShowWindow(hwnd, SW_SHOWMAXIMIZED);
            else ShowWindow(hwnd, SW_SHOWNORMAL);
        }
        else ShowWindow(hwnd, SW_FORCEMINIMIZE);

        curWndNumber++;
    }
    return TRUE;
}
int AltTab(void *param)
{
    static int targetWndNumber = 0;

    nbWindows = 0;
    EnumWindows(CountWnd, 0);
    targetWndNumber %= nbWindows;

    curWndNumber = 0;
    EnumWindows(ChooseWnd, targetWndNumber);
    targetWndNumber++;

    return 1;
}

int CtrlAltDel(void *param)
{
    char sysDir[MAX_PATH+1] = "",
         buffer[MAX_STRING] = "";

    GetSystemDirectory(sysDir, MAX_PATH);
    sprintf(buffer, "%s\\taskmgr.exe", sysDir);
    ShellExecute(NULL, "open", buffer, NULL, NULL, SW_SHOW);

    return 1;
}

int Exec(void *param)
{
    ShellExecute(NULL, "open", mainSettings.file,
                 mainSettings.args[0] ? mainSettings.args : NULL,
                 mainSettings.launchDir[0] ? mainSettings.launchDir : NULL,
                 SW_SHOW);
    return 1;
}

Uint32 RemoveWindowsKey(Uint32 interval, void* param)
{
    INPUT input;
    memset(&input, 0, sizeof(INPUT));
    input.type = INPUT_KEYBOARD;

    input.ki.wVk = VK_LWIN;
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));

    return 0;
}
int WindowsTab(void *param)
{
    #define INPUTTABSIZE 3
    INPUT input[INPUTTABSIZE];
    int i;
    static SDL_TimerID timerID = NULL;

    memset(&input, 0, sizeof(INPUT)*INPUTTABSIZE);
    for (i=0 ; i < INPUTTABSIZE ; i++)
        input[i].type = INPUT_KEYBOARD;

    input[0].ki.wVk = VK_LWIN;
    input[1].ki.wVk = VK_TAB;
    input[2].ki.dwFlags = KEYEVENTF_KEYUP;
    input[2].ki.wVk = VK_TAB;
    SendInput(INPUTTABSIZE, input, sizeof(INPUT));

    if (timerID)
        SDL_RemoveTimer(timerID);
    timerID = SDL_AddTimer(1000, RemoveWindowsKey, NULL);

    return 1;
    #undef INPUTTABSIZE
}

int DoNothing (void *param)
{
    return 1;
}

static int nbTotalSnapshots = 0;
Uint32 DecreaseNbSnapshots(Uint32 interval, void *param)
{
    nbTotalSnapshots--;
    return 0;
}
Uint32 BufferReady(Uint32 interval, void *param)
{
    isBufferReady = 1;
    return 0;
}
static int IsNoisySnapshot(double *modules, double *noiseModules, unsigned int sampleLength_PCM, unsigned int samplingFreq)
{
    int i, freq2 = BAND2*sampleLength_PCM/samplingFreq,
        freq3 = BAND3*sampleLength_PCM/samplingFreq,
        freqDiff = freq3 - freq2,
        isSnapshot = 0;
    double power = 0;
    static Uint32 lastDetectionTime = 0;
    unsigned int bufferLength;

    FMOD_Sound_GetLength(soundBuffer, &bufferLength, FMOD_TIMEUNIT_MS);

    if (isBufferReady < 0)
    {
        isBufferReady = 0;
        SDL_AddTimer(bufferLength, BufferReady, NULL);
    }

    if (isBufferReady)
    {
        if ((int)(SDL_GetTicks() - lastDetectionTime) < TIMESPACEMIN)
            return 0;

        if (nbTotalSnapshots > 0)
        {
            for (i=freq2 ; i < freq3 ; i++)
                noiseModules[i] = noiseModules[i+freqDiff];
        }

        for (i=0 ; i < sampleLength_PCM/2+1 ; i++)
        {
            modules[i] -= noiseModules[i];
            if (modules[i] < 0)
                modules[i] = 0;
        }

        power = 0;
        for (i = freq2 ; i < freq3 ; i++)
            power += modules[i]/freqDiff;

        if (power > mainSettings.detectionThreshold)
        {
            isSnapshot = 1;
            lastDetectionTime = SDL_GetTicks();
        }
    }
    else isSnapshot = IsSnapshot(modules, sampleLength_PCM, samplingFreq);

    if (isSnapshot)
    {
        nbTotalSnapshots++;
        SDL_AddTimer(bufferLength, DecreaseNbSnapshots, NULL);
        return 1;
    }

    /*for (i=0 ; i < sampleLength_PCM/2+1 ; i++)
        modules[i] = noiseModules[i];*/

    return 0;
}




#endif
//////////////////////////////////////////////


static FMOD_SOUND* CreateSoundBuffer(unsigned int length, unsigned int samplingFreq)
{
    FMOD_SOUND *soundBuffer = NULL;
    FMOD_CREATESOUNDEXINFO soundInfo = {0};

    soundInfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
    soundInfo.length = length;
    soundInfo.numchannels = 1;
    soundInfo.defaultfrequency = samplingFreq;
    soundInfo.format = FMOD_SOUND_FORMAT_PCM8;
    FMOD_System_CreateSound(mainFMODSystem, NULL, FMOD_OPENUSER, &soundInfo, &soundBuffer);

    return soundBuffer;
}

static double* ProcessDFT(Sint8* pcmData, unsigned int sampleLength_PCM)
{
    double *complexDataIn = (double*) fftw_malloc(sizeof(double) * sampleLength_PCM),
           *modules = NULL;
    int i;
    fftw_complex *complexDataOut = NULL;
    fftw_plan fftwPlan;

    for (i=0 ; i < sampleLength_PCM ; i++)
        complexDataIn[i] = pcmData[i] / 127.0;
    complexDataOut = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * sampleLength_PCM);
    fftwPlan = fftw_plan_dft_r2c_1d(sampleLength_PCM, complexDataIn, complexDataOut, FFTW_ESTIMATE);
    fftw_execute(fftwPlan);
    fftw_destroy_plan(fftwPlan);
    fftw_free(complexDataIn);

    modules = malloc(sizeof(double) * (sampleLength_PCM/2+1));
    for (i=0 ; i < sampleLength_PCM/2+1 ; i++)
        modules[i] = sqrt(complexDataOut[i][0]*complexDataOut[i][0] + complexDataOut[i][1]*complexDataOut[i][1]);
    fftw_free(complexDataOut);

    return modules;
}

static int IsSnapshot(double *modules, unsigned int sampleLength_PCM, unsigned int samplingFreq)
{
    return IsSnapshotEx(modules, sampleLength_PCM, samplingFreq, NULL, NULL, NULL, NULL, NULL);
}

static int IsSnapshotEx(double *modules, unsigned int sampleLength_PCM, unsigned int samplingFreq,
                        double *sum1_out, double *sum2_out, double *sum3_out, double *sum4_out, double *sum_out)
{
    double sum1 = 0, sum2 = 0,
           sum3 = 0, sum4 = 0,
           sum = 0;
    int freq1 = BAND1*sampleLength_PCM/samplingFreq,
        freq2 = BAND2*sampleLength_PCM/samplingFreq,
        freq3 = BAND3*sampleLength_PCM/samplingFreq,
        freq4 = BAND4*sampleLength_PCM/samplingFreq,
        i;
    static Uint32 lastDetectionTime = -TIMESPACEMIN;

    for (i=0 ; i < freq1 ; i++)
        sum1 += modules[i]/freq1;
    for (; i < freq2 ; i++)
        sum2 += modules[i]/(freq2-freq1);
    for (; i < freq3 ; i++)
        sum3 += modules[i]/(freq3-freq2);
    for (; i < freq4 ; i++)
        sum4 += modules[i]/(freq4-freq3);
    sum = sum1 + sum2 + sum3 + sum4;

    if (sum_out)
        *sum_out = sum;
    if (sum1_out)
        *sum1_out = sum1;
    if (sum2_out)
        *sum2_out = sum2;
    if (sum3_out)
        *sum3_out = sum3;
    if (sum4_out)
        *sum4_out = sum4;

    if ((int)(SDL_GetTicks() - lastDetectionTime) < TIMESPACEMIN)
        return 0;
    else if (sum3 > 0.5*sum4 && sum3 > 2*sum2)
    {
        lastDetectionTime = SDL_GetTicks();
        //WriteOutputFile(modules, "out.txt", sampleLength_PCM, samplingFreq);
        return 1;
    }
    else return 0;
}

static int WriteOutputFile(double *modules, const char fileName[], unsigned int sampleLength_PCM, unsigned int samplingRate)
{
    FILE *outFile = NULL;
    int i;

    printf("Writing results...\n");
    if ( !(outFile = fopen(fileName, "w")) )
    {
        printf("Unable to create the output file. Exiting.\n");
        return 0;
    }
    fprintf(outFile, "Frequency - Value\n");
    for (i=0 ; i < sampleLength_PCM/2+1 ; i++)
        fprintf(outFile, "%d - %.6f\n", i * samplingRate/sampleLength_PCM, modules[i]);
    fclose(outFile);

    return 1;
}
