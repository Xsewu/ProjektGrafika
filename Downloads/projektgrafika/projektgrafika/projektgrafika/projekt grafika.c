#define _CRT_SECURE_NO_WARNINGS

#ifdef _MSC_VER
#  pragma comment(lib, "opengl32.lib")
#  pragma comment(lib, "glu32.lib")
#  pragma comment(lib, "comctl32.lib") 
#endif

#include <windows.h>
#include <gl\gl.h>
#include <gl\glu.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Limity systemowe
#define MAX_PARTICLES 2000      // Maksymalna liczba cząsteczek (iskry + spray)
#define MAX_VERTICES 20000
#define MAX_LINE_LENGTH 256
#define TIMER_INTERVAL 16      

// Wymiary sceny
#define FLOOR_SIZE 18.0f        
#define CONVEYOR_LENGTH 45.f    // Długość taśmociągu
#define CONVEYOR_WIDTH 2.5f
#define CONVEYOR_HEIGHT 2.3f    // Wysokość powierzchni taśmy nad podłogą

// Parametry animacji
#define BASE_PRODUCT_SPEED 0.15f    // Bazowa prędkość ruchu produktu po taśmie
#define WELD_DURATION 150.0f        // Liczba klatek trwania spawania/lakierowania
#define ARM_SPEED_SLOW 1.0f         // Prędkość ruchu ramion 
#define ARM_SPEED_FAST 1.5f        
#define ARM_SPEED_FASTER 2.5f       

// Pozycja bazowa robotów względem środka sceny
#define ROBOT_OFFSET_X 0.0f
#define ROBOT_OFFSET_Y 0.0f
#define ROBOT_OFFSET_Z -5.0f

// Kolory dla elementów UI (RGB jako argumenty makra)
#define UI_COLOR_BG       0.2f, 0.2f, 0.2f      // Tło przycisków
#define UI_COLOR_HOVER    0.3f, 0.3f, 0.3f      // Przycisk pod kursorem
#define UI_COLOR_ACTIVE   0.4f, 0.5f, 0.8f      // Przycisk aktywny/wciśnięty
#define UI_COLOR_TEXT     1.0f, 1.0f, 1.0f      // Kolor tekstu
#define ENABLE_LIGHTING 1

// Zmienne globalne - kontekst Windows i OpenGL
static const TCHAR lpszAppName[] = TEXT("Projekt Grafika");
static HINSTANCE hInstance;
static HDC hDC;                         
static GLuint fontBase = 0;              

// Flagi stanu aplikacji
static int isFreeCamMode = 0;            // 0 = kamera statyczna, 1 = sterowanie WASD
static float simulationSpeed = 1.0f;     // Mnożnik prędkości (0.1 - 3.0)
static int isPaused = 0;
static int productCounter = 0;           // Licznik ukończonych produktów
static int isPaintingStationActive = 1;  // Czy stacja lakiernicza przetwarza produkty

// Pozycja robota lakierniczego na szynie (ruch wzdłuż osi Z)
static float paintingRobotZ = 9.3f;
#define PAINT_Z_WORK 9.3f       // Pozycja przy taśmie (praca)
#define PAINT_Z_RETRACT 16.0f   // Pozycja wycofana (serwis/nieaktywny)

// Tryb konfiguracji ręcznej robotów
static int isConfigMode = 0;             // 0 = auto, 1 = ręczne sterowanie kątami
static int configSelectedRobot = 1;      // Który robot edytujemy (1 lub 2)
static int hudHeight = 140;              // Wysokość panelu sterowania na dole ekranu

// Stan myszy dla UI i kamery
static int mouseX = 0;
static int mouseY = 0;                   // Współrzędne w układzie OpenGL (Y od dołu)
static int mouseLeftDown = 0;
static int mouseRightDown = 0;
static int uiCapturedMouse = 0;          // Czy kliknięcie było w obszarze HUD
static int isNightMode = 0;              // Tryb nocny - ciemniejsze światło ambient

// Cząsteczka efektów wizualnych (iskry spawalnicze, spray lakierniczy)
typedef struct {
    float x, y, z;           // Pozycja w przestrzeni 3D
    float vx, vy, vz;        // Wektor prędkości
    float life;              // Pozostały czas życia (0 = martwa)
    float lifeMax;           // Początkowy czas życia 
    float r, g, b;           // Kolor cząsteczki
    float size;              
    int type;                // 0 = iskra, 1 = spray
    float rotation;        
    float spin;             
} Particle;

static Particle particles[MAX_PARTICLES];
static int animationMode = 0;

// Konfiguracja systemu oświetlenia
typedef struct {
    int enableLighting;      // Główny przełącznik świateł
    int enableSpotlight;     // Czy włączone są lampy sufitowe
    float ambientIntensity;
    float diffuseIntensity;
    float specularIntensity;
} LightingConfig;

static LightingConfig lighting = {
    1,      // Oświetlenie włączone
    1,      // Reflektory włączone
    0.3f,   // Ambient
    0.8f,   // Diffuse
    1.0f    // Specular
};

// Kamera FPS-style z możliwością swobodnego latania
typedef struct {
    float x, y, z;           // Pozycja oka kamery
    float yaw;               // Obrót wokół osi Y (lewo-prawo)
    float pitch;             // Nachylenie góra-dół (-89 do +89 stopni)
    int lastMouseX;          // Poprzednia pozycja myszy (do delta)
    int lastMouseY;
    int isDragging;          // Czy aktualnie przeciągamy myszą
} Camera;

// Domyślna pozycja kamery - widok z góry na całą scenę
static Camera camera = { 0.0f, 10.0f, 25.0f, -90.0f, -20.0f, 0, 0, 0 };
#define DEG2RAD (3.14159265f / 180.0f)

// Animacja płynnego przejścia kamery między widokami
static const int CAMERA_TRANSITION_MS = 400;
static int cameraTransitionRemaining = 0;
static float cameraStartXRot = 0.0f;
static float cameraStartYRot = 0.0f;
static float cameraStartZoom = 0.0f;
static float cameraTargetXRot = 0.0f;
static float cameraTargetYRot = 0.0f;
static float cameraTargetZoom = 0.0f;
static float cameraEyeX = 0.0f;          // Aktualna pozycja oka 
static float cameraEyeY = 0.0f;
static float cameraEyeZ = 0.0f;
static DWORD lastFrameTick = 0;
static int cameraResetRequested = 0;

// Identyfikatory tekstur załadowanych z plików BMP
static GLuint textureConveyor = 0;       // Powierzchnia taśmy
static GLuint textureFloor = 0;          // Podłoga hali
static GLuint textureWood = 0;           // Drewno (regały)
static GLuint textureMetal = 0;          // Metal (ściany, konstrukcje)
static GLuint textureCrate = 0;          // Skrzynki drewniane
static GLuint texturePolki = 0;          // Półki regałów
static GLuint textureocynkowanyMetal = 0;// Wentylacja
static GLuint textureDoor = 0;           // Brama wjazdowa
static GLuint textureCeiling = 0;        // Sufit
static float conveyorTextureOffset = 0.0f; 


typedef enum {
    STATE_APPROACHING = 0,   // Produkt jedzie do pozycji robota
    STATE_POSITIONING = 1,   // Robot opuszcza ramię do produktu
    STATE_WELDING = 2,       // Proces spawania/lakierowania w toku
    STATE_DEPARTING = 3      // Produkt odjeżdża, ramię wraca do pozycji spoczynkowej
} RobotState;

// Pełny stan symulacji pojedynczego stanowiska robotycznego
typedef struct {
    RobotState state;        // Aktualny stan maszyny stanów
    float productX;          // Pozycja X produktu na taśmie
    float weldTimer;         // Licznik czasu spawania (0 do WELD_DURATION)
    int showSparks;          // Czy generować efekty wizualne

    // Kąty stawów robota (stopnie)
    float angleBase;         // Oś 1 - obrót bazy (0-180)
    float angleArm1;         // Oś 2 - bark (-90 do +45)
    float angleArm2;         // Oś 3 - łokieć (-90 do +90)
    float targetAngleArm1;   // Docelowy kąt dla interpolacji
    float targetAngleArm2;

    // Pozycje kluczowe na taśmie
    float startX;            // Gdzie produkt się pojawia
    float stopPosition;      // Gdzie produkt zatrzymuje się do obróbki
    float endX;              // Gdzie produkt znika 

    int isActive;            // Czy na tym stanowisku jest produkt
    int productType;         // 0=kątowniki, 1=lokomotywa, 2=samochód
    float paintR, paintG, paintB;  // Docelowy kolor lakieru
} SimulationData;

// Robot 1: Stacja spawalnicza (lewa strona, X = -5)
static SimulationData sim1 = {
    STATE_APPROACHING,
    -20.0f,                  // Produkt startuje poza sceną 
    0.0f, 0,
    90.0f, -20.0f, -30.0f,   // Kąty początkowe 
    0.0f, 0.0f,
    -18.0f,                  // startX
    -5.0f,                   // stopPosition (pod robotem)
    0.0f,                    // endX 
    1,                       // Aktywny na starcie
    0,                       // Typ produktu 
    0.5f, 0.5f, 0.55f        // Domyślny kolor
};

// Robot 2: Stacja lakiernicza
static SimulationData sim2 = {
    STATE_APPROACHING,
    0.0f,                    // Produkt przyjeżdża ze środka
    0.0f, 0,
    90.0f, -20.0f, -30.0f,
    0.0f, 0.0f,
    0.0f,                    // startX (odbiór od sim1)
    5.0f,                    // stopPosition
    20.0f,                   // endX (poza sceną - ukończony produkt)
    0,                       // Nieaktywny na starcie
    0,
    0.5f, 0.5f, 0.55f
};

// Deklaracje funkcji (forward declarations)
GLuint LoadTextureFromBMP(const char* filename, int wrapMode, int filterMode);
void DrawCube(void);
void DrawFloor(void);
void DrawTasma(void);
void DrawSparks(float timer);
void DrawShelf(void);
void DrawROBOT2(SimulationData* sim, float offsetX, float offsetZ, float rotationY);
void SetupLighting(void);
void DrawCylinder(void);
void DrawCrate(void);
void DrawTasmaEndCovers(void);


// Zwraca target gdy różnica < 0.1 stopnia
float LerpAngle(float current, float target, float speed) {
    float diff = target - current;
    if ((float)fabs(diff) < 0.1f) return target;
    return current + diff * speed * 0.05f;
}

// Wczytuje plik BMP i konwertuje BGR -> RGB
// Zwraca wskaźnik do danych pikseli 
unsigned char* LoadBitmapFile(const char* filename, BITMAPINFOHEADER* bitmapInfoHeader) {
    FILE* filePtr = fopen(filename, "rb");
    if (filePtr == NULL) return NULL;

    BITMAPFILEHEADER bitmapFileHeader;
    fread(&bitmapFileHeader, sizeof(BITMAPFILEHEADER), 1, filePtr);

    // Weryfikacja sygnatury "BM"
    if (bitmapFileHeader.bfType != 0x4D42) {
        fclose(filePtr);
        return NULL;
    }

    fread(bitmapInfoHeader, sizeof(BITMAPINFOHEADER), 1, filePtr);

    int height = abs(bitmapInfoHeader->biHeight);

    // Oblicz rozmiar danych jeśli nie podany
    if (bitmapInfoHeader->biSizeImage == 0) {
        int rowSize = ((bitmapInfoHeader->biWidth * 3 + 3) / 4) * 4;  // Wyrównanie do 4 bajtów
        bitmapInfoHeader->biSizeImage = rowSize * height;
    }

    fseek(filePtr, bitmapFileHeader.bfOffBits, SEEK_SET);

    unsigned char* bitmapImage = (unsigned char*)malloc(bitmapInfoHeader->biSizeImage);
    if (!bitmapImage) {
        fclose(filePtr);
        return NULL;
    }

    fread(bitmapImage, 1, bitmapInfoHeader->biSizeImage, filePtr);

    // Konwersja BGR (format BMP) na RGB (format OpenGL)
    for (DWORD i = 0; i + 2 < bitmapInfoHeader->biSizeImage; i += 3) {
        unsigned char temp = bitmapImage[i];
        bitmapImage[i] = bitmapImage[i + 2];
        bitmapImage[i + 2] = temp;
    }

    bitmapInfoHeader->biHeight = height;
    fclose(filePtr);

    return bitmapImage;
}


// Ładuje teksturę z pliku BMP i tworzy obiekt tekstury OpenGL
GLuint LoadTextureFromBMP(const char* filename, int wrapMode, int filterMode) {
    BITMAPINFOHEADER info;
    unsigned char* data = LoadBitmapFile(filename, &info);

    if (!data) return 0;

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Parametry próbkowania
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);    // GL_REPEAT lub GL_CLAMP
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterMode);  // GL_LINEAR lub GL_NEAREST
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterMode);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, info.biWidth, info.biHeight,
        0, GL_RGB, GL_UNSIGNED_BYTE, data);

    free(data);
    return textureID;
}

// Tworzy display listy dla czcionki bitmapowej Windows
void BuildFont(void) {
    HFONT font;
    fontBase = glGenLists(96);
    font = CreateFont(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_DONTCARE | DEFAULT_PITCH, TEXT("Arial"));
    SelectObject(hDC, font);
    wglUseFontBitmaps(hDC, 32, 96, fontBase);
}

void KillFont(void) {
    glDeleteLists(fontBase, 96);
}

// Rysuje tekst w bieżącej pozycji
// Obsługuje formatowanie printf-style
void glPrint(const char* fmt, ...) {
    char text[256];
    va_list ap;
    if (fmt == NULL) return;
    va_start(ap, fmt);
    vsprintf(text, fmt, ap);
    va_end(ap);
    glPushAttrib(GL_LIST_BIT);
    glListBase(fontBase - 32);
    glCallLists((GLsizei)strlen(text), GL_UNSIGNED_BYTE, text);
    glPopAttrib();
}

// Generuje losową wartość float z zakresu [min, max]
float RandomFloat(float min, float max) {
    return min + (float)rand() / (float)RAND_MAX * (max - min);
}

// Inicjalizuje tablicę cząsteczek - wszystkie martwe (life <= 0)
void InitParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++)
        particles[i].life = -1.0f;
}

// Tworzy iskrę spawalniczą w podanej pozycji
// Iskry lecą w losowych kierunkach, opadają pod wpływem grawitacji
void SpawnSpark(float x, float y, float z) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life <= 0.0f) {
            particles[i].x = x;
            particles[i].y = y;
            particles[i].z = z;

            // Losowy kierunek w płaszczyźnie XZ
            float angle = RandomFloat(0, 6.28f);
            float speed = RandomFloat(0.05f, 0.2f);
            particles[i].vx = cosf(angle) * speed;
            particles[i].vy = RandomFloat(0.1f, 0.3f);  // Początkowy impuls w górę
            particles[i].vz = sinf(angle) * speed;

            particles[i].life = 1.0f;
            particles[i].r = 1.0f;
            particles[i].g = RandomFloat(0.5f, 1.0f);  // Żółto-pomarańczowy gradient
            particles[i].b = 0.2f;
            particles[i].size = 0.05f;
            particles[i].type = 0;  // Typ: iskra (blending addytywny)
            return;
        }
    }
}

// Tworzy cząsteczkę sprayu lakierniczego
// dir* określa główny kierunek natrysku, r/g/b to kolor farby
void SpawnSpray(float x, float y, float z, float dirX, float dirY, float dirZ, float r, float g, float b) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life <= 0.0f) {
            // Lekkie rozproszenie pozycji startowej
            particles[i].x = x + RandomFloat(-0.02f, 0.02f);
            particles[i].y = y + RandomFloat(-0.02f, 0.02f);
            particles[i].z = z + RandomFloat(-0.02f, 0.02f);

            // Stożkowe rozproszenie kierunku
            float spread = 0.12f;
            particles[i].vx = dirX * RandomFloat(0.12f, 0.28f) + RandomFloat(-spread, spread);
            particles[i].vy = dirY * RandomFloat(0.08f, 0.20f) + RandomFloat(-spread * 0.5f, spread * 0.5f);
            particles[i].vz = dirZ * RandomFloat(0.12f, 0.28f) + RandomFloat(-spread, spread);

            particles[i].lifeMax = RandomFloat(0.8f, 1.2f);
            particles[i].life = particles[i].lifeMax;
            particles[i].size = RandomFloat(0.04f, 0.12f);

            float tint = RandomFloat(0.85f, 1.08f);
            particles[i].r = fminf(1.0f, r * tint);
            particles[i].g = fminf(1.0f, g * tint);
            particles[i].b = fminf(1.0f, b * tint);

            particles[i].type = 1;  // Typ: spray (alpha blending)
            particles[i].rotation = RandomFloat(0.0f, 360.0f);
            particles[i].spin = RandomFloat(-120.0f, 120.0f);
            return;
        }
    }
}

void DrawParticles() {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);

    // Wektory do budowy
    float radYaw = camera.yaw * DEG2RAD;
    float rightX = cosf(radYaw);
    float rightZ = sinf(radYaw);
    float upX = 0.0f, upY = 1.0f, upZ = 0.0f;

    // Iskry - blending addytywny 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glBegin(GL_QUADS);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life > 0.0f && particles[i].type == 0) {
            float s = particles[i].size;
            float alpha = particles[i].life;
            glColor4f(particles[i].r, particles[i].g, particles[i].b, alpha);
            // Quad zbudowany z wektorów right i up
            glVertex3f(particles[i].x - rightX * s - upX * s, particles[i].y - rightZ * s - upY * s, particles[i].z + rightZ * s - upZ * s);
            glVertex3f(particles[i].x + rightX * s - upX * s, particles[i].y + rightZ * s - upY * s, particles[i].z - rightZ * s - upZ * s);
            glVertex3f(particles[i].x + rightX * s + upX * s, particles[i].y + rightZ * s + upY * s, particles[i].z - rightZ * s + upZ * s);
            glVertex3f(particles[i].x - rightX * s + upX * s, particles[i].y - rightZ * s + upY * s, particles[i].z + rightZ * s + upZ * s);
        }
    }
    glEnd();

    // Spray - standardowy alpha blending (półprzezroczystość)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life > 0.0f && particles[i].type == 1) {
            float s = particles[i].size;
            // Alpha zanika wraz z czasem życia
            float alpha = particles[i].life / (particles[i].lifeMax > 0.0f ? particles[i].lifeMax : 1.0f);
            alpha = alpha * 0.9f;

            glColor4f(particles[i].r, particles[i].g, particles[i].b, alpha);
            glVertex3f(particles[i].x - rightX * s - upX * s, particles[i].y - rightZ * s - upY * s, particles[i].z + rightZ * s - upZ * s);
            glVertex3f(particles[i].x + rightX * s - upX * s, particles[i].y + rightZ * s - upY * s, particles[i].z - rightZ * s - upZ * s);
            glVertex3f(particles[i].x + rightX * s + upX * s, particles[i].y + rightZ * s + upY * s, particles[i].z - rightZ * s + upZ * s);
            glVertex3f(particles[i].x - rightX * s + upX * s, particles[i].y - rightZ * s + upY * s, particles[i].z + rightZ * s + upZ * s);
        }
    }
    glEnd();

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
}

// Rysuje linie iskier spawalniczych (efekt natychmiastowy, bez cząsteczek)
void DrawSparks(float timer) {
    glDisable(GL_TEXTURE_2D);
    glLineWidth(2.0f);

    // Pulsowanie intensywności
    float intensity = ((float)sin(timer * 0.1) + 1.0f) * 0.5f;
    int sparkCount = (int)(20 + intensity * 40);

    glBegin(GL_LINES);
    for (int i = 0; i < sparkCount; i++) {
        float angle = (float)(rand() % 360) * 3.14159265f / 180.0f;
        float dist = (float)(rand() % 100) / 100.0f * (0.5f + intensity * 0.3f);
        float ex = cosf(angle) * dist;
        float ez = sinf(angle) * dist;
        float ey = ((float)(rand() % 100) / 100.0f) * 0.8;

        // Gradient: jasny żółty w centrum -> pomarańczowy na końcu
        glColor3f(1.0f, 1.0f, 0.3f + intensity * 0.5f);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glColor3f(1.0f, 0.2f + intensity * 0.3f, 0.0f);
        glVertex3f(ex, ey, ez);
    }
    glEnd();
}

// Aktualizacja fizyki cząsteczek - wywoływana co klatkę
void UpdateParticles() {
    if (isPaused) return;

    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life > 0.0f) {
            particles[i].x += particles[i].vx * simulationSpeed;
            particles[i].y += particles[i].vy * simulationSpeed;
            particles[i].z += particles[i].vz * simulationSpeed;

            particles[i].rotation += particles[i].spin * 0.01f * simulationSpeed;
            if (particles[i].rotation > 360.0f) particles[i].rotation -= 360.0f;
            if (particles[i].rotation < -360.0f) particles[i].rotation += 360.0f;

            if (particles[i].type == 0) {
                // Iskry: grawitacja, odbicie od podłogi, zanikanie koloru
                particles[i].vy -= 0.015f * simulationSpeed;
                particles[i].life -= 0.02f * simulationSpeed;
                if (particles[i].y < 0.0f) {
                    particles[i].y = 0.0f;
                    particles[i].vy *= -0.5f;  // Odbicie z utratą energii
                }
                particles[i].g *= 0.95f;  // Przejście żółty -> czerwony
            }
            else if (particles[i].type == 1) {
                // Spray: lekka grawitacja, turbulencje, rozpraszanie
                particles[i].vy -= 0.009f * simulationSpeed;
                particles[i].vx += RandomFloat(-0.0015f, 0.0015f) * simulationSpeed;
                particles[i].vz += RandomFloat(-0.0015f, 0.0015f) * simulationSpeed;
                particles[i].life -= 0.035f * simulationSpeed;
                particles[i].size += 0.0008f * simulationSpeed;  // Rozpraszanie mgły

                // Usunięcie przy kontakcie z taśmą
                if (particles[i].y <= CONVEYOR_HEIGHT + 0.25f) {
                    particles[i].life = 0.0f;
                }
            }

            if (particles[i].life <= 0.0f) {
                particles[i].life = -1.0f;  // Oznacz jako wolną
            }
        }
    }
}


// Resetuje materiał OpenGL do wartości domyślnych
void ResetMaterial(void) {
    float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float black[] = { 0.0f, 0.0f, 0.0f, 1.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, white);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, white);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, black);

    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
}


// Rysuje wypełniony prostokąt 2D z obramowaniem
void DrawRect(int x, int y, int w, int h, float r, float g, float b) {
    // Wypełnienie
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();

    // Obramowanie
    glColor3f(0.8f, 0.8f, 0.8f);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2i(x, y);
    glVertex2i(x + w, y);
    glVertex2i(x + w, y + h);
    glVertex2i(x, y + h);
    glEnd();
}

// Przycisk UI z detekcją hover i kliknięcia
// Zwraca 1 przy kliknięciu, 0 w przeciwnym razie
int DoButton(int x, int y, int w, int h, const char* label) {
    int hot = (mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h);

    if (hot) {
        uiCapturedMouse = 1;  // Blokuj przekazanie do sceny 3D
        if (mouseLeftDown) {
            DrawRect(x, y, w, h, UI_COLOR_ACTIVE);
        }
        else {
            DrawRect(x, y, w, h, UI_COLOR_HOVER);
        }
    }
    else {
        DrawRect(x, y, w, h, UI_COLOR_BG);
    }

    glColor3f(UI_COLOR_TEXT);
    glRasterPos2i(x + 10, y + h / 2 - 4);
    glPrint(label);

    // Detekcja kliknięcia (edge detection - tylko raz przy wciśnięciu)
    static int wasDown = 0;
    if (hot && mouseLeftDown && !wasDown) { wasDown = 1; return 1; }
    if (!mouseLeftDown) wasDown = 0;
    return 0;
}

// Suwak UI do edycji wartości float w zakresie [min, max]
void DoSlider(int x, int y, int w, int h, const char* label, float* value, float min, float max) {
    int hot = (mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h);

    if (hot || (mouseLeftDown && uiCapturedMouse)) {
        if (hot) uiCapturedMouse = 1;
        if (mouseLeftDown && hot) {
            // Oblicz nową wartość na podstawie pozycji myszy
            float ratio = (float)(mouseX - x) / (float)w;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            *value = min + ratio * (max - min);
        }
    }

    // Tło suwaka
    DrawRect(x, y, w, h, 0.15f, 0.15f, 0.15f);

    // Wypełnienie proporcjonalne do wartości
    float ratio = (*value - min) / (max - min);
    int fillW = (int)(ratio * w);
    DrawRect(x, y, fillW, h, UI_COLOR_ACTIVE);

    // Etykieta z aktualną wartością
    glColor3f(UI_COLOR_TEXT);
    char buf[64];
    sprintf(buf, "%s: %.2f", label, *value);
    glRasterPos2i(x + 5, y + h / 2 - 4);
    glPrint(buf);
}

// Obsługuje sterowanie kamerą za pomocą klawiatury WASD
void MoveFreeCamera(void) {
    if (!isFreeCamMode) return;
    if (GetForegroundWindow() != GetActiveWindow()) return;  // Tylko gdy okno aktywne

    float speed = 0.3f;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
        speed *= 4.0f;  // Shift = szybsze latanie

    // Wektor kierunku patrzenia (tylko XZ)
    float frontX = cosf(camera.yaw * DEG2RAD);
    float frontZ = sinf(camera.yaw * DEG2RAD);

    // Wektor prostopadły (w prawo)
    float rightX = -frontZ;
    float rightZ = frontX;

    // WASD - ruch w płaszczyźnie poziomej
    if (GetAsyncKeyState('W') & 0x8000) { camera.x += frontX * speed; camera.z += frontZ * speed; }
    if (GetAsyncKeyState('S') & 0x8000) { camera.x -= frontX * speed; camera.z -= frontZ * speed; }
    if (GetAsyncKeyState('A') & 0x8000) { camera.x -= rightX * speed; camera.z -= rightZ * speed; }
    if (GetAsyncKeyState('D') & 0x8000) { camera.x += rightX * speed; camera.z += rightZ * speed; }

    // Q/E - ruch pionowy
    if (GetAsyncKeyState('E') & 0x8000) camera.y += speed;
    if (GetAsyncKeyState('Q') & 0x8000) camera.y -= speed;

    // Ograniczenie minimalnej wysokości
    if (camera.y < 1.5f) camera.y = 1.5f;
}

// Główna funkcja rysująca panel sterowania 
// Renderowane w przestrzeni 2D na wierzchu sceny 3D
void DrawInterfejs(int width, int height) {
    // Licznik FPS
    static int frameCount = 0;
    static int currentTime = 0;
    static int previousTime = 0;
    static float fps = 0.0f;
    frameCount++;
    currentTime = GetTickCount64();
    if (currentTime - previousTime > 1000) {
        fps = frameCount * 1000.0f / (currentTime - previousTime);
        previousTime = currentTime;
        frameCount = 0;
    }

    // Przejście do trybu 2D
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_FOG);
    ResetMaterial();

    // Projekcja ortogonalna: (0,0) w lewym dolnym rogu
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

    // Tło panelu
    DrawRect(0, 0, width, hudHeight, 0.18f, 0.18f, 0.2f);

    // Kolorowy pasek górny - wskaźnik trybu
    if (isConfigMode)
        DrawRect(0, hudHeight, width, 4, 1.0f, 0.2f, 0.2f);      // Czerwony = konfiguracja
    else if (isNightMode)
        DrawRect(0, hudHeight, width, 4, 0.0f, 0.4f, 1.0f);      // Niebieski = noc
    else
        DrawRect(0, hudHeight, width, 4, 1.0f, 0.6f, 0.0f);      // Pomarańczowy = dzień/auto

    // Przycisk przełączania trybu
    if (DoButton(20, 95, 220, 35, isConfigMode ? "TRYB: KONFIGURACJA" : "TRYB: AUTO")) {
        isConfigMode = !isConfigMode;
        if (isConfigMode) isPaused = 0;
    }

    if (!isConfigMode) {
        // Panel trybu automatycznego
        DoSlider(20, 50, 220, 30, "PREDKOSC LINII", &simulationSpeed, 0.1f, 3.0f);
        if (DoButton(250, 50, 140, 30, isPaused ? "WZNOW" : "PAUZA"))
            isPaused = !isPaused;

        if (DoButton(420, 95, 200, 35, isPaintingStationActive ? "STACJA LAKIER: WL." : "STACJA LAKIER: WYL.")) {
            isPaintingStationActive = !isPaintingStationActive;
        }

        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        ResetMaterial();

        // Wyświetlanie statusu
        char statusBuf[64];
        if (isPaused) {
            glColor3f(1.0f, 0.3f, 0.3f);
            sprintf(statusBuf, "STATUS: ZATRZYMANO");
        }
        else if (sim1.state == STATE_WELDING) {
            glColor3f(1.0f, 0.8f, 0.2f);
            sprintf(statusBuf, "STATUS: SPAWANIE");
        }
        else if (sim2.state == STATE_WELDING) {
            glColor3f(0.4f, 0.6f, 1.0f);
            sprintf(statusBuf, "STATUS: LAKIEROWANIE");
        }
        else {
            glColor3f(1.0f, 1.0f, 1.0f);
            sprintf(statusBuf, "STATUS: TRANSPORT");
        }
        glRasterPos2i(420, 75); glPrint(statusBuf);

        // Licznik produktów
        glColor3f(0.2f, 1.0f, 0.2f);
        char countBuf[64];
        sprintf(countBuf, "GOTOWE PRODUKTY: %d", productCounter);
        glRasterPos2i(420, 55); glPrint(countBuf);

        glColor3f(1.0f, 0.9f, 0.2f);
        glRasterPos2i(250, 105); glPrint("FPS: %.0f", fps);
    }
    else {
        // Panel trybu konfiguracji ręcznej
        char robLabel[32];
        sprintf(robLabel, "EDYTUJESZ: ROBOT %d", configSelectedRobot);
        if (DoButton(260, 95, 200, 35, robLabel))
            configSelectedRobot = (configSelectedRobot == 1) ? 2 : 1;

        SimulationData* targetSim = (configSelectedRobot == 1) ? &sim1 : &sim2;
        DrawRect(20, 10, 440, 80, 0.12f, 0.12f, 0.12f);

        // Suwaki dla poszczególnych osi robota
        DoSlider(30, 62, 180, 20, "OS J1 (BAZA)", &targetSim->angleBase, 0.0f, 180.0f);
        DoSlider(30, 38, 180, 20, "OS J2 (BARK)", &targetSim->angleArm1, -90.0f, 45.0f);
        DoSlider(30, 14, 180, 20, "OS J3 (LOKIEC)", &targetSim->angleArm2, -90.0f, 90.0f);

        // Dodatkowy suwak dla robota 2 - pozycja na szynie
        if (configSelectedRobot == 2) {
            DoSlider(230, 60, 180, 20, "POZYCJA SZYNY", &paintingRobotZ, PAINT_Z_WORK, PAINT_Z_RETRACT);
        }

        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        ResetMaterial();

        glColor3f(0.6f, 0.8f, 1.0f);
        glRasterPos2i(230, 35); glPrint("TRYB RECZNY AKTYWNY");
        glRasterPos2i(230, 15); glPrint("AUTOMATYKA WYLACZONA");
    }

    // Przyciski widoków kamery
    int camX = 650, btnW = 160, btnH = 30, gap = 170;
    if (DoButton(camX, 100, btnW * 2 + 10, btnH, isFreeCamMode ? "[ TRYB: SWOBODNY (WSAD) ]" : "[ TRYB: STATYCZNY ]"))
        isFreeCamMode = !isFreeCamMode;

    // Predefiniowane pozycje kamery
    if (DoButton(camX, 60, btnW, btnH, "WIDOK: GLOWNY")) {
        isFreeCamMode = 0;
        camera.x = 0.0f; camera.y = 18.0f; camera.z = 28.0f;
        camera.yaw = -90.0f; camera.pitch = -30.0f;
    }
    if (DoButton(camX + gap, 60, btnW, btnH, "WIDOK: OD DOLU")) {
        isFreeCamMode = 0;
        camera.x = 15.0f; camera.y = 2.0f; camera.z = 15.0f;
        camera.yaw = -135.0f; camera.pitch = 15.0f;
    }
    if (DoButton(camX, 20, btnW, btnH, "ZBLIZ: ROBOT 1")) {
        isFreeCamMode = 0;
        camera.x = -12.0f; camera.y = 6.0f; camera.z = 3.0f;
        camera.yaw = -20.0f; camera.pitch = -15.0f;
    }
    if (DoButton(camX + gap, 20, btnW, btnH, "ZBLIZ: ROBOT 2")) {
        isFreeCamMode = 0;
        camera.x = 12.0f; camera.y = 6.0f; camera.z = 6.0f;
        camera.yaw = -160.0f; camera.pitch = -15.0f;
    }

    if (DoButton(width - 140, 95, 120, 35, isNightMode ? "DZIEN" : "NOC"))
        isNightMode = !isNightMode;

    // Instrukcja sterowania w trybie swobodnym
    if (isFreeCamMode) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        int boxW = 330, boxH = 120;
        int boxX = width - boxW - 10;
        int boxY = height - boxH - 10;

        // Półprzezroczyste tło
        glColor4f(0.0f, 0.0f, 0.0f, 0.7f);
        glBegin(GL_QUADS);
        glVertex2i(boxX, boxY);
        glVertex2i(boxX + boxW, boxY);
        glVertex2i(boxX + boxW, boxY + boxH);
        glVertex2i(boxX, boxY + boxH);
        glEnd();
        glDisable(GL_BLEND);

        // Zielona ramka
        glLineWidth(2.0f);
        glColor3f(0.0f, 0.8f, 0.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2i(boxX, boxY);
        glVertex2i(boxX + boxW, boxY);
        glVertex2i(boxX + boxW, boxY + boxH);
        glVertex2i(boxX, boxY + boxH);
        glEnd();

        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        ResetMaterial();

        int textX = boxX + 20, textY = boxY + 95, step = 20;
        glColor3f(0.4f, 1.0f, 0.4f);
        glRasterPos2i(textX, textY);
        glPrint("STEROWANIE KAMERA:");

        glColor3f(1.0f, 1.0f, 1.0f);
        int colKeys = textX;
        int colDesc = textX + 140;

        glRasterPos2i(colKeys, textY - step * 1); glPrint(" [ W, A, S, D ]");
        glRasterPos2i(colDesc, textY - step * 1); glPrint("- Latanie");

        glRasterPos2i(colKeys, textY - step * 2); glPrint(" [ PPM ]");
        glRasterPos2i(colDesc, textY - step * 2); glPrint("- Rozgladanie sie");

        glRasterPos2i(colKeys, textY - step * 3); glPrint(" [ Scroll ]");
        glRasterPos2i(colDesc, textY - step * 3); glPrint("- Zoom / Przod-Tyl");

        glRasterPos2i(colKeys, textY - step * 4); glPrint(" [ Q / E ]");
        glRasterPos2i(colDesc, textY - step * 4); glPrint("- Dol / Gora");
    }

    // Powrót do 3D
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glEnable(GL_DEPTH_TEST);
    if (lighting.enableLighting) glEnable(GL_LIGHTING);
}



// Rysuje ściany hali, filary, sufit i belki konstrukcyjne
void DrawWalls(void) {
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    float height = 22.9f;
    float repeat = 4.0f;

    // Ściany boczne i tylna
    if (textureMetal) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureMetal);
    }
    else if (textureFloor) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureFloor);
    }
    else glDisable(GL_TEXTURE_2D);

    glColor3f(0.6f, 0.6f, 0.65f);
    glBegin(GL_QUADS);

    // Ściana tylna (-Z)
    glNormal3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);         glVertex3f(-FLOOR_SIZE, -0.01f, -FLOOR_SIZE);
    glTexCoord2f(repeat, 0.0f);       glVertex3f(FLOOR_SIZE, -0.01f, -FLOOR_SIZE);
    glTexCoord2f(repeat, repeat / 2); glVertex3f(FLOOR_SIZE, height, -FLOOR_SIZE);
    glTexCoord2f(0.0f, repeat / 2);   glVertex3f(-FLOOR_SIZE, height, -FLOOR_SIZE);

    // Ściana lewa (-X)
    glNormal3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);         glVertex3f(-FLOOR_SIZE, -0.01f, FLOOR_SIZE);
    glTexCoord2f(repeat, 0.0f);       glVertex3f(-FLOOR_SIZE, -0.01f, -FLOOR_SIZE);
    glTexCoord2f(repeat, repeat / 2); glVertex3f(-FLOOR_SIZE, height, -FLOOR_SIZE);
    glTexCoord2f(0.0f, repeat / 2);   glVertex3f(-FLOOR_SIZE, height, FLOOR_SIZE);

    // Ściana prawa (+X)
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);         glVertex3f(FLOOR_SIZE, -0.01f, -FLOOR_SIZE);
    glTexCoord2f(repeat, 0.0f);       glVertex3f(FLOOR_SIZE, -0.01f, FLOOR_SIZE);
    glTexCoord2f(repeat, repeat / 2); glVertex3f(FLOOR_SIZE, height, FLOOR_SIZE);
    glTexCoord2f(0.0f, repeat / 2);   glVertex3f(FLOOR_SIZE, height, -FLOOR_SIZE);
    glEnd();

    // Ściana przednia z bramą (osobna tekstura)
    if (textureDoor) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureDoor);
    }
    else if (textureMetal) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureMetal);
    }
    else glDisable(GL_TEXTURE_2D);

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(FLOOR_SIZE, -0.01f, FLOOR_SIZE);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-FLOOR_SIZE, -0.01f, FLOOR_SIZE);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-FLOOR_SIZE, height, FLOOR_SIZE);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(FLOOR_SIZE, height, FLOOR_SIZE);
    glEnd();
    glDisable(GL_TEXTURE_2D);

    // Sprawdzenie czy kamera jest na zewnątrz (dla optymalizacji)
    const float camMargin = 0.5f;
    int cameraOutsideFront = (cameraEyeZ > FLOOR_SIZE - camMargin) ? 1 : 0;

    // Filary przy bramie (widoczne tylko od wewnątrz)
    if (!cameraOutsideFront) {
        float pillarWidth = 1.0f;
        float pillarDepth = 1.0f;
        float pillarHeight = height;
        const float zInset = 0.01f;
        const float xInset = 0.05f;
        float pillarFrontFaceZ = FLOOR_SIZE - zInset;
        float pillarCenterZ = pillarFrontFaceZ - pillarDepth * 0.5f;
        float pillarCenterX = FLOOR_SIZE - xInset - pillarWidth * 0.5f;

        if (textureMetal) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureMetal);
            glColor3f(1.0f, 1.0f, 1.0f);
        }
        else {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.38f, 0.38f, 0.4f);
        }

        // Lewy i prawy filar
        glPushMatrix();
        glTranslatef(-pillarCenterX, pillarHeight * 0.5f, pillarCenterZ);
        glScalef(pillarWidth, pillarHeight, pillarDepth);
        DrawCube();
        glPopMatrix();

        glPushMatrix();
        glTranslatef(pillarCenterX, pillarHeight * 0.5f, pillarCenterZ);
        glScalef(pillarWidth, pillarHeight, pillarDepth);
        DrawCube();
        glPopMatrix();

        // Kapitele filarów (drewniane)
        if (textureWood) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureWood);
            glColor3f(1.0f, 1.0f, 1.0f);
        }
        else {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.75f, 0.65f, 0.5f);
        }

        glPushMatrix();
        glTranslatef(-pillarCenterX, pillarHeight - 0.35f, pillarCenterZ);
        glScalef(pillarWidth * 1.2f, 0.5f, pillarDepth * 1.2f);
        DrawCube();
        glPopMatrix();

        glPushMatrix();
        glTranslatef(pillarCenterX, pillarHeight - 0.35f, pillarCenterZ);
        glScalef(pillarWidth * 1.2f, 0.5f, pillarDepth * 1.2f);
        DrawCube();
        glPopMatrix();

        // Nadproże bramy
        if (textureMetal) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureMetal);
            glColor3f(1.0f, 1.0f, 1.0f);
        }
        else {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.4f, 0.4f, 0.45f);
        }

        float lintelLeftOuterX = -pillarCenterX - pillarWidth * 0.5f;
        float lintelRightOuterX = pillarCenterX + pillarWidth * 0.5f;
        float lintelWidth = lintelRightOuterX - lintelLeftOuterX;

        glPushMatrix();
        glTranslatef((lintelLeftOuterX + lintelRightOuterX) * 0.5f, pillarHeight - 0.85f, pillarCenterZ - 0.02f);
        glScalef(lintelWidth + 0.2f, 0.6f, pillarDepth * 0.9f);
        DrawCube();
        glPopMatrix();

        glDisable(GL_TEXTURE_2D);
    }

    // Sufit
    float ceilRepeat = repeat * 2.0f;
    if (textureMetal) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureCeiling);
    }
    else if (textureFloor) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureCeiling);
    }
    else glDisable(GL_TEXTURE_2D);

    glColor3f(0.62f, 0.62f, 0.66f);
    glBegin(GL_QUADS);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);             glVertex3f(-FLOOR_SIZE, height, -FLOOR_SIZE);
    glTexCoord2f(ceilRepeat, 0.0f);       glVertex3f(FLOOR_SIZE, height, -FLOOR_SIZE);
    glTexCoord2f(ceilRepeat, ceilRepeat); glVertex3f(FLOOR_SIZE, height, FLOOR_SIZE);
    glTexCoord2f(0.0f, ceilRepeat);       glVertex3f(-FLOOR_SIZE, height, FLOOR_SIZE);
    glEnd();

    glDisable(GL_TEXTURE_2D);

    // Belki dachowe (widoczne tylko od wewnątrz)
    if (!cameraOutsideFront) {
        // Główne belki poprzeczne (stalowe)
        if (textureMetal) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureMetal);
            glColor3f(1.0f, 1.0f, 1.0f);
        }
        else {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.45f, 0.45f, 0.48f);
        }

        int beamCount = 5;
        float startZ = -FLOOR_SIZE + 1.5f;
        float endZ = FLOOR_SIZE - 1.5f;
        float beamLength = (FLOOR_SIZE * 2.0f) + 0.5f;

        for (int i = 0; i < beamCount; i++) {
            float t = beamCount > 1 ? (float)i / (beamCount - 1) : 0.5f;
            float zPos = startZ + (endZ - startZ) * t;
            glPushMatrix();
            glTranslatef(0.0f, height - 0.4f, zPos);
            glScalef(beamLength, 0.4f, 0.6f);
            DrawCube();
            glPopMatrix();
        }

        // Płatwie (drewniane, prostopadle do belek)
        if (textureWood) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureWood);
            glColor3f(1.0f, 1.0f, 1.0f);
        }
        else {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.6f, 0.5f, 0.4f);
        }

        int crossCount = 10;
        for (int i = 0; i < crossCount; i++) {
            float xPos = -FLOOR_SIZE + 1.0f + i * ((FLOOR_SIZE * 2.0f - 2.0f) / (crossCount - 1));
            glPushMatrix();
            glTranslatef(xPos, height - 0.75f, 0.0f);
            glScalef(0.25f, 0.2f, (FLOOR_SIZE * 2.0f) + 0.5f);
            DrawCube();
            glPopMatrix();
        }
        glDisable(GL_TEXTURE_2D);
    }

    glDisable(GL_CULL_FACE);
}

// Rysuje kanał wentylacyjny pod sufitem
void DrawWentylacja(void) {
    float wallZ = -14.0f;
    const float length = 36.0;
    const float height = 1.8f;
    const float depth = 2.4f;
    const float hx = length * 0.5f;
    const float hy = height * 0.5f;
    const float hz = depth * 0.5f;
    const float texRepeatLength = length / 2.0f;
    const float texRepeatDepth = depth / 1.0f;

    GLuint tex = textureocynkowanyMetal ? textureocynkowanyMetal : textureMetal;

    glPushMatrix();
    glTranslatef(0.0f, 13.0f, wallZ);

    if (tex) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, tex);
        glColor3f(0.65f, 0.65f, 0.68f);
    }
    else {
        glDisable(GL_TEXTURE_2D);
        if (lighting.enableLighting) {
            GLfloat matCol[] = { 0.48f, 0.48f, 0.52f, 1.0f };
            GLfloat matSpec[] = { 0.45f, 0.45f, 0.45f, 1.0f };
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, matCol);
            glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
        }
        else {
            glColor3f(0.48f, 0.48f, 0.52f);
        }
    }

    // Prostopadłościan kanału (6 ścian z normalnymi)
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);  // Góra
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-hx, hy, -hz);
    glTexCoord2f(texRepeatLength, 0.0f); glVertex3f(hx, hy, -hz);
    glTexCoord2f(texRepeatLength, texRepeatDepth); glVertex3f(hx, hy, hz);
    glTexCoord2f(0.0f, texRepeatDepth); glVertex3f(-hx, hy, hz);

    glNormal3f(0.0f, -1.0f, 0.0f);  // Dół
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-hx, -hy, hz);
    glTexCoord2f(texRepeatLength, 0.0f); glVertex3f(hx, -hy, hz);
    glTexCoord2f(texRepeatLength, texRepeatDepth); glVertex3f(hx, -hy, -hz);
    glTexCoord2f(0.0f, texRepeatDepth); glVertex3f(-hx, -hy, -hz);

    glNormal3f(0.0f, 0.0f, 1.0f);  // Przód
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-hx, -hy, hz);
    glTexCoord2f(texRepeatLength, 0.0f); glVertex3f(hx, -hy, hz);
    glTexCoord2f(texRepeatLength, 1.0f); glVertex3f(hx, hy, hz);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-hx, hy, hz);

    glNormal3f(0.0f, 0.0f, -1.0f);  // Tył
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-hx, hy, -hz);
    glTexCoord2f(texRepeatLength, 0.0f); glVertex3f(hx, hy, -hz);
    glTexCoord2f(texRepeatLength, 1.0f); glVertex3f(hx, -hy, -hz);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-hx, -hy, -hz);

    glNormal3f(-1.0f, 0.0f, 0.0f);  // Lewo
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-hx, -hy, -hz);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-hx, -hy, hz);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-hx, hy, hz);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-hx, hy, -hz);

    glNormal3f(1.0f, 0.0f, 0.0f);  // Prawo
    glTexCoord2f(0.0f, 0.0f); glVertex3f(hx, -hy, hz);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(hx, -hy, -hz);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(hx, hy, -hz);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(hx, hy, hz);
    glEnd();

    // Opaski łączące sekcje kanału
    if (tex) {
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.32f, 0.32f, 0.32f);
    }

    if (lighting.enableLighting) {
        GLfloat jointCol[] = { 0.20f, 0.15f, 0.12f, 1.0f };
        GLfloat jointSpec[] = { 0.12f, 0.12f, 0.12f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, jointCol);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, jointSpec);
    }
    else {
        glColor3f(0.20f, 0.15f, 0.12f);
    }

    int segments = (int)texRepeatLength;
    if (segments < 2) segments = 2;
    float step = length / (float)segments;
    float collarRadiusX = 0.25f;
    float collarHalfHeight = hy + 0.20f;
    float collarDepth = hz + 1.25f;

    for (int i = 0; i <= segments; i++) {
        float xPos = -hx + (i * step);
        glPushMatrix();
        glTranslatef(xPos, 0.0f, 0.0f);
        glScalef(collarRadiusX, collarHalfHeight * 2.0f, collarDepth);
        DrawCube();
        glPopMatrix();
    }

    // Reset materiału
    if (lighting.enableLighting) {
        GLfloat def[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        GLfloat spec[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, def);
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
    }
    else {
        glColor3f(1.0f, 1.0f, 1.0f);
    }

    glPopMatrix();
}


// Rysuje podłogę hali z teksturą powtarzaną
void DrawFloor(void) {
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureFloor);
    glColor3f(1.0f, 1.0f, 1.0f);

    float texRepeat = FLOOR_SIZE / 5.0f;  // Powtórzenie tekstury

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(-FLOOR_SIZE, -0.01f, FLOOR_SIZE);
    glTexCoord2f(texRepeat, 0.0f);
    glVertex3f(FLOOR_SIZE, -0.01f, FLOOR_SIZE);
    glTexCoord2f(texRepeat, texRepeat);
    glVertex3f(FLOOR_SIZE, -0.01f, -FLOOR_SIZE);
    glTexCoord2f(0.0f, texRepeat);
    glVertex3f(-FLOOR_SIZE, -0.01f, -FLOOR_SIZE);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_CULL_FACE);
}


// Rysuje wentylator ścienny z animowanymi łopatkami
void DrawWallFan(float x, float y, float z) {
    // Kąt obrotu zależny od czasu i prędkości symulacji
    float angle = (float)(GetTickCount64() * 0.8f * simulationSpeed);

    glPushMatrix();
    glTranslatef(x, y, z);

    float frameSize = 2.5f;
    float frameThick = 0.2f;
    float depth = 1.5f;

    glDisable(GL_TEXTURE_2D);

    // Ciemne tło wentylatora
    glDisable(GL_LIGHTING);
    glColor3f(0.05f, 0.05f, 0.05f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -depth / 4.0f);
    glScalef(frameSize * 1.95f, frameSize * 1.95f, 0.1f);
    DrawCube();
    glPopMatrix();
    glEnable(GL_LIGHTING);

    // Metalowa ramka
    glColor3f(0.3f, 0.3f, 0.35f);

    // Górna belka
    glPushMatrix();
    glTranslatef(0, frameSize, 0);
    glScalef(frameSize * 2 + frameThick, frameThick, depth);
    DrawCube();
    glPopMatrix();

    // Dolna belka
    glPushMatrix();
    glTranslatef(0, -frameSize, 0);
    glScalef(frameSize * 2 + frameThick, frameThick, depth);
    DrawCube();
    glPopMatrix();

    // Lewa belka
    glPushMatrix();
    glTranslatef(-frameSize, 0, 0);
    glScalef(frameThick, frameSize * 2, depth);
    DrawCube();
    glPopMatrix();

    // Prawa belka
    glPushMatrix();
    glTranslatef(frameSize, 0, 0);
    glScalef(frameThick, frameSize * 2, depth);
    DrawCube();
    glPopMatrix();

    // Silnik centralny
    glColor3f(0.2f, 0.2f, 0.2f);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.2f);

    glPushMatrix();
    glScalef(0.7f, 0.7f, 0.4f);
    DrawCube();
    glPopMatrix();

    // Animacja obrotu łopatek
    glRotatef(angle, 0.0f, 0.0f, 1.0f);

    // Pięć łopatek wentylatora
    glColor3f(0.4f, 0.45f, 0.5f);
    for (int i = 0; i < 5; i++) {
        glPushMatrix();
        glRotatef((float)(i * 72), 0.0f, 0.0f, 1.0f);  // 360/5 = 72 stopnie

        glTranslatef(0.0f, 1.6f, 0.1f);
        glScalef(0.5f, 1.4f, 0.08f);
        glRotatef(30.0f, 0.0f, 1.0f, 0.0f);  // Skos łopatki
        DrawCube();
        glPopMatrix();
    }
    glPopMatrix();

    // Kratka ochronna (linie diagonalne)
    glColor3f(0.1f, 0.1f, 0.1f);
    glLineWidth(2.0f);
    float frontZ = depth / 2.0f + 0.05f;

    glBegin(GL_LINES);
    glVertex3f(-frameSize, -frameSize, frontZ); glVertex3f(frameSize, frameSize, frontZ);
    glVertex3f(-frameSize, frameSize, frontZ); glVertex3f(frameSize, -frameSize, frontZ);
    glVertex3f(0, -frameSize, frontZ); glVertex3f(0, frameSize, frontZ);
    glVertex3f(-frameSize, 0, frontZ); glVertex3f(frameSize, 0, frontZ);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    glPopMatrix();
}

// Rysuje taśmociąg z animowaną teksturą
void DrawTasma(void) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureConveyor);
    glColor3f(1.0f, 1.0f, 1.0f);

    glPushMatrix();
    glTranslatef(0.0f, CONVEYOR_HEIGHT, 0.0f);
    glScalef(CONVEYOR_LENGTH, 0.2f, CONVEYOR_WIDTH * 1.1f);

    // Animacja ruchu taśmy przez przesunięcie współrzędnych UV
    float uStart = conveyorTextureOffset;
    float uEnd = uStart + 24.0f;

    glBegin(GL_QUADS);
    glTexCoord2f(uStart, 0.0f); glVertex3f(-0.5f, 0.0f, 0.5f);
    glTexCoord2f(uEnd, 0.0f);   glVertex3f(0.5f, 0.0f, 0.5f);
    glTexCoord2f(uEnd, 1.0f);   glVertex3f(0.5f, 0.0f, -0.5f);
    glTexCoord2f(uStart, 1.0f); glVertex3f(-0.5f, 0.0f, -0.5f);
    glEnd();
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);

    // Metalowe krawędzie taśmociągu
    glColor3f(0.3f, 0.3f, 0.4f);
    glPushMatrix();
    glTranslatef(0.0f, CONVEYOR_HEIGHT - 0.1f, CONVEYOR_WIDTH * 0.55f);
    glScalef(CONVEYOR_LENGTH, 0.3f, 0.1f);
    DrawCube();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, CONVEYOR_HEIGHT - 0.1f, -CONVEYOR_WIDTH * 0.55f);
    glScalef(CONVEYOR_LENGTH, 0.3f, 0.1f);
    DrawCube();
    glPopMatrix();

    // Nogi podtrzymujące
    glColor3f(0.2f, 0.2f, 0.2f);
    for (float i = -10.0f; i <= 10.0f; i += 5.0f) {
        glPushMatrix();
        glTranslatef(i, CONVEYOR_HEIGHT / 2.1f, 0.0f);
        glScalef(0.2f, CONVEYOR_HEIGHT, CONVEYOR_WIDTH * 0.9f);
        DrawCube();
        glPopMatrix();
    }
}

// Rysuje osłony tunelowe na końcach taśmociągu
// Tworzą efekt "wjazdu/wyjazdu" produktów ze sceny
void DrawTasmaEndCovers(void) {
    float edgeX = FLOOR_SIZE - 0.1f;
    float tunnelHeight = 4.0f;
    float tunnelDepth = CONVEYOR_WIDTH * 1.5f;
    float tunnelLength = 2.5f;
    float wallThick = 0.4f;

    for (int i = 0; i < 2; i++) {
        float direction = (i == 0) ? 1.0f : -1.0f;
        float posX = (i == 0) ? -edgeX : edgeX;

        glPushMatrix();
        glTranslatef(posX, 0.0f, 0.0f);

        glEnable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureMetal);
        glColor3f(1.0f, 1.0f, 1.0f);

        // Ściana boczna daleka
        glPushMatrix();
        glTranslatef(direction * tunnelLength / 2, tunnelHeight / 2, -tunnelDepth / 2);
        glScalef(tunnelLength, tunnelHeight, wallThick);
        DrawCube();
        glPopMatrix();

        // Ściana boczna bliska
        glPushMatrix();
        glTranslatef(direction * tunnelLength / 2, tunnelHeight / 2, tunnelDepth / 2);
        glScalef(tunnelLength, tunnelHeight, wallThick);
        DrawCube();
        glPopMatrix();

        // Dach tunelu
        glPushMatrix();
        glTranslatef(direction * tunnelLength / 2, tunnelHeight, 0);
        glScalef(tunnelLength, wallThick, tunnelDepth + wallThick);
        DrawCube();
        glPopMatrix();

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_LIGHTING);

        // Czarne wnętrze tunelu (symulacja głębi)
        glColor3f(0.01f, 0.01f, 0.01f);
        glPushMatrix();
        glTranslatef(direction * 0.05f, tunnelHeight / 2, 0);
        glScalef(0.1f, tunnelHeight - 0.2f, tunnelDepth - wallThick);
        DrawCube();
        glPopMatrix();

        glEnable(GL_LIGHTING);
        glPopMatrix();
    }
}

// Rysuje sześcian jednostkowy (-0.5 do +0.5) z normalnymi i UV
void DrawCube(void) {
    glPushMatrix();
    glScalef(0.5f, 0.5f, 0.5f);

    glBegin(GL_QUADS);
    // Przód (+Z)
    glNormal3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1, -1, 1);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(1, -1, 1);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(1, 1, 1);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1, 1, 1);

    // Tył (-Z)
    glNormal3f(0.0f, 0.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1, -1, -1);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1, 1, -1);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(1, 1, -1);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(1, -1, -1);

    // Góra (+Y)
    glNormal3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1, 1, -1);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1, 1, 1);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(1, 1, 1);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(1, 1, -1);

    // Dół (-Y)
    glNormal3f(0.0f, -1.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1, -1, -1);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(1, -1, -1);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(1, -1, 1);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1, -1, 1);

    // Prawo (+X)
    glNormal3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(1, -1, -1);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(1, 1, -1);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(1, 1, 1);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(1, -1, 1);

    // Lewo (-X)
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1, -1, -1);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1, -1, 1);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1, 1, 1);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1, 1, -1);

    glEnd();
    glPopMatrix();
}


// Rysuje robota spawalniczego (stacjonarny, 3 osie obrotu)
// offsetX, offsetZ - pozycja bazy, rotationY - orientacja całego robota
void DrawROBOT1(SimulationData* sim, float offsetX, float offsetZ, float rotationY) {
    // Wymiary segmentów ramienia (w jednostkach sceny)
    float baseToA2 = 2.90f;      // Wysokość od bazy do osi 2 (bark)
    float armA2toA3 = 2.70f;     // Długość ramienia dolnego
    float armA3toA5 = 2.80f;     // Długość ramienia górnego
    float wristToTCP = 0.20f;    // Odległość od nadgarstka do końcówki narzędzia

    // Paleta kolorów robota przemysłowego
    float colWhite[] = { 0.96f, 0.96f, 0.94f };   // Obudowa
    float colJoint[] = { 0.80f, 0.80f, 0.82f };   // Stawy
    float colDark[] = { 0.15f, 0.15f, 0.18f };    // Akcenty
    float colBlack[] = { 0.05f, 0.05f, 0.05f };
    float colRed[] = { 0.85f, 0.10f, 0.10f };   

    // Baza robota (Oś 1 - obrót poziomy)
    glPushMatrix();
    glTranslatef(ROBOT_OFFSET_X + offsetX, ROBOT_OFFSET_Y, offsetZ);
    glRotatef(rotationY, 0.0f, 1.0f, 0.0f);

    // Podstawa montażowa
    glColor3fv(colWhite);
    glPushMatrix();
    glTranslatef(0.0f, 0.1f, 0.0f);
    glScalef(2.0f, 0.2f, 2.0f);
    DrawCylinder();
    glPopMatrix();

    // Kolumna obrotowa
    glPushMatrix();
    glTranslatef(0.0f, 0.7f, 0.0f);
    glScalef(1.7f, 1.2f, 1.7f);
    DrawCylinder();
    glPopMatrix();

    glRotatef(sim->angleBase, 0.0f, 1.0f, 0.0f);  // Obrót osi 1

    // Korpus z widłami do ramienia
    glColor3fv(colWhite);
    glPushMatrix();
    glTranslatef(0.3f, 2.0f, 0.0f);

    // Lewe widły
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.5f);
    glScalef(1.0f, 2.5f, 0.3f);
    DrawCube();
    glPopMatrix();

    // Prawe widły
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, 0.5f);
    glScalef(1.0f, 2.5f, 0.3f);
    DrawCube();
    glPopMatrix();

    glPopMatrix();

    glColor3fv(colRed);
    glPushMatrix();
    glTranslatef(0.2f, 2.0f, 0.61f);
    glScalef(0.5f, 0.15f, 0.02f);
    DrawCube();
    glPopMatrix();

    // Ramię dolne (Oś 2 - bark)
    glTranslatef(0.2f, baseToA2, 0.0f);

    // Staw barkowy (cylinder poziomy)
    glColor3fv(colJoint);
    glPushMatrix();
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glScalef(1.4f, 1.4f, 1.4f);
    DrawCylinder();
    glPopMatrix();

    glRotatef(sim->angleArm1, 0.0f, 0.0f, 1.0f);  // Obrót osi 2

    // Segment ramienia dolnego
    glColor3fv(colWhite);
    glPushMatrix();
    glTranslatef(0.0f, armA2toA3 / 2.0f, 0.0f);
    glScalef(1.0f, armA2toA3, 0.8f);
    DrawCube();
    glPopMatrix();

    // Osłona kabli
    glColor3fv(colBlack);
    glPushMatrix();
    glTranslatef(-0.55f, armA2toA3 / 2.0f, 0.0f);
    glScalef(0.1f, armA2toA3 * 0.8f, 0.3f);
    DrawCube();
    glPopMatrix();

    // Ramię górne (Oś 3 - łokieć)
    glTranslatef(0.0f, armA2toA3, 0.0f);

    glColor3fv(colJoint);
    glPushMatrix();
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glScalef(1.2f, 1.2f, 1.2f);
    DrawCylinder();
    glPopMatrix();

    glRotatef(sim->angleArm2, 0.0f, 0.0f, 1.0f);  // Obrót osi 3

    // Segment ramienia górnego (przedramię)
    glColor3fv(colWhite);
    glPushMatrix();
    glTranslatef(0.0f, armA3toA5 / 2.0f, 0.0f);
    glScalef(0.9f, armA3toA5, 0.9f);
    DrawCube();
    glPopMatrix();

    // Nadgarstek i narzędzie spawalnicze
    glTranslatef(0.0f, armA3toA5, 0.0f);

    glColor3fv(colJoint);
    glPushMatrix();
    glScalef(0.7f, 0.7f, 0.7f);
    DrawCube();
    glPopMatrix();

    glTranslatef(0.0f, wristToTCP, 0.0f);

    // Uchwyt spawalniczy
    glColor3f(0.12f, 0.12f, 0.12f);
    glPushMatrix();
    glTranslatef(0.0f, 0.4f, 0.0f);
    glScalef(0.35f, 0.8f, 0.35f);
    DrawCube();
    glPopMatrix();

    // Przewody niebieskie (gaz osłonowy)
    glColor3f(0.1f, 0.3f, 0.8f);
    glPushMatrix();
    glTranslatef(0.15f, 0.4f, 0.0f);
    glScalef(0.08f, 0.7f, 0.08f);
    DrawCube();
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-0.15f, 0.4f, 0.0f);
    glScalef(0.08f, 0.7f, 0.08f);
    DrawCube();
    glPopMatrix();

    // Szyjka miedziana i dysza
    glColor3f(0.85f, 0.55f, 0.25f);
    glPushMatrix();
    glTranslatef(0.0f, 0.9f, 0.0f);
    glScalef(0.18f, 0.5f, 0.18f);
    DrawCube();
    glPopMatrix();

    glColor3f(0.7f, 0.8f, 0.9f);
    glPushMatrix();
    glTranslatef(0.0f, 1.0f, 0.0f);
    glScalef(0.25f, 0.4f, 0.25f);
    DrawCube();
    glPopMatrix();

    // Końcówka elektrody
    float toolLength = 1.25f;
    glColor3f(0.9f, 0.9f, 0.85f);
    glPushMatrix();
    glTranslatef(0.0f, toolLength, 0.0f);
    glScalef(0.12f, 0.15f, 0.12f);
    DrawCube();
    glPopMatrix();

    // Efekty spawania (gdy aktywne)
    if (sim->showSparks) {
        glPushMatrix();
        glTranslatef(0.0f, toolLength + 0.1f, 0.0f);

        // Jasny punkt spawania
        glColor3f(1.0f, 1.0f, 0.9f);
        glScalef(0.15f, 0.15f, 0.15f);
        DrawCube();

        // Linie iskier
        DrawSparks(sim->weldTimer);
        glPopMatrix();

        // Generowanie cząsteczek fizycznych w pozycji produktu
        float sparkX = sim->productX;
        float sparkY = CONVEYOR_HEIGHT + 0.45f;
        float sparkZ = 0.0f;
        for (int i = 0; i < 5; i++) {
            SpawnSpark(sparkX, sparkY, sparkZ);
        }
    }
    glPopMatrix();
}



// Rysuje drewnianą skrzynkę z teksturą
void DrawCrate(void) {
    if (textureCrate) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureCrate);
        glColor3f(1.0f, 1.0f, 1.0f);
    }
    else {
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.6f, 0.4f, 0.2f);  // Fallback: brązowy
    }

    glBegin(GL_QUADS);

    // Przód (+Z)
    glNormal3f(0.0f, 0.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, 1.0f, 1.0f);

    // Tył (-Z)
    glNormal3f(0.0f, 0.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, 1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(1.0f, 1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(1.0f, -1.0f, -1.0f);

    // Góra (+Y)
    glNormal3f(0.0f, 1.0f, 0.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, 1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, 1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(1.0f, 1.0f, 1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(1.0f, 1.0f, -1.0f);

    // Dół (-Y)
    glNormal3f(0.0f, -1.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(1.0f, -1.0f, -1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, 1.0f);

    // Prawo (+X)
    glNormal3f(1.0f, 0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(1.0f, 1.0f, -1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(1.0f, -1.0f, 1.0f);

    // Lewo (-X)
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex3f(-1.0f, -1.0f, 1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, 1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, 1.0f, -1.0f);

    glEnd();

    if (textureCrate) glDisable(GL_TEXTURE_2D);
}

// Rysuje regał magazynowy z półkami i skrzynkami
void DrawShelf(void) {
    const float shelfWidth = 8.0f;
    const float shelfDepth = 5.0f;
    const float shelfThick = 0.3f;
    const float shelfSpacing = 2.5f;
    const int shelfCount = 4;
    const float legWidth = 0.4f;
    const float legDepth = 0.4f;
    const float totalHeight = shelfSpacing * (shelfCount - 1) + shelfThick;
    const float backThick = 0.12f;
    const float edgeThick = 0.15f;

    glTranslatef(8.0f, 0.0f, -8.0f);
    glPushMatrix();
    glTranslatef(0.0f, shelfSpacing, 0.0f);

    // Tekstura metalowych nóg
    if (textureMetal > 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureMetal);
        glColor3f(1.0f, 1.0f, 1.0f);
    }
    else {
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.7f, 0.75f, 0.8f);
    }

    // Cztery nogi w narożnikach
    float legPos[4][2] = {
        {-shelfWidth / 2 + 0.5f,  shelfDepth / 2 - 0.5f},
        { shelfWidth / 2 - 0.5f,  shelfDepth / 2 - 0.5f},
        {-shelfWidth / 2 + 0.5f, -shelfDepth / 2 + 0.5f},
        { shelfWidth / 2 - 0.5f, -shelfDepth / 2 + 0.5f}
    };

    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        glTranslatef(legPos[i][0], totalHeight / 2 - shelfSpacing, legPos[i][1]);
        glScalef(legWidth, totalHeight, legDepth);
        DrawCube();
        glPopMatrix();

        // Stopka stabilizująca
        glPushMatrix();
        glTranslatef(legPos[i][0], -shelfSpacing + 0.08f, legPos[i][1]);
        glScalef(legWidth * 1.3f, 0.16f, legDepth * 1.2f);
        DrawCube();
        glPopMatrix();
    }

    glDisable(GL_TEXTURE_2D);

    // Tylna ścianka (płyta pilśniowa)
    if (textureWood > 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureWood);
        glColor3f(0.85f, 0.85f, 0.85f);
    }
    else {
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.55f, 0.40f, 0.22f);
    }

    glPushMatrix();
    glTranslatef(0.0f, totalHeight / 2 - shelfSpacing, -shelfDepth / 2 + backThick);
    glScalef(shelfWidth - 1.0f, totalHeight - 0.4f, backThick);
    DrawCube();
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);

    // Półki i ich zawartość
    for (int i = 0; i < shelfCount; i++) {
        float yPos = (float)i * shelfSpacing - shelfSpacing;

        // Powierzchnia półki
        if (texturePolki > 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texturePolki);
            glColor3f(0.88f, 0.88f, 0.88f);
        }
        else if (textureWood > 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureWood);
            glColor3f(0.9f, 0.9f, 0.9f);
        }
        else {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.65f, 0.45f, 0.25f);
        }

        glPushMatrix();
        glTranslatef(0.0f, yPos, 0.0f);
        glScalef(shelfWidth, shelfThick, shelfDepth);
        DrawCube();
        glPopMatrix();

        // Dekoracyjne skrzynki na wybranych półkach
        if (i == 1) {
            glPushMatrix();
            glTranslatef(-2.5f, yPos + 1.0f, 0.5f);
            glRotatef(10.0f, 0.0f, 1.0f, 0.0f);
            DrawCrate();
            glPopMatrix();

            glPushMatrix();
            glTranslatef(1.0f, yPos + 1.0f, -0.5f);
            glRotatef(-5.0f, 0.0f, 1.0f, 0.0f);
            DrawCrate();
            glPopMatrix();
        }
        else if (i == 2) {
            glPushMatrix();
            glTranslatef(2.0f, yPos + 1.0f, 0.0f);
            glRotatef(-25.0f, 0.0f, 1.0f, 0.0f);
            DrawCrate();
            glPopMatrix();
        }

        glDisable(GL_TEXTURE_2D);

        // Krawędzie zabezpieczające przed spadaniem
        if (i < shelfCount - 1) {
            glColor3f(0.45f, 0.30f, 0.18f);

            // Przednia krawędź
            glPushMatrix();
            glTranslatef(0.0f, yPos + shelfThick / 2 + edgeThick / 2, shelfDepth / 2 - edgeThick);
            glScalef(shelfWidth - 0.2f, edgeThick, edgeThick);
            DrawCube();
            glPopMatrix();

            // Boczne krawędzie
            glPushMatrix();
            glTranslatef(-shelfWidth / 2 + edgeThick, yPos + shelfThick / 2 + edgeThick / 2, 0.0f);
            glScalef(edgeThick, edgeThick, shelfDepth - edgeThick * 2);
            DrawCube();
            glPopMatrix();

            glPushMatrix();
            glTranslatef(shelfWidth / 2 - edgeThick, yPos + shelfThick / 2 + edgeThick / 2, 0.0f);
            glScalef(edgeThick, edgeThick, shelfDepth - edgeThick * 2);
            DrawCube();
            glPopMatrix();
        }
    }

    glPopMatrix();
}

// Rysuje robota lakierniczego na szynie (mobilny, 3 osie + tor jezdny)
void DrawROBOT2(SimulationData* sim, float offsetX, float offsetZ, float rotationY) {
    glPushMatrix();
    glTranslatef(ROBOT_OFFSET_X + offsetX, ROBOT_OFFSET_Y, offsetZ);
    glRotatef(rotationY, 0.0f, 1.0f, 0.0f);

    // Wózek jezdny (platforma na szynach)
    glColor3f(0.18f, 0.18f, 0.20f);
    glPushMatrix();
    glTranslatef(0.0f, 0.14f, 0.0f);
    glScalef(2.5f, 0.08f, 2.5f);
    DrawCube();
    glPopMatrix();

    // Osłony boczne wózka
    glPushMatrix();
    glTranslatef(-1.3f, 0.10f, 0.0f);
    glScalef(0.15f, 0.14f, 2.5f);
    DrawCube();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(1.3f, 0.10f, 0.0f);
    glScalef(0.15f, 0.14f, 2.5f);
    DrawCube();
    glPopMatrix();

    // Śruby mocujące (detal wizualny)
    glColor3f(0.8f, 0.8f, 0.8f);
    float boltDist = 1.0f;
    for (int bx = -1; bx <= 1; bx += 2) {
        for (int bz = -1; bz <= 1; bz += 2) {
            glPushMatrix();
            glTranslatef(bx * boltDist, 0.185f, bz * boltDist);
            glScalef(0.08f, 0.02f, 0.08f);
            DrawCube();
            glPopMatrix();
        }
    }

    // Silnik napędowy toru
    glPushMatrix();
    glTranslatef(0.9f, 0.25f, -1.0f);

    // Przekładnia planetarna
    glColor3f(0.2f, 0.2f, 0.2f);
    glPushMatrix();
    glScalef(0.5f, 0.25f, 0.5f);
    DrawCube();
    glPopMatrix();

    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glTranslatef(0.0f, 0.3f, 0.0f);

    // Radiator silnika (żebrowany)
    for (int i = 0; i < 5; i++) {
        glColor3f(0.1f, 0.1f, 0.1f);
        glPushMatrix();
        glTranslatef(0.0f, i * 0.15f, 0.0f);
        glScalef(0.32f, 0.05f, 0.32f);
        DrawCylinder();
        glPopMatrix();

        glColor3f(0.3f, 0.3f, 0.3f);
        glPushMatrix();
        glTranslatef(0.0f, i * 0.15f + 0.07f, 0.0f);
        glScalef(0.25f, 0.1f, 0.25f);
        DrawCylinder();
        glPopMatrix();
    }

    // Enkoder (czerwony)
    glColor3f(0.8f, 0.2f, 0.2f);
    glPushMatrix();
    glTranslatef(0.0f, 0.8f, 0.0f);
    glScalef(0.28f, 0.1f, 0.28f);
    DrawCylinder();
    glPopMatrix();

    // Wyjście kablowe
    glColor3f(0.1f, 0.1f, 0.1f);
    glPushMatrix();
    glTranslatef(0.25f, 0.7f, 0.0f);
    glScalef(0.1f, 0.1f, 0.15f);
    DrawCube();
    glPopMatrix();
    glPopMatrix();

    // Prowadnik kabli (pomarańczowy)
    glColor3f(1.0f, 0.6f, 0.0f);
    glPushMatrix();
    glTranslatef(-0.6f, 0.15f, -1.35f);
    glScalef(0.4f, 0.1f, 0.2f);
    DrawCube();

    glColor3f(0.1f, 0.1f, 0.1f);
    glTranslatef(0.0f, 0.2f, 0.1f);
    glRotatef(-20.0f, 1.0f, 0.0f, 0.0f);
    glScalef(0.8f, 3.0f, 0.5f);
    DrawCube();
    glPopMatrix();

    // Pasy ostrzegawcze BHP (żółto-czarne)
    float stripZ = 1.32f;
    for (float sx = -1.2f; sx < 1.2f; sx += 0.4f) {
        glColor3f(0.9f, 0.8f, 0.0f);
        glPushMatrix(); glTranslatef(sx, 0.14f, stripZ); glScalef(0.2f, 0.12f, 0.03f); DrawCube(); glPopMatrix();
        glPushMatrix(); glTranslatef(sx, 0.14f, -stripZ); glScalef(0.2f, 0.12f, 0.03f); DrawCube(); glPopMatrix();

        glColor3f(0.1f, 0.1f, 0.1f);
        glPushMatrix(); glTranslatef(sx + 0.2f, 0.14f, stripZ); glScalef(0.2f, 0.12f, 0.03f); DrawCube(); glPopMatrix();
        glPushMatrix(); glTranslatef(sx + 0.2f, 0.14f, -stripZ); glScalef(0.2f, 0.12f, 0.03f); DrawCube(); glPopMatrix();
    }

    // Podstawa kolumny
    glColor3f(0.05f, 0.05f, 0.05f);
    glPushMatrix();
    glTranslatef(0.0f, 0.02f, 0.0f);
    glScalef(2.0f, 1.0f, 2.0f);
    DrawCube();
    glPopMatrix();

    glColor3f(0.25f, 0.25f, 0.28f);
    glPushMatrix();
    glTranslatef(0.0f, 0.08f, 0.0f);
    glScalef(3.5f, 0.16f, 3.5f);
    DrawCube();
    glPopMatrix();

    // Kolumna obrotowa (Oś 1)
    glPushMatrix();
    glTranslatef(0.0f, 0.5f, 0.0f);
    glRotatef(sim->angleBase, 0.0f, 1.0f, 0.0f);

    // Korpus kolumny (pomarańczowy - kolor KUKA)
    glColor3f(0.92f, 0.35f, 0.08f);
    glPushMatrix();
    glTranslatef(0.0f, 0.4f, 0.0f);
    glScalef(1.8f, 0.8f, 1.8f);
    DrawCube();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.0f, 1.5f, 0.0f);
    glScalef(1.4f, 2.2f, 1.4f);
    DrawCube();
    glPopMatrix();

    // Pierścienie dekoracyjne
    glColor3f(0.12f, 0.12f, 0.12f);
    for (int i = 0; i < 2; i++) {
        glPushMatrix();
        glTranslatef(0.0f, 0.9f + i * 1.0f, 0.0f);
        glScalef(1.5f, 0.12f, 1.5f);
        DrawCube();
        glPopMatrix();
    }

    // Tabliczka znamionowa
    glColor3f(0.95f, 0.95f, 0.95f);
    glPushMatrix();
    glTranslatef(0.0f, 1.8f, 0.72f);
    glScalef(0.8f, 0.4f, 0.02f);
    DrawCube();
    glPopMatrix();

    // Dioda statusu (zielona = OK)
    glColor3f(0.1f, 1.0f, 0.2f);
    glPushMatrix();
    glTranslatef(0.0f, 2.4f, 0.72f);
    glScalef(0.15f, 0.15f, 0.02f);
    DrawCube();
    glPopMatrix();

    glColor3f(0.92f, 0.35f, 0.08f);
    glPushMatrix();
    glTranslatef(0.0f, 2.6f, 0.0f);
    glScalef(1.6f, 0.5f, 1.2f);
    DrawCube();
    glPopMatrix();

    // Staw barkowy (Oś 2)
    glColor3f(0.92f, 0.35f, 0.08f);
    glPushMatrix();
    glTranslatef(0.0f, 2.6f, 0.0f);
    glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
    glScalef(1.4f, 1.8f, 1.4f);
    DrawCylinder();
    glPopMatrix();

    // Ramię główne
    glPushMatrix();
    glTranslatef(0.0f, 2.8f, 0.0f);
    glRotatef(sim->angleArm1, 0.0f, 0.0f, 1.0f);

    glColor3f(0.92f, 0.35f, 0.08f);
    glPushMatrix();
    glTranslatef(0.0f, 2.2f, 0.0f);
    glScalef(1.0f, 5.0f, 1.0f);
    DrawCube();
    glPopMatrix();

    // Siłowniki hydrauliczne (cylindry wzdłuż ramienia)
    glColor3f(0.1f, 0.1f, 0.12f);
    glPushMatrix();
    glTranslatef(-0.6f, 2.0f, 0.0f);
    glScalef(0.15f, 4.5f, 0.15f);
    DrawCylinder();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.6f, 2.0f, 0.0f);
    glScalef(0.15f, 4.5f, 0.15f);
    DrawCylinder();
    glPopMatrix();

    // Nadgarstek (Oś 3)
    glPushMatrix();
    glTranslatef(0.0f, 4.8f, 0.0f);
    glRotatef(sim->angleArm2, 0.0f, 0.0f, 1.0f);

    glColor3f(0.1f, 0.1f, 0.12f);
    glPushMatrix();
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glScalef(1.5f, 1.4f, 1.5f);
    DrawCylinder();
    glPopMatrix();

    glColor3f(0.92f, 0.35f, 0.08f);
    glPushMatrix();
    glTranslatef(0.0f, 0.35f, 0.0f);
    glScalef(0.95f, 0.95f, 0.95f);
    DrawCube();
    glPopMatrix();

    // Przedramię z osłoną kabli
    glPushMatrix();
    glTranslatef(0.0f, 2.0f, 0.0f);
    glScalef(0.75f, 4.8f, 0.75f);
    DrawCube();
    glPopMatrix();

    glColor3f(0.15f, 0.15f, 0.15f);
    glPushMatrix();
    glTranslatef(0.0f, 1.4f, 0.32f);
    glScalef(0.6f, 4.2f, 0.35f);
    DrawCube();
    glPopMatrix();

    // Głowica lakiernicza (pistolet natryskowy)
    glPushMatrix();
    glTranslatef(0.0f, 3.2f, 0.0f);

    // Adapter montażowy
    glColor3f(0.55f, 0.55f, 0.6f);
    glPushMatrix();
    glTranslatef(0.0f, 0.3f, 0.0f);
    glScalef(0.6f, 0.6f, 0.6f);
    DrawCube();
    glPopMatrix();

    // Korpus pistoletu
    glColor3f(0.25f, 0.25f, 0.28f);
    glPushMatrix();
    glTranslatef(0.0f, 0.8f, 0.0f);
    glScalef(0.5f, 1.0f, 0.5f);
    DrawCube();
    glPopMatrix();

    // Pokrętło regulacji (czerwone)
    glColor3f(0.9f, 0.1f, 0.1f);
    glPushMatrix();
    glTranslatef(-0.3f, 0.8f, 0.0f);
    glScalef(0.15f, 0.15f, 0.15f);
    DrawCube();
    glPopMatrix();

    // Przewód powietrza (żółty)
    glColor3f(0.95f, 0.85f, 0.1f);
    glPushMatrix();
    glTranslatef(0.2f, 0.7f, 0.0f);
    glScalef(0.12f, 0.8f, 0.12f);
    DrawCube();
    glPopMatrix();

    // Zbiornik farby (niebieski, przezroczysty wizualnie)
    glColor3f(0.6f, 0.75f, 0.9f);
    glPushMatrix();
    glTranslatef(0.0f, 1.4f, 0.0f);
    glScalef(0.35f, 0.7f, 0.35f);
    DrawCube();
    glPopMatrix();

    // Pokrywka zbiornika
    glColor3f(0.2f, 0.2f, 0.22f);
    glPushMatrix();
    glTranslatef(0.0f, 1.8f, 0.0f);
    glScalef(0.25f, 0.1f, 0.25f);
    DrawCube();
    glPopMatrix();

    // Dysza natryskowa
    glColor3f(0.85f, 0.85f, 0.9f);
    glPushMatrix();
    glTranslatef(0.0f, 2.05f, 0.0f);
    glScalef(0.2f, 0.2f, 0.2f);
    DrawCube();
    glPopMatrix();

    // Końcówka dyszy
    glColor3f(0.4f, 0.4f, 0.45f);
    glPushMatrix();
    glTranslatef(0.0f, 2.2f, 0.0f);
    glScalef(0.15f, 0.15f, 0.15f);
    DrawCube();
    glPopMatrix();

    // Efekt lakierowania (gdy aktywny)
    if (sim->showSparks) {
        glPushMatrix();
        glTranslatef(0.0f, 2.3f, 0.0f);

        // Generowanie cząsteczek sprayu w kierunku produktu (w dół)
        float sprayX = sim->productX;
        float sprayY = CONVEYOR_HEIGHT + 0.4f + 1.5f;
        float sprayZ = 0.0f;
        for (int i = 0; i < 8; i++)
            SpawnSpray(sprayX, sprayY, sprayZ, 0.0f, -1.0f, 0.0f, sim->paintR, sim->paintG, sim->paintB);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_LIGHTING);

        // Stożek mgły lakierniczej (pulsujący)
        float intensity = ((float)sin(sim->weldTimer * 0.15) + 1.0f) * 0.5f;
        glColor4f(sim->paintR, sim->paintG, sim->paintB, 0.4f * intensity);
        glPushMatrix();
        glScalef(0.3f, 0.8f, 0.3f);
        DrawCube();
        glPopMatrix();

        // Warstwy mgły o malejącej gęstości
        glColor4f(0.5f, 0.7f, 1.0f, 0.25f * intensity);
        glPushMatrix();
        glTranslatef(0.0f, -0.2f, 0.0f);
        glScalef(0.5f, 1.0f, 0.5f);
        DrawCube();
        glPopMatrix();

        glColor4f(0.7f, 0.8f, 1.0f, 0.15f * intensity);
        glPushMatrix();
        glTranslatef(0.0f, -0.4f, 0.0f);
        glScalef(0.7f, 1.2f, 0.7f);
        DrawCube();
        glPopMatrix();

        // Turbulencje powietrza (obracające się pierścienie)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        for (int i = 0; i < 3; i++) {
            float offset = -0.3f * i;
            float rotation = (float)fmod(sim->weldTimer * 2.0 + i * 120.0, 360.0);

            glColor4f(0.4f, 0.6f, 0.95f, 0.2f * intensity * (1.0f - i * 0.3f));
            glPushMatrix();
            glTranslatef(0.0f, offset, 0.0f);
            glRotatef(rotation, 0.0f, 1.0f, 0.0f);
            glScalef(0.4f + i * 0.15f, 0.05f, 0.4f + i * 0.15f);
            DrawCube();
            glPopMatrix();
        }

        // Punkt zapłonu (pomarańczowy blask przy dyszy)
        glColor4f(1.0f, 0.7f, 0.3f, 0.6f * intensity);
        glPushMatrix();
        glTranslatef(0.0f, 0.4f, 0.0f);
        glScalef(0.15f, 0.1f, 0.15f);
        DrawCube();
        glPopMatrix();

        glEnable(GL_LIGHTING);
        glDisable(GL_BLEND);
        glPopMatrix();
    }

    glPopMatrix();  // Głowica
    glPopMatrix();  // Nadgarstek
    glPopMatrix();  // Ramię główne
    glPopMatrix();  // Kolumna
    glPopMatrix();  // Robot
}

// Rysuje strefę bezpieczeństwa na podłodze (żółto-czarne pasy BHP)
// x, z - środek strefy, w, d - szerokość i głębokość
void DrawSafetyZone(float x, float z, float w, float d) {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glPushMatrix();
    glTranslatef(x, 0.01f, z);  // Lekko nad podłogą aby uniknąć Z-fighting

    float border = 0.3f;  // Szerokość pasa ostrzegawczego

    // Żółte paski obwodowe (4 krawędzie)
    glBegin(GL_QUADS);
    glColor3f(0.9f, 0.8f, 0.0f);

    // Lewa krawędź
    glVertex3f(-w / 2, 0, -d / 2);
    glVertex3f(-w / 2 + border, 0, -d / 2);
    glVertex3f(-w / 2 + border, 0, d / 2);
    glVertex3f(-w / 2, 0, d / 2);

    // Prawa krawędź
    glVertex3f(w / 2 - border, 0, -d / 2);
    glVertex3f(w / 2, 0, -d / 2);
    glVertex3f(w / 2, 0, d / 2);
    glVertex3f(w / 2 - border, 0, d / 2);

    // Tylna krawędź
    glVertex3f(-w / 2, 0, -d / 2);
    glVertex3f(w / 2, 0, -d / 2);
    glVertex3f(w / 2, 0, -d / 2 + border);
    glVertex3f(-w / 2, 0, -d / 2 + border);

    // Przednia krawędź - z przerwą na przejście (między -2 a +2)
    glVertex3f(-w / 2, 0, d / 2 - border);
    glVertex3f(-2.0f, 0, d / 2 - border);
    glVertex3f(-2.0f, 0, d / 2);
    glVertex3f(-w / 2, 0, d / 2);

    glVertex3f(2.0f, 0, d / 2 - border);
    glVertex3f(w / 2, 0, d / 2 - border);
    glVertex3f(w / 2, 0, d / 2);
    glVertex3f(2.0f, 0, d / 2);
    glEnd();

    // Czarne ukośne linie (wzór zebry)
    glColor3f(0.1f, 0.1f, 0.1f);
    glBegin(GL_LINES);
    glLineWidth(2.0f);

    // Linie na tylnej krawędzi
    for (float i = -w / 2; i < w / 2; i += 0.5f) {
        glVertex3f(i, 0.02f, -d / 2);
        glVertex3f(i + 0.2f, 0.02f, -d / 2 + border);

        // Linie na przedniej krawędzi (z pominięciem przejścia)
        if (i < -2.0f || i > 2.0f) {
            glVertex3f(i, 0.02f, d / 2 - border);
            glVertex3f(i + 0.2f, 0.02f, d / 2);
        }
    }

    // Linie na bokach
    for (float i = -d / 2; i < d / 2; i += 0.5f) {
        glVertex3f(-w / 2, 0.02f, i);
        glVertex3f(-w / 2 + border, 0.02f, i + 0.2f);

        glVertex3f(w / 2 - border, 0.02f, i);
        glVertex3f(w / 2, 0.02f, i + 0.2f);
    }
    glEnd();

    glEnable(GL_LIGHTING);
    glPopMatrix();
}

// Rysuje szyny jezdne dla robota lakierniczego
// x - pozycja X szyny, y - wysokość, zStart/zEnd - zakres ruchu
void DrawRailsForRobot(float x, float y, float zStart, float zEnd) {
    glDisable(GL_TEXTURE_2D);

    float length = fabs(zEnd - zStart);
    float centerZ = (zStart + zEnd) / 2.0f;
    float railSpacing = 2.0f;  // Rozstaw szyn

    //podkład
    glColor3f(0.25f, 0.25f, 0.28f);
    glPushMatrix();
    glTranslatef(x, y - 0.04f, centerZ);
    glScalef(railSpacing + 0.8f, 0.05f, length + 1.0f);
    DrawCube();
    glPopMatrix();

    // Dwie równoległe szyny
    glColor3f(0.7f, 0.75f, 0.8f);
    float railWidth = 0.12f;
    float railHeight = 0.12f;

    // Lewa szyna
    glPushMatrix();
    glTranslatef(x - railSpacing / 2, y + railHeight / 2, centerZ);
    glScalef(railWidth, railHeight, length + 0.8f);
    DrawCube();
    glPopMatrix();

    // Prawa szyna
    glPushMatrix();
    glTranslatef(x + railSpacing / 2, y + railHeight / 2, centerZ);
    glScalef(railWidth, railHeight, length + 0.8f);
    DrawCube();
    glPopMatrix();

    // Ograniczniki krańcowe (zderzaki)
    glColor3f(0.1f, 0.1f, 0.1f);
    float stopSize = 0.2f;

    // Przednie ograniczniki
    glPushMatrix();
    glTranslatef(x - railSpacing / 2, y + railHeight, zStart - 0.4f);
    glScalef(stopSize, stopSize, stopSize);
    DrawCube();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(x + railSpacing / 2, y + railHeight, zStart - 0.4f);
    glScalef(stopSize, stopSize, stopSize);
    DrawCube();
    glPopMatrix();

    // Tylne ograniczniki
    glPushMatrix();
    glTranslatef(x - railSpacing / 2, y + railHeight, zEnd + 0.4f);
    glScalef(stopSize, stopSize, stopSize);
    DrawCube();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(x + railSpacing / 2, y + railHeight, zEnd + 0.4f);
    glScalef(stopSize, stopSize, stopSize);
    DrawCube();
    glPopMatrix();

    glEnable(GL_TEXTURE_2D);
}

// Rysuje lampę przemysłową z efektem świetlnym
// W trybie nocnym generuje stożek światła (volumetric fake)
void DrawLamp(float x, float height, float z) {
    glPushMatrix();
    glTranslatef(x, height, z);
    glScalef(2.5f, 2.5f, 2.5f);

    // Przewód zasilający
    glColor3f(0.1f, 0.1f, 0.1f);
    glPushMatrix();
    glTranslatef(0.0f, 1.0f, 0.0f);
    glScalef(0.05f, 2.0f, 0.05f);
    DrawCube();
    glPopMatrix();

    // Klosz lampy (piramida ścięta)
    glColor3f(0.2f, 0.2f, 0.25f);
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.5f, 0.0f); glVertex3f(-0.5f, 0.0f, 0.5f); glVertex3f(0.5f, 0.0f, 0.5f);
    glVertex3f(0.0f, 0.5f, 0.0f); glVertex3f(0.5f, 0.0f, 0.5f); glVertex3f(0.5f, 0.0f, -0.5f);
    glVertex3f(0.0f, 0.5f, 0.0f); glVertex3f(0.5f, 0.0f, -0.5f); glVertex3f(-0.5f, 0.0f, -0.5f);
    glVertex3f(0.0f, 0.5f, 0.0f); glVertex3f(-0.5f, 0.0f, -0.5f); glVertex3f(-0.5f, 0.0f, 0.5f);
    glEnd();

    // Żarówka - materiał emitujący światło
    GLfloat lightEmit[] = { 1.0f, 0.9f, 0.7f, 1.0f };
    GLfloat noEmit[] = { 0.0f, 0.0f, 0.0f, 1.0f };

    glMaterialfv(GL_FRONT, GL_EMISSION, lightEmit);
    glColor3f(1.0f, 1.0f, 0.9f);

    glPushMatrix();
    glTranslatef(0.0f, -0.1f, 0.0f);
    glScalef(0.3f, 0.15f, 0.3f);
    DrawCube();
    glPopMatrix();

    glMaterialfv(GL_FRONT, GL_EMISSION, noEmit);

    // Efekt volumetryczny światła (tylko w trybie nocnym)
    if (isNightMode) {
        float glowR = 1.0f, glowG = 0.85f, glowB = 0.55f;

        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);  // Blending addytywny
        glDepthMask(GL_FALSE);

        const int segments = 24;
        float apexY = -0.15f;      // Wierzchołek stożka (przy żarówce)
        float baseY = -0.9f;       // Podstawa stożka (na podłodze)
        float baseRadius = 1.4f;

        // Dwie warstwy stożka o różnej przezroczystości
        for (int layer = 0; layer < 2; ++layer) {
            float layerAlpha = (layer == 0) ? 0.35f : 0.14f;
            float radiusScale = (layer == 0) ? 1.0f : 1.4f;
            float r = baseRadius * radiusScale;

            glColor4f(glowR, glowG, glowB, layerAlpha);
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(0.0f, apexY, 0.0f);
            for (int i = 0; i <= segments; ++i) {
                float ang = (float)i / (float)segments * 2.0f * 3.14159265f;
                float cx = cosf(ang) * r;
                float cz = sinf(ang) * r;
                glVertex3f(cx, baseY, cz);
            }
            glEnd();
        }

        // Jasne centrum przy żarówce
        glColor4f(glowR, glowG, glowB, 0.55f);
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(0.0f, -0.22f, 0.0f);
        for (int i = 0; i <= segments; ++i) {
            float ang = (float)i / (float)segments * 2.0f * 3.14159265f;
            float cx = cosf(ang) * 0.45f;
            float cz = sinf(ang) * 0.45f;
            glVertex3f(cx, -0.22f, cz);
        }
        glEnd();

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
    }

    glPopMatrix();
}

// Rysuje uproszczony model samochodu (produkt typu 2)
void DrawCar(void) {
    glPushMatrix();
    glScalef(0.5f, 0.5f, 0.5f);
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);  // Obrót aby jechał wzdłuż taśmy

    // Nadwozie dolne
    glPushMatrix();
    glScalef(1.0f, 0.3f, 2.2f);
    DrawCube();
    glPopMatrix();

    // Kabina (nadwozie górne)
    glPushMatrix();
    glTranslatef(0.0f, 0.35f, -0.2f);
    glScalef(0.9f, 0.4f, 1.0f);
    DrawCube();
    glPopMatrix();

    // Koła
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.1f, 0.1f, 0.1f);

    float wheelX = 0.6f;   // Odległość od osi symetrii
    float wheelZ = 1.2f;   // Rozstaw osi
    float wheelY = -0.3f;  // Wysokość nad podłożem

    for (int i = 0; i < 4; i++) {
        glPushMatrix();
        float wx = (i % 2 == 0) ? wheelX : -wheelX;
        float wz = (i < 2) ? wheelZ : -wheelZ;

        glTranslatef(wx, wheelY, wz);
        glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
        glScalef(0.4f, 0.2f, 0.4f);
        DrawCylinder();
        glPopMatrix();
    }

    glColor3f(1.0f, 1.0f, 1.0f);
    glEnable(GL_TEXTURE_2D);

    glPopMatrix();
}

// Rysuje walec o jednostkowym promieniu i wysokości
void DrawCylinder(void) {
    glPushMatrix();
    glScalef(0.5f, 0.5f, 0.5f);

    const int segments = 16;

    // Powierzchnia boczna
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / segments * 6.283f;
        float x = cosf(angle);
        float z = sinf(angle);
        glNormal3f(x, 0.0f, z);
        glVertex3f(x, 1.0f, z);
        glVertex3f(x, -1.0f, z);
    }
    glEnd();

    glNormal3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, 1.0f, 0.0f);
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / segments * 6.283f;
        glVertex3f(cosf(angle), 1.0f, sinf(angle));
    }
    glEnd();

    
    glNormal3f(0.0f, -1.0f, 0.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, -1.0f, 0.0f);
    for (int i = 0; i <= segments; i++) {
        float angle = -(float)i / segments * 6.283f;
        glVertex3f(cosf(angle), -1.0f, sinf(angle));
    }
    glEnd();

    glPopMatrix();
}


// Rysuje dwa stalowe kątowniki 
// Symuluje elementy do spawania
void DrawKatowniki(void) {
    glPushMatrix();
    glScalef(0.5f, 0.5f, 0.5f);

    float len1 = 3.6f;   // Długość głównego kątownika
    float len2 = 2.4f;   // Długość poprzecznego kątownika
    float w = 0.8f;      // Szerokość ramienia kątownika
    float t = 0.20f;     // Grubość materiału

    glTranslatef(1.6f, -0.4f, 1.1f);

    // Kątownik główny (profil L leżący)
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -0.8f);

    // Poziome ramię
    glPushMatrix();
    glTranslatef(0.0f, t / 2.0f, 0.0f);
    glScalef(len1, t, w);
    DrawCube();
    glPopMatrix();

    // Pionowe ramię
    glPushMatrix();
    glTranslatef(0.0f, w / 2.0f, -w / 2.0f + t / 2.0f);
    glScalef(len1, w, t);
    DrawCube();
    glPopMatrix();

    glPopMatrix();

    // Kątownik poprzeczny (obrócony o 90°)
    glPushMatrix();
    glTranslatef(-1.55f, 0.0f, 0.0f);
    glRotatef(90.0f, 0.0f, 1.0f, 0.0f);

    // Poziome ramię
    glPushMatrix();
    glTranslatef(0.0f, t / 2.0f, 0.0f);
    glScalef(len2, t, w);
    DrawCube();
    glPopMatrix();

    // Pionowe ramię
    glPushMatrix();
    glTranslatef(0.0f, w / 2.0f, -w / 2.0f + t / 2.0f);
    glScalef(len2, w, t);
    DrawCube();
    glPopMatrix();

    glPopMatrix();

    glPopMatrix();
}

// Rysuje zabawkową lokomotywę parową (produkt typu 1)
void DrawToyLocomotive(void) {
    glPushMatrix();
    glScalef(1.2f, 1.2f, 1.2f);
    glTranslatef(0.0f, -0.27f, 0.0f);  // Wyrównanie do poziomu taśmy

    // Parametry geometrii
    float wheelR = 0.25f;
    float wheelH = 0.1f;
    float wheelY = wheelR * 0.5f;
    float wheelZ = 0.28f;

    float baseY = wheelY + 0.08f;
    float baseH = 0.12f;
    float baseTop = baseY + baseH * 0.25f;

    float boilerX = 0.25f;
    float boilerLen = 0.59f;
    float cabW = 0.28f;
    float cabX = boilerX - boilerLen * 0.5f - cabW * 0.5f + 0.02f;

    // Koła (czarne)
    glPushAttrib(GL_CURRENT_BIT);
    glColor3f(0.1f, 0.1f, 0.12f);

    // Tylne koła (pod kabiną)
    glPushMatrix();
    glTranslatef(cabX, wheelY, wheelZ);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glScalef(wheelR * 0.9f, wheelH, wheelR * 0.9f);
    DrawCylinder();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(cabX, wheelY, -wheelZ);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glScalef(wheelR * 0.9f, wheelH, wheelR * 0.9f);
    DrawCylinder();
    glPopMatrix();

    // Przednie koła (mniejsze)
    glPushMatrix();
    glTranslatef(0.55f, wheelY * 0.9f, wheelZ);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glScalef(wheelR * 0.75f, wheelH, wheelR * 0.75f);
    DrawCylinder();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(0.55f, wheelY * 0.9f, -wheelZ);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glScalef(wheelR * 0.75f, wheelH, wheelR * 0.75f);
    DrawCylinder();
    glPopMatrix();

    glPopAttrib();

    // Podwozie
    float baseLength = 0.95f;
    float baseOffset = 0.12f;

    glPushMatrix();
    glTranslatef(baseOffset, baseY, 0.0f);
    glScalef(baseLength, baseH, 0.5f);
    DrawCube();
    glPopMatrix();

    // Kocioł (cylinder poziomy)
    float boilerR = 0.22f;
    float boilerY = baseTop + boilerR * 0.5f + 0.02f;

    glPushMatrix();
    glTranslatef(boilerX, boilerY, 0.0f);
    glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
    glScalef(boilerR, boilerLen, boilerR);
    DrawCylinder();
    glPopMatrix();

    // Przód kotła (dysk)
    glPushMatrix();
    glTranslatef(boilerX + boilerLen * 0.5f + 0.02f, boilerY, 0.0f);
    glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
    glScalef(boilerR * 0.8f, 0.04f, boilerR * 0.8f);
    DrawCylinder();
    glPopMatrix();

    // Kabina maszynisty
    float cabH = 0.35f;
    float cabD = 0.38f;
    float cabY = baseTop + cabH * 0.5f + 0.02f;

    glPushMatrix();
    glTranslatef(cabX, cabY, 0.0f);
    glScalef(cabW, cabH, cabD);
    DrawCube();
    glPopMatrix();

    // Dach kabiny
    float roofY = cabY + cabH * 0.5f + 0.025f;
    glPushMatrix();
    glTranslatef(cabX, roofY, 0.0f);
    glScalef(cabW + 0.04f, 0.05f, cabD + 0.04f);
    DrawCube();
    glPopMatrix();

    // Okna kabiny (niebieskawe)
    glPushAttrib(GL_CURRENT_BIT);
    glColor3f(0.5f, 0.65f, 0.78f);
    float winY = cabY + 0.05f;
    float winZ = cabD * 0.5f + 0.005f;

    glPushMatrix();
    glTranslatef(cabX, winY, winZ);
    glScalef(cabW * 0.7f, cabH * 0.4f, 0.01f);
    DrawCube();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(cabX, winY, -winZ);
    glScalef(cabW * 0.7f, cabH * 0.4f, 0.01f);
    DrawCube();
    glPopMatrix();
    glPopAttrib();

    // Komin
    glPushAttrib(GL_CURRENT_BIT);
    glColor3f(0.1f, 0.1f, 0.12f);
    float chimX = boilerX + 0.12f;
    float chimBaseY = boilerY + boilerR * 0.5f;
    float chimH = 0.2f;
    float chimR = 0.08f;

    // Trzon komina
    glPushMatrix();
    glTranslatef(chimX, chimBaseY + chimH * 0.5f, 0.0f);
    glScalef(chimR, chimH, chimR);
    DrawCylinder();
    glPopMatrix();

    // Kołnierz komina
    glPushMatrix();
    glTranslatef(chimX, chimBaseY + chimH + 0.02f, 0.0f);
    glScalef(chimR + 0.03f, 0.04f, chimR + 0.03f);
    DrawCylinder();
    glPopMatrix();
    glPopAttrib();

    // Przedni zderzak
    float bumperX = boilerX + boilerLen * 0.5f + 0.08f;
    glPushMatrix();
    glTranslatef(bumperX, baseY, 0.0f);
    glScalef(0.05f, 0.1f, 0.35f);
    DrawCube();
    glPopMatrix();

    // Sprzęg tylny
    glPushMatrix();
    glTranslatef(cabX - cabW * 0.5f - 0.06f, baseY, 0.0f);
    glScalef(0.06f, 0.05f, 0.1f);
    DrawCube();
    glPopMatrix();

    glPopMatrix();
}

// Konfiguruje mgłę atmosferyczną (FOG)
// Tworzy efekt głębi i dystansu w hali
void SetupFog(void) {
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);  // Wykładniczy spadek widoczności

    GLfloat fogColor[4];
    if (isNightMode) {
        fogColor[0] = 0.15f; fogColor[1] = 0.15f; fogColor[2] = 0.20f; fogColor[3] = 1.0f;
    }
    else {
        fogColor[0] = 0.12f; fogColor[1] = 0.12f; fogColor[2] = 0.14f; fogColor[3] = 1.0f;
    }

    glFogfv(GL_FOG_COLOR, fogColor);
    glFogf(GL_FOG_DENSITY, 0.015f);  // Gęstość mgły
    glHint(GL_FOG_HINT, GL_NICEST);
}


// Główna funkcja renderująca - wywoływana przy każdym WM_PAINT
void RenderScene(void) {
    // Kolor tła zależny od trybu
    if (isNightMode)
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    else
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);

    SetupFog();
    SetupLighting();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Obliczenie punktu patrzenia kamery
    float lookX = camera.x + cosf(camera.yaw * DEG2RAD) * cosf(camera.pitch * DEG2RAD);
    float lookY = camera.y + sinf(camera.pitch * DEG2RAD);
    float lookZ = camera.z + sinf(camera.yaw * DEG2RAD) * cosf(camera.pitch * DEG2RAD);

    gluLookAt(camera.x, camera.y, camera.z,
        lookX, lookY, lookZ,
        0.0f, 1.0f, 0.0f);

    // Zapisanie pozycji kamery do optymalizacji (culling)
    cameraEyeX = camera.x;
    cameraEyeY = camera.y;
    cameraEyeZ = camera.z;

    // Rysowanie statycznych elementów sceny
    DrawFloor();
    DrawWalls();
    DrawWentylacja();

    glEnable(GL_COLOR_MATERIAL);
    DrawTasma();
    DrawTasmaEndCovers();

    // Wentylatory na tylnej ścianie
    DrawWallFan(-8.0f, 18.0f, -FLOOR_SIZE + 0.6f);
    DrawWallFan(0.0f, 18.0f, -FLOOR_SIZE + 0.6f);
    DrawWallFan(8.0f, 18.0f, -FLOOR_SIZE + 0.6f);

    // Lampy sufitowe
    DrawLamp(-5.0f, 18.0f, 7.0f);
    DrawLamp(5.0f, 18.0f, 7.0f);
    DrawLamp(0.0f, 18.0f, 0.0f);

    // Strefy bezpieczeństwa wokół robotów
    DrawSafetyZone(-5.0f, 6.3f, 6.0f, 6.0f);
    DrawSafetyZone(5.0f, 9.3f, 6.0f, 6.0f);

    // Produkt na stacji 1 (spawanie)
    if (sim1.isActive) {
        glPushMatrix();
        glTranslatef(sim1.productX, CONVEYOR_HEIGHT + 0.35f, 0.0f);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureMetal);
        glColor3f(0.7f, 0.7f, 0.75f);

        // Wybór modelu na podstawie typu produktu
        if (sim1.productType == 2) {
            glScalef(1.2f, 1.2f, 1.2f);
            DrawCar();
        }
        else if (sim1.productType == 1) {
            DrawToyLocomotive();
        }
        else {
            DrawKatowniki();
        }

        glDisable(GL_TEXTURE_2D);
        glPopMatrix();
    }

    // Produkt na stacji 2 (lakierowanie) z animacją zmiany koloru
    if (sim2.isActive) {
        glPushMatrix();
        glTranslatef(sim2.productX, CONVEYOR_HEIGHT + 0.35f, 0.0f);

        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, textureMetal);

        // Interpolacja koloru: surowy metal -> docelowy lakier
        float rawR = 0.7f, rawG = 0.7f, rawB = 0.75f;
        float progress = sim2.weldTimer / WELD_DURATION;
        if (progress > 1.0f) progress = 1.0f;
        else if (progress < 0.0f) progress = 0.0f;

        float r = rawR + (sim2.paintR - rawR) * progress;
        float g = rawG + (sim2.paintG - rawG) * progress;
        float b = rawB + (sim2.paintB - rawB) * progress;
        glColor3f(r, g, b);

        if (sim2.productType == 2) {
            glScalef(1.2f, 1.2f, 1.2f);
            DrawCar();
        }
        else if (sim2.productType == 1) {
            DrawToyLocomotive();
        }
        else {
            DrawKatowniki();
        }

        glDisable(GL_TEXTURE_2D);
        glPopMatrix();
    }

    // Okrąg zaznaczenia w trybie konfiguracji
    if (isConfigMode) {
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.0f, 1.0f, 0.0f);
        glPushMatrix();

        if (configSelectedRobot == 1)
            glTranslatef(-5.0f, 0.1f, 6.5f);
        else
            glTranslatef(5.0f, 0.1f, paintingRobotZ);

        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 36; i++) {
            float theta = 2.0f * 3.1415926f * (float)i / 36.0f;
            glVertex3f(3.0f * cosf(theta), 0.0f, 3.0f * sinf(theta));
        }
        glEnd();

        glEnable(GL_LIGHTING);
        glPopMatrix();
    }

    // Roboty
    DrawROBOT1(&sim1, -5.0f, 6.3f, 0.0f);
    DrawRailsForRobot(5.0f, 0.05f, PAINT_Z_WORK - 2.0f, PAINT_Z_RETRACT + 1.5f);
    DrawROBOT2(&sim2, 5.0f, paintingRobotZ, 0.0f);

    // Efekty cząsteczkowe (iskry, spray)
    DrawParticles();

    // Regały magazynowe
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -10.0f);
    glRotatef(-25.0f, 0.0f, 1.0f, 0.0f);
    DrawShelf();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-13.0f, 0.0f, -3.0f);
    glRotatef(25.0f, 0.0f, 1.0f, 0.0f);
    DrawShelf();
    glPopMatrix();

    glPushMatrix();
    glTranslatef(-7.0f, 0.0f, -7.5f);
    DrawShelf();
    glPopMatrix();

    // Dekoracyjne skrzynki rozrzucone po hali
    glPushMatrix(); glTranslatef(0.0f, 1.5f, 6.0f); glRotatef(15.0f, 0.0f, 1.0f, 0.0f); glScalef(1.5f, 1.5f, 1.5f); DrawCrate(); glPopMatrix();
    glPushMatrix(); glTranslatef(-12.0f, 1.0f, -4.0f); glRotatef(5.0f, 0.0f, 1.0f, 0.0f); DrawCrate(); glPopMatrix();
    glPushMatrix(); glTranslatef(-12.0f, 3.0f, -4.0f); glRotatef(-15.0f, 0.0f, 1.0f, 0.0f); DrawCrate(); glPopMatrix();
    glPushMatrix(); glTranslatef(10.0f, 1.0f, 5.0f); glRotatef(60.0f, 0.0f, 1.0f, 0.0f); DrawCrate(); glPopMatrix();
    glPushMatrix(); glTranslatef(10.0f, 2.0f, -8.0f); glRotatef(-30.0f, 0.0f, 1.0f, 0.0f); glScalef(2.0f, 2.0f, 2.0f); DrawCrate(); glPopMatrix();

    // Panel sterowania - rysowany na końcu
    RECT rect;
    GetClientRect(WindowFromDC(hDC), &rect);
    DrawInterfejs(rect.right, rect.bottom);

    glFlush();
}

// Aktualizuje logikę maszyny stanów dla pojedynczego robota
void UpdateSimulationData(SimulationData* sim) {
    // Gdy robot nieaktywny lub pauza - tylko animacja powrotu do pozycji spoczynkowej
    if (!sim->isActive || isPaused) {
        if (isPaused) return;

        sim->targetAngleArm1 = -30.0f;
        sim->targetAngleArm2 = 60.0f;
        sim->angleArm1 = LerpAngle(sim->angleArm1, sim->targetAngleArm1, ARM_SPEED_SLOW * simulationSpeed);
        sim->angleArm2 = LerpAngle(sim->angleArm2, sim->targetAngleArm2, ARM_SPEED_SLOW * simulationSpeed);
        sim->angleBase = LerpAngle(sim->angleBase, 90.0f, 0.5f * simulationSpeed);
        return;
    }

    float currentSpeed = BASE_PRODUCT_SPEED * simulationSpeed;

    // Kąty robocze różnią się w zależności od typu produktu
    float workAngleArm1, workAngleArm2;
    if (sim == &sim1 && sim->productType == 0) {
        // Kątowniki wymagają niższej pozycji (spawanie w rogu)
        workAngleArm1 = -50.0f;
        workAngleArm2 = -68.0f;
    }
    else if (sim == &sim1) {
        // Lokomotywa i samochód - standardowa pozycja
        workAngleArm1 = -50.0f;
        workAngleArm2 = -60.0f;
    }
    else {
        // Robot lakierniczy - inna geometria
        workAngleArm1 = -45.0f;
        workAngleArm2 = -57.5f;
    }

    switch (sim->state) {
    case STATE_APPROACHING:
        // Produkt jedzie do pozycji robota
        sim->showSparks = 0;
        sim->productX += currentSpeed;

        // Ramię w pozycji oczekiwania
        sim->targetAngleArm1 = -30.0f;
        sim->targetAngleArm2 = 60.0f;
        sim->angleArm1 = LerpAngle(sim->angleArm1, sim->targetAngleArm1, ARM_SPEED_SLOW * simulationSpeed);
        sim->angleArm2 = LerpAngle(sim->angleArm2, sim->targetAngleArm2, ARM_SPEED_SLOW * simulationSpeed);
        sim->angleBase = LerpAngle(sim->angleBase, 90.0f, 0.5f * simulationSpeed);

        if (sim->productX >= sim->stopPosition) {
            sim->productX = sim->stopPosition;
            sim->state = STATE_POSITIONING;
            sim->weldTimer = 0.0f;
        }
        break;

    case STATE_POSITIONING:
        // Robot opuszcza ramię do produktu
        sim->targetAngleArm1 = workAngleArm1;
        sim->targetAngleArm2 = workAngleArm2;
        sim->angleArm1 = LerpAngle(sim->angleArm1, sim->targetAngleArm1, ARM_SPEED_SLOW * simulationSpeed);
        sim->angleArm2 = LerpAngle(sim->angleArm2, sim->targetAngleArm2, ARM_SPEED_SLOW * simulationSpeed);
        sim->angleBase = LerpAngle(sim->angleBase, 90.0f, 0.5f * simulationSpeed);

        // Przejście do spawania gdy ramię osiągnie pozycję
        if (fabs(sim->angleArm1 - sim->targetAngleArm1) < 2.0f) {
            sim->state = STATE_WELDING;
            sim->weldTimer = 0.0f;
        }
        break;

    case STATE_WELDING:
        // Proces spawania/lakierowania z wibracją ramienia
        sim->weldTimer += 1.0f * simulationSpeed;
        {
            // Symulacja drgań podczas pracy
            float vib = (float)(sin(sim->weldTimer * 0.5) * 0.3);
            sim->targetAngleArm1 = workAngleArm1 + vib;
            sim->targetAngleArm2 = workAngleArm2 - vib * 0.5f;

            sim->angleArm1 = LerpAngle(sim->angleArm1, sim->targetAngleArm1, ARM_SPEED_FAST * simulationSpeed);
            sim->angleArm2 = LerpAngle(sim->angleArm2, sim->targetAngleArm2, ARM_SPEED_FASTER * simulationSpeed);

            // Włącz efekty gdy ramię w pozycji roboczej
            if (fabs(sim->angleArm1 - sim->targetAngleArm1) < 2.0f)
                sim->showSparks = 1;

            if (sim->weldTimer > WELD_DURATION) {
                sim->state = STATE_DEPARTING;
                sim->showSparks = 0;
            }
        }
        break;

    case STATE_DEPARTING:
        // Produkt odjeżdża, ramię wraca do góry
        sim->showSparks = 0;
        sim->productX += currentSpeed;

        sim->targetAngleArm1 = -30.0f;
        sim->targetAngleArm2 = 60.0f;
        sim->angleArm1 = LerpAngle(sim->angleArm1, sim->targetAngleArm1, ARM_SPEED_SLOW * simulationSpeed);
        sim->angleArm2 = LerpAngle(sim->angleArm2, sim->targetAngleArm2, ARM_SPEED_SLOW * simulationSpeed);
        sim->angleBase = LerpAngle(sim->angleBase, 90.0f, 0.5f * simulationSpeed);
        break;
    }
}



// Główna pętla aktualizacji logiki - wywoływana przez timer
void UpdateSimulation(void) {
    MoveFreeCamera();

    // W trybie konfiguracji - tylko aktualizacja cząsteczek
    if (isConfigMode) {
        UpdateParticles();
        return;
    }

    if (isPaused) return;

    // Animacja ruchu robota lakierniczego po szynie
    float targetZ = isPaintingStationActive ? PAINT_Z_WORK : PAINT_Z_RETRACT;
    if (fabs(paintingRobotZ - targetZ) > 0.02f) {
        paintingRobotZ += (targetZ - paintingRobotZ) * 0.01f * simulationSpeed;
    }
    else {
        paintingRobotZ = targetZ;
    }

    // Aktualizacja robota spawalniczego
    UpdateSimulationData(&sim1);

    // Gdy stacja lakiernicza wyłączona - przerwij proces
    if (!isPaintingStationActive && (sim2.state == STATE_POSITIONING || sim2.state == STATE_WELDING)) {
        sim2.state = STATE_DEPARTING;
        sim2.showSparks = 0;
    }

    // Aktualizacja robota lakierniczego
    if (sim2.isActive) {
        float currentSpeed = BASE_PRODUCT_SPEED * simulationSpeed;

        if (!isPaintingStationActive) {
            // Stacja wyłączona - produkt przejeżdża bez zatrzymywania
            if (sim2.state == STATE_APPROACHING) {
                sim2.productX += currentSpeed;

                sim2.targetAngleArm1 = -30.0f;
                sim2.targetAngleArm2 = 60.0f;
                sim2.angleArm1 = LerpAngle(sim2.angleArm1, sim2.targetAngleArm1, ARM_SPEED_SLOW * simulationSpeed);
                sim2.angleArm2 = LerpAngle(sim2.angleArm2, sim2.targetAngleArm2, ARM_SPEED_SLOW * simulationSpeed);
                sim2.angleBase = LerpAngle(sim2.angleBase, 90.0f, 0.5f * simulationSpeed);

                if (sim2.productX >= sim2.stopPosition) {
                    sim2.state = STATE_DEPARTING;
                }
            }
            else if (sim2.state == STATE_DEPARTING) {
                UpdateSimulationData(&sim2);
            }
        }
        else {
            UpdateSimulationData(&sim2);
        }
    }

    UpdateParticles();

    // Animacja taśmociągu 
    int beltMoving = 0;
    if (sim1.isActive && sim1.state != STATE_POSITIONING && sim1.state != STATE_WELDING)
        beltMoving = 1;

    if (sim2.isActive) {
        if (!isPaintingStationActive)
            beltMoving = 1;
        else if (sim2.state != STATE_POSITIONING && sim2.state != STATE_WELDING)
            beltMoving = 1;
    }

    if (beltMoving) {
        conveyorTextureOffset -= 0.12f * simulationSpeed;
    }

    // Przekazanie produktu z robota 1 do robota 2
    if (sim1.isActive && sim1.state == STATE_DEPARTING && sim1.productX >= sim1.endX) {
        sim1.isActive = 0;
        sim1.state = STATE_APPROACHING;
        sim1.productX = sim1.startX;

        sim2.isActive = 1;
        sim2.state = STATE_APPROACHING;
        sim2.productX = sim2.startX;

        // Skopiowanie typu produktu i koloru
        sim2.productType = sim1.productType;
        sim2.paintR = sim1.paintR;
        sim2.paintG = sim1.paintG;
        sim2.paintB = sim1.paintB;
        sim2.weldTimer = 0.0f;
    }

    // Zakończenie cyklu - produkt opuszcza scenę
    if (sim2.isActive && sim2.state == STATE_DEPARTING && sim2.productX >= sim2.endX) {
        sim2.isActive = 0;
        sim2.state = STATE_APPROACHING;
        sim2.productX = sim2.startX;

        sim1.isActive = 1;
        sim1.state = STATE_APPROACHING;
        sim1.productX = sim1.startX;

        // Losowanie nowego produktu
        sim1.productType = rand() % 3;
        int colorID = rand() % 4;
        if (colorID == 0) { sim1.paintR = 0.8f; sim1.paintG = 0.1f; sim1.paintB = 0.1f; }       // Czerwony
        else if (colorID == 1) { sim1.paintR = 0.1f; sim1.paintG = 0.8f; sim1.paintB = 0.1f; }  // Zielony
        else if (colorID == 2) { sim1.paintR = 0.1f; sim1.paintG = 0.4f; sim1.paintB = 0.9f; }  // Niebieski
        else { sim1.paintR = 0.9f; sim1.paintG = 0.8f; sim1.paintB = 0.0f; }                    // Żółty

        productCounter++;
    }
}

// Konfiguruje system oświetlenia OpenGL
// LIGHT0 - główne światło sufitowe
// LIGHT1-3 - reflektory lamp
void SetupLighting(void) {
    if (!lighting.enableLighting) {
        glDisable(GL_LIGHTING);
        return;
    }

    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);  
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

    // Materiał domyślny
    {
        GLfloat matSpec[] = { 0.8f, 0.8f, 0.8f, 1.0f };
        GLfloat matShine = 48.0f;
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, matShine);
    }

    // Poziom światła ambient zależny od trybu
    float ambientLevel = isNightMode ? 0.25f : 0.45f;
    float mainLightLevel = isNightMode ? 0.25f : 0.85f;

    GLfloat globalAmbient[] = { ambientLevel, ambientLevel, ambientLevel, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    // LIGHT0 - główne światło punktowe (sufit)
    glEnable(GL_LIGHT0);
    {
        GLfloat diff[] = { mainLightLevel * 0.9f, mainLightLevel * 0.9f, mainLightLevel * 0.95f, 1.0f };
        GLfloat spec[] = { 0.9f, 0.9f, 0.95f, 1.0f };
        GLfloat pos[] = { 0.0f, 20.0f, 0.0f, 1.0f };  // Pozycja punktowa (w=1)

        glLightfv(GL_LIGHT0, GL_DIFFUSE, diff);
        glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
        glLightfv(GL_LIGHT0, GL_POSITION, pos);

        // Atenuacja (zanikanie z odległością)
        glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.005f);
        glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.0005f);
    }

    // Reflektory lamp sufitowych
    if (lighting.enableSpotlight) {
        // W trybie nocnym reflektory są intensywniejsze
        float spotIntensity = isNightMode ? 3.0f : 1.0f;
        float specIntensity = isNightMode ? 2.2f : 1.0f;

        // Ciepłe kolory dla trybu nocnego
        GLfloat nightSpotBase[] = { 1.2f, 0.7f, 0.35f, 1.0f };
        GLfloat nightSpecBase[] = { 1.0f, 0.7f, 0.4f, 1.0f };
        GLfloat daySpotBase[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        GLfloat daySpecBase[] = { 1.0f, 1.0f, 1.0f, 1.0f };

        GLfloat spotBase[4];
        GLfloat specBase[4];

        if (isNightMode) {
            for (int i = 0; i < 4; ++i) { spotBase[i] = nightSpotBase[i]; specBase[i] = nightSpecBase[i]; }
        }
        else {
            for (int i = 0; i < 4; ++i) { spotBase[i] = daySpotBase[i]; specBase[i] = daySpecBase[i]; }
        }

        GLfloat warmSpotLight[4] = {
            spotBase[0] * spotIntensity,
            spotBase[1] * spotIntensity,
            spotBase[2] * spotIntensity,
            1.0f
        };
        GLfloat warmSpecular[4] = {
            specBase[0] * specIntensity,
            specBase[1] * specIntensity,
            specBase[2] * specIntensity,
            1.0f
        };

        // silniejsza w nocy aby ograniczyć zasięg
        float constAtt = isNightMode ? 0.25f : 0.6f;
        float linAtt = isNightMode ? 0.008f : 0.015f;
        float quadAtt = isNightMode ? 0.0008f : 0.003f;

        // Pozycje lamp
        const float lamp1Pos[3] = { -5.0f, 18.0f, 7.0f };
        const float lamp2Pos[3] = { 5.0f, 18.0f, 7.0f };
        const float lamp3Pos[3] = { 0.0f, 18.0f, 0.0f };
        GLfloat dirDown[] = { 0.0f, -1.0f, 0.0f };

        // LIGHT1 - lewa lampa
        glEnable(GL_LIGHT1);
        {
            GLfloat pos1[] = { lamp1Pos[0], lamp1Pos[1], lamp1Pos[2], 1.0f };

            glLightfv(GL_LIGHT1, GL_DIFFUSE, warmSpotLight);
            glLightfv(GL_LIGHT1, GL_SPECULAR, warmSpecular);
            glLightfv(GL_LIGHT1, GL_POSITION, pos1);
            glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, dirDown);

            glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, (GLfloat)(isNightMode ? 18.0f : 35.0f));
            glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, (GLfloat)(isNightMode ? 40.0f : 12.0f));

            glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, (GLfloat)constAtt);
            glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, (GLfloat)linAtt);
            glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, (GLfloat)quadAtt);
        }

        // LIGHT2 - prawa lampa
        glEnable(GL_LIGHT2);
        {
            GLfloat pos2[] = { lamp2Pos[0], lamp2Pos[1], lamp2Pos[2], 1.0f };

            glLightfv(GL_LIGHT2, GL_DIFFUSE, warmSpotLight);
            glLightfv(GL_LIGHT2, GL_SPECULAR, warmSpecular);
            glLightfv(GL_LIGHT2, GL_POSITION, pos2);
            glLightfv(GL_LIGHT2, GL_SPOT_DIRECTION, dirDown);

            glLightf(GL_LIGHT2, GL_SPOT_CUTOFF, (GLfloat)(isNightMode ? 18.0f : 35.0f));
            glLightf(GL_LIGHT2, GL_SPOT_EXPONENT, (GLfloat)(isNightMode ? 40.0f : 12.0f));

            glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, (GLfloat)constAtt);
            glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, (GLfloat)linAtt);
            glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, (GLfloat)quadAtt);
        }

        // LIGHT3 - środkowa lampa
        glEnable(GL_LIGHT3);
        {
            GLfloat pos3[] = { lamp3Pos[0], lamp3Pos[1], lamp3Pos[2], 1.0f };

            glLightfv(GL_LIGHT3, GL_DIFFUSE, warmSpotLight);
            glLightfv(GL_LIGHT3, GL_SPECULAR, warmSpecular);
            glLightfv(GL_LIGHT3, GL_POSITION, pos3);
            glLightfv(GL_LIGHT3, GL_SPOT_DIRECTION, dirDown);

            glLightf(GL_LIGHT3, GL_SPOT_CUTOFF, (GLfloat)(isNightMode ? 20.0f : 36.0f));
            glLightf(GL_LIGHT3, GL_SPOT_EXPONENT, (GLfloat)(isNightMode ? 30.0f : 10.0f));

            glLightf(GL_LIGHT3, GL_CONSTANT_ATTENUATION, (GLfloat)constAtt);
            glLightf(GL_LIGHT3, GL_LINEAR_ATTENUATION, (GLfloat)linAtt);
            glLightf(GL_LIGHT3, GL_QUADRATIC_ATTENUATION, (GLfloat)quadAtt);
        }
    }
    else {
        glDisable(GL_LIGHT1);
        glDisable(GL_LIGHT2);
        glDisable(GL_LIGHT3);
    }
}



// Inicjalizacja kontekstu renderingu - wywoływana raz przy starcie
void SetupRC(void) {
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);

    InitParticles();
    SetupLighting();

    // Ładowanie tekstur z plików BMP
    textureFloor = LoadTextureFromBMP("floor.bmp", GL_REPEAT, GL_LINEAR);
    textureConveyor = LoadTextureFromBMP("conveyor.bmp", GL_REPEAT, GL_LINEAR);
    textureWood = LoadTextureFromBMP("drewno.bmp", GL_REPEAT, GL_LINEAR);
    texturePolki = LoadTextureFromBMP("polki.bmp", GL_REPEAT, GL_LINEAR);
    if (!texturePolki) texturePolki = textureWood;  // Fallback

    textureDoor = LoadTextureFromBMP("drzwi.bmp", GL_REPEAT, GL_LINEAR);
    textureCeiling = LoadTextureFromBMP("sufit.bmp", GL_REPEAT, GL_LINEAR);
    textureocynkowanyMetal = LoadTextureFromBMP("ocynkowany_metal.bmp", GL_REPEAT, GL_LINEAR);
    textureMetal = LoadTextureFromBMP("metal.bmp", GL_REPEAT, GL_LINEAR);
    textureCrate = LoadTextureFromBMP("skrzynka.bmp", GL_REPEAT, GL_LINEAR);

    // Losowanie pierwszego produktu
    srand((unsigned int)GetTickCount64());
    sim1.productType = rand() % 3;
    int colorID = rand() % 4;
    if (colorID == 0) { sim1.paintR = 0.8f; sim1.paintG = 0.1f; sim1.paintB = 0.1f; }
    else if (colorID == 1) { sim1.paintR = 0.1f; sim1.paintG = 0.8f; sim1.paintB = 0.1f; }
    else if (colorID == 2) { sim1.paintR = 0.1f; sim1.paintG = 0.4f; sim1.paintB = 0.9f; }
    else { sim1.paintR = 0.9f; sim1.paintG = 0.8f; sim1.paintB = 0.0f; }
}

// Zwalnia zasoby OpenGL przy zamykaniu aplikacji
void CleanupResources(void) {
    if (textureConveyor) { glDeleteTextures(1, &textureConveyor); textureConveyor = 0; }
    if (textureFloor) { glDeleteTextures(1, &textureFloor); textureFloor = 0; }
    if (textureWood) { glDeleteTextures(1, &textureWood); textureWood = 0; }
    if (textureMetal) { glDeleteTextures(1, &textureMetal); textureMetal = 0; }
    if (textureCrate) { glDeleteTextures(1, &textureCrate); textureCrate = 0; }
    if (texturePolki) { glDeleteTextures(1, &texturePolki); texturePolki = 0; }
    if (textureocynkowanyMetal) { glDeleteTextures(1, &textureocynkowanyMetal); textureocynkowanyMetal = 0; }
    if (textureDoor) { glDeleteTextures(1, &textureDoor); textureDoor = 0; }
    if (textureCeiling) { glDeleteTextures(1, &textureCeiling); textureCeiling = 0; }
}

// Obsługuje zmianę rozmiaru okna
void ChangeSize(int width, int height) {
    if (height == 0) height = 1; 
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)width / (double)height, 1.0, 400.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void SetDCPixelFormat(HDC hDC) {
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        32,  
        0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0
    };

    int nPixelFormat = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, nPixelFormat, &pfd);
}

// Procedura okna - obsługa komunikatów Windows
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    static HGLRC hRC = NULL;

    switch (message) {
    case WM_CREATE:
        hDC = GetDC(hWnd);
        SetDCPixelFormat(hDC);
        hRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hRC);
        SetupRC();
        BuildFont();
        SetTimer(hWnd, 1, TIMER_INTERVAL, NULL);  
        break;

    case WM_DESTROY:
        KillTimer(hWnd, 1);
        KillFont();
        CleanupResources();
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hRC);
        ReleaseDC(hWnd, hDC);
        PostQuitMessage(0);
        break;

    case WM_SIZE:
        ChangeSize(LOWORD(lParam), HIWORD(lParam));
        break;

    case WM_TIMER:
        UpdateSimulation();
        InvalidateRect(hWnd, NULL, FALSE);
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        RenderScene();
        SwapBuffers(hDC);  
        EndPaint(hWnd, &ps);
    }
    break;

    case WM_LBUTTONDOWN:
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        mouseX = LOWORD(lParam);
        mouseY = rc.bottom - HIWORD(lParam);  
        mouseLeftDown = 1;
        if (mouseY <= hudHeight)
            uiCapturedMouse = 1;
        else
            uiCapturedMouse = 0;
    }
    break;

    case WM_LBUTTONUP:
        mouseLeftDown = 0;
        if (!mouseRightDown) ReleaseCapture();
        break;

    case WM_RBUTTONDOWN:
    {
        int rawX = LOWORD(lParam);
        int rawY = HIWORD(lParam);
        camera.lastMouseX = rawX;
        camera.lastMouseY = rawY;
        mouseRightDown = 1;
        SetCapture(hWnd); 
    }
    break;

    case WM_RBUTTONUP:
        mouseRightDown = 0;
        if (!mouseLeftDown) ReleaseCapture();
        break;

    case WM_MOUSEMOVE:
    {
        RECT rc;
        GetClientRect(hWnd, &rc);
        int rawX = LOWORD(lParam);
        int rawY = HIWORD(lParam);
        mouseX = rawX;
        mouseY = rc.bottom - rawY;

        uiCapturedMouse = (mouseY <= hudHeight) ? 1 : 0;

        if (mouseRightDown) {
            float sensitivity = 0.3f;
            int dx = rawX - camera.lastMouseX;
            int dy = rawY - camera.lastMouseY;

            camera.yaw += dx * sensitivity;
            camera.pitch -= dy * sensitivity;

            if (camera.pitch > 89.0f) camera.pitch = 89.0f;
            if (camera.pitch < -89.0f) camera.pitch = -89.0f;

            camera.lastMouseX = rawX;
            camera.lastMouseY = rawY;
        }
        else {
            camera.lastMouseX = rawX;
            camera.lastMouseY = rawY;
        }
    }
    break;

    case WM_MOUSEWHEEL:
    {
        short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        float scrollSpeed = 0.05f;
        float moveAmount = (float)zDelta * scrollSpeed;

        float radYaw = camera.yaw * DEG2RAD;
        float radPitch = camera.pitch * DEG2RAD;

        float frontX = cosf(radYaw) * cosf(radPitch);
        float frontY = sinf(radPitch);
        float frontZ = sinf(radYaw) * cosf(radPitch);

        camera.x += frontX * moveAmount;
        camera.y += frontY * moveAmount;
        camera.z += frontZ * moveAmount;

        if (camera.y < 1.5f) camera.y = 1.5f;
        InvalidateRect(hWnd, NULL, FALSE);
    }
    break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance_, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wc = { 0 };
    MSG msg;
    RECT rc = { 0, 0, 1280, 720 };

    hInstance = hInstance_;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = lpszAppName;

    if (!RegisterClassEx(&wc)) return 0;

    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindowEx(
        0,
        lpszAppName,
        lpszAppName,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL);

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}